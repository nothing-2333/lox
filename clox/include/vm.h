#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

// 虚拟机，喜欢吗
typedef struct 
{
    Chunk* chunk;
    uint8_t* ip; // ip 总是指向下一条指令，而不是当前正在处理的指令
    Value stack[STACK_MAX]; // 栈式虚拟机，最重要的当然是栈
    Value* stackTop;    // 下一个空闲空间
} VM;

// 虚拟机执行过程结果
typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

// 初始化虚拟机的内存
void initVM();
// 释放虚拟机的内存
void freeVM();

// 开始执行吧，宝贝（呕，好恶心）
InterpretResult interpret(Chunk* chunk);

// 入栈、出栈
void push(Value value);
Value pop();

#endif