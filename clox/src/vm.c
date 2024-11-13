#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

VM vm;  // 虚拟机是个全局变量

// 初始化虚拟机的栈内存
static void resetStack()
{
    vm.stackTop = vm.stack;
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

// 指令分发函数-主函数
static InterpretResult run()
{
    #define READ_BYTE() (*vm.ip++)  // vm 写成一个全局变量真的有点难受
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])    // 宏像不像 eval ？
    #define BINAPY_OP(op) \
        do  \
        {   \
            double b = pop();   \
            double a = pop();   \
            push(a op b);   \
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
        case OP_ADD:            BINAPY_OP(+); break;
        case OP_SUBTRACT:       BINAPY_OP(-); break;
        case OP_MULTIPLY:       BINAPY_OP(*); break;
        case OP_DIVIDE:         BINAPY_OP(/); break;
        case OP_NEGATE:         push(-pop()); break;
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
    compile(source);
    return INTERPRET_OK;
}