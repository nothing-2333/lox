#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

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
    resetStack();
}
 
void freeVM()
{

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

// 指令分发函数-主函数
static InterpretResult run()
{
    #define READ_BYTE() (*vm.ip++)  // vm 写成一个全局变量真的有点难受
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])    // 宏像不像 eval ？
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
        case OP_EQUAL:
        {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:        BINAPY_OP(BOOL_VAL, >); break;
        case OP_LESS:           BINAPY_OP(BOOL_VAL, <); break;
        case OP_ADD:            BINAPY_OP(NUMBER_VAL, +); break;
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
        case OP_RETURN:
        {
            printValue(pop());
            printf("\n");
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