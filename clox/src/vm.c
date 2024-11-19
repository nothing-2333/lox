#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include "memory.h"

VM vm;  // 虚拟机是个全局变量

// 重置虚拟机的栈内存
static void resetStack()
{
    vm.stackTop = vm.stack;
}

// 运行时错误
static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void initVM()
{
    freeTable(&vm.globals);
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
}
 
void freeVM()
{
    freeTable(&vm.strings);
    freeObjects(); 
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}

// 查看栈中的值
static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}

// 取反
static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// 字符串连接
static void concatenate()
{
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

// 指令分发函数-主函数
static InterpretResult run()
{
    #define READ_BYTE() (*vm.ip++)  // vm 写成一个全局变量真的有点难受
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])    // 宏像不像 eval ？
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    #define BINAPY_OP(valueType, op) \
        do  \
        {   \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))  \
            {   \
                runtimeError("Operands must be numbers.");  \
                return INTERPRET_RUNTIME_ERROR; \
            }   \
            double b = AS_NUMBER(pop());    \
            double a = AS_NUMBER(pop());    \
            push(valueType(a op b));    \
        } while (false)

    for (;;)
    {
        #ifdef DEBUG_TRACE_EXECUTION    // 反汇编
            disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
            printf("          ");   // 打印栈内容
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
            {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_NIL:            push(NIL_VAL); break;
        case OP_TRUE:           push(BOOL_VAL(true)); break;
        case OP_FALSE:          push(BOOL_VAL(false)); break;
        case OP_POP:            pop(); break;
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            push(vm.stack[slot]);   // 让vm的栈与编译器的局部变量数组所用重合
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            vm.stack[slot] = peek(0);  // 让vm的栈与编译器的局部变量数组所用重合
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjString* name = READ_STRING();
            Value value;
            if (!tableGet(&vm.globals, name, &value))
            {
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_GLOBAL:
        {
            ObjString* name = READ_STRING();
            tableSet(&vm.globals, name, peek(0));
            /*请注意，直到将值添加到哈希表之后，我们才会弹出它。这确保了如果在将值添加到哈希表的过程中触发了垃圾回收，
            虚拟机仍然可以找到这个值。这显然是很可能的，因为哈希表在调整大小时需要动态分配。*/
            pop(); 
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjString* name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0))) 
            {
                tableDelete(&vm.globals, name); // 如果是第一次设置那就完蛋了
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;  // 赋值表达式不用pop
        }
        case OP_EQUAL:
        {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:        BINAPY_OP(BOOL_VAL, >); break;
        case OP_LESS:           BINAPY_OP(BOOL_VAL, <); break;
        case OP_ADD:
        {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
            {
                concatenate();
            }
            else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop()); 
                push(NUMBER_VAL(a + b));
            }
            else
            {
                runtimeError("Operands must be two numbers or two strings."); 
                return INTERPRET_RUNTIME_ERROR; 
            }
            break;
        }
        case OP_SUBTRACT:       BINAPY_OP(NUMBER_VAL, -); break;
        case OP_MULTIPLY:       BINAPY_OP(NUMBER_VAL, *); break;
        case OP_DIVIDE:         BINAPY_OP(NUMBER_VAL, /); break;
        case OP_NOT:            push(BOOL_VAL(isFalsey(pop()))); break;
        case OP_NEGATE:
        {
            if (!IS_NUMBER(peek(0)))
            {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        }
        case OP_PRINT:
        {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_RETURN:
        {
            return INTERPRET_OK;
        }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk))
    {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}