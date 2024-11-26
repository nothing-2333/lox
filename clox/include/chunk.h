#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// 指令定义
typedef enum
{
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_METHOD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_INVOKE,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_SUPER_INVOKE,
} OpCode;

// 代码
typedef struct 
{
    int count;              // 当前大小
    int capacity;           // 动态数组容量
    uint8_t* code;          // 代码的字节码
    int* lines;             // 代码对应的行数
    ValueArray constants;   // 常量池
} Chunk;

// 初始化代码
void initChunk(Chunk* chunk);
// 写一个字节
void writeChunk(Chunk* chunk, uint8_t byte, int line);
// 释放代码
void freeChunk(Chunk* chunk);
// 向字节码块中的常量池添加常量，返回常量索引
int addConstant(Chunk* chunk, Value value);

#endif