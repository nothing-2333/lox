#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "value.h"
#include "table.h"

#define FRAMES_MAX          64
#define STACK_MAX           (FRAMES_MAX * UINT8_COUNT)

// 栈帧
typedef struct 
{
  ObjClosure* closure;      // 函数的闭包
  uint8_t* ip;              // 函数执行的位置
  Value* slots;             // 函数可以使用栈开始位置
} CallFrame;

// 虚拟机，喜欢吗
typedef struct 
{
    CallFrame frames[FRAMES_MAX];   // 存储一个一个栈帧
    int frameCount;

    Value stack[STACK_MAX];         // 栈式虚拟机，最重要的当然是栈
    Value* stackTop;                // 下一个空闲空间
    Table globals;                  // 全局作用域表
    Table strings;                  // hash表中驻留的字符串-集合
    ObjUpvalue* openUpvalues;       // 指向上值的堆地址的指针列表头
    Obj* objects;                   // 指向申请内存的对象

    int grayCount;                  // 三色抽象灰色工厂
    int grayCapacity;
    Obj** grayStack;

    size_t bytesAllocated;          // GC触发机制
    size_t nextGC;
} VM;

// 虚拟机执行过程结果
typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

// vm公开到外部
extern VM vm;

// 初始化虚拟机的内存
void initVM();
// 释放虚拟机的内存
void freeVM();

// 开始执行吧，宝贝（呕）
InterpretResult interpret(const char* source);

// 入栈、出栈
void push(Value value);
Value pop();

#endif