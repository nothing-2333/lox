#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// 指令定义
typedef enum
{
    OP_CONSTANT,
    OP_RETURN,
} OpCode;

// 代码
typedef struct 
{
    int count;
    int capacity;
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