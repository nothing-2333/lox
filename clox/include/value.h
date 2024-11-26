#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

// 对象类型在c中的实现
typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define QUAN                    ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT                ((uint64_t)0x8000000000000000)

#define TAG_NIL                 1
#define TAG_FALSE               2
#define TAG_TRUE                3

typedef uint64_t Value;

#define IS_NUMBER(value)        (((value) & QUAN) != QUAN)
#define IS_NIL(value)           ((value) == NIL_VAL)
#define IS_BOOL(value)          (((value) | 1) == TRUE_VAL)
#define IS_OBJ(value)           (((value) & (QUAN | SIGN_BIT)) == (QUAN | SIGN_BIT))

#define AS_NUMBER(value)        valueToNum(value)
#define AS_BOOL(value)          ((value) == TRUE_VAL)
#define AS_OBJ(value)           ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QUAN)))

#define NUMBER_VAL(num)         numToValue(num)
#define NIL_VAL                 ((Value)(uint64_t)(QUAN | TAG_NIL))
#define FALSE_VAL               ((Value)(uint64_t)(QUAN | TAG_FALSE))
#define TRUE_VAL                ((Value)(uint64_t)(QUAN | TAG_TRUE))
#define BOOL_VAL(b)             ((b) ? TRUE_VAL : FALSE_VAL)
#define OBJ_VAL(obj)            (Value)(SIGN_BIT | QUAN | (uint64_t)(uintptr_t)(obj))

static inline Value numToValue(double num)
{
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

static inline double valueToNum(Value value)
{
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

#else

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

#endif

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