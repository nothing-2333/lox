#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// 对象类型在c中的实现
typedef struct Obj Obj;
typedef struct ObjString ObjString;

// 值的类型
typedef enum
{
  VAL_BOOL,
  VAL_NIL, 
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

// 常量类型
typedef struct 
{
    ValueType type;
    union 
    {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// 判断
#define IS_BOOL(value)          ((value).type == VAL_BOOL)
#define IS_NIL(value)           ((value).type == VAL_NIL)
#define IS_NUMBER(value)        ((value).type == VAL_NUMBER)
#define IS_OBJ(value)           ((value).type == VAL_OBJ)

// 转换
#define AS_OBJ(value)           ((value).as.obj)
#define AS_BOOL(value)          ((value).as.boolean)
#define AS_NUMBER(value)        ((value).as.number)

// 快速定义
#define BOOL_VAL(value)         ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL                 ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)       ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)         ((Value){VAL_OBJ, {.obj = (Obj*)object}})

// 常量池，动态数组
typedef struct
{
    int capacity;
    int count;
    Value* values;
} ValueArray;

// 值的比较
bool valuesEqual(Value a, Value b);

// 常量池动态数组的分配
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

// 打印一个常量
void printValue(Value value);

#endif