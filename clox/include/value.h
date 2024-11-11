#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// 常量类型
typedef double Value;

// 常量池，动态数组
typedef struct
{
    int capacity;
    int count;
    Value* values;
} ValueArray;

// 常量池动态数组的分配
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

// 打印一个常量
void printValue(Value value);

#endif