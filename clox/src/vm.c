#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include "memory.h"

VM vm;  // 虚拟机是个全局变量

// 本地函数-时钟
static Value clockNative(int argCount, Value* args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// 重置虚拟机的栈内存
static void resetStack()
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

// 运行时错误
static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // 打印调用堆栈
    for (int i = vm.frameCount - 1; i >= 0; --i)
    {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

// 定义本地函数 
static void defineNative(const char* name, NativeFn function)
{
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM()
{
    freeTable(&vm.globals);
    resetStack();
    vm.objects = NULL;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    initTable(&vm.strings);

    defineNative("clock", clockNative);
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

// 函数调用
static bool call(ObjClosure* closure, int argCount)
{
    if (argCount != closure->function->arity)
    {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)    // 函数递归超出深度
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

// 调用
static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case OBJ_NATIVE:
            {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            case OBJ_CLOSURE:   // 闭包对象将代替函数执行
                return call(AS_CLOSURE(callee), argCount);
            default:
                break;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

// 创建上值，将堆地址分装成ObjUpvalue
static ObjUpvalue* captureUpvalue(Value* local)
{
    ObjUpvalue* preUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local)    // 构建有序列表，世上本没有路，走的人多了也就成了路
    {
        preUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)  // 两个ObjUpvalue不能存储同一个Value
    {
        return upvalue;
    }
    

    ObjUpvalue* createUpvalue = newUpvalue(local);
    createUpvalue->next = upvalue;

    if (preUpvalue == NULL)
    {
        vm.openUpvalues = createUpvalue;
    }
    else
    {
        preUpvalue->next = createUpvalue;
    }

    return createUpvalue;
}

// 传入栈槽的地址。该函数负责关闭上值，并将局部变量从栈中移动到堆上
static void closeUpvalues(Value* last)
{
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last)
    {
        ObjUpvalue* upvalue = vm.openUpvalues;

        upvalue->closed = *upvalue->location;   // 将上值独立出来
        upvalue->location = &upvalue->closed;

        vm.openUpvalues = upvalue->next;
    }
}

// 判断假值
static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// 字符串连接
static void concatenate()
{
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

// 指令执行-主函数
static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    #define READ_BYTE() (*frame->ip++)  // vm 写成一个全局变量真的有点难受
    #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])    // 宏像不像 eval ？
    #define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
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
            printf("          ");   // 打印栈内容
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
            {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
            push(frame->slots[slot]);   // 让vm的栈与编译器的局部变量数组所用重合
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);  // 让vm的栈与编译器的局部变量数组所用重合
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
        case OP_GET_UPVALUE:
        {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);    // 早就给你准备好了
            break;
        }
        case OP_SET_UPVALUE:
        {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
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
        case OP_JUMP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE:
        {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0))) frame->ip += offset;
            break;
        }
        case OP_PRINT:
        {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL:
        {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];  // 转到最新的栈帧
            break;
        }
        case OP_CLOSURE:
        {
            ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
            ObjClosure* closure = newClosure(function);
            push(OBJ_VAL(closure));
            /**
             * 静态编译时会留下闭包变量的信息，到这里动态执行时，储存每个函数的上值对应常量池的地址
             */
            for (int i = 0; i < closure->upvalueCount; ++i)
            {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal)
                {
                    closure->upvalues[i] = captureUpvalue(frame->slots + index);
                }
                else
                {
                    closure->upvalues[i] = frame->closure->upvalues[index]; // 由浅入深，上上上*值早已储存好了
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE:
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        case OP_RETURN:
        {
            Value result = pop();
            closeUpvalues(frame->slots);    // 当函数结束的时候，将这个函数用到的上值从常量池中独立出来
            vm.frameCount--;    // vm.frameCount是下一个未被使用的栈帧，--后是当前栈帧

            if (0 == vm.frameCount)
            {
                pop();
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots; // 回到调用当前函数最开始的栈顶
            push(result);
            frame = &vm.frames[vm.frameCount - 1];  // 上一个栈帧
            break;
        }
        }
    }

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);  // 设置第一个栈帧

    return run();
}