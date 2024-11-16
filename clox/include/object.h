#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

// 获取对象类型具体是哪个：字符串、实例、函数...
#define OBJ_TYPE(value)             (AS_OBJ(value)->type)

// 判断
#define IS_STRING(value)            isObjType(value, OBJ_STRING)

// 转换
#define AS_STRING(value)            ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)           (((ObjString*)AS_OBJ(value))->chars)

// 对象类型包含的类型
typedef enum
{
    OBJ_STRING,
} ObjType;

// 各个类型的实现
struct Obj
{
    ObjType type;
    struct Obj* next;   // 用于GC
};
struct ObjString    // 实现多态
{
    Obj obj;
    int length;
    char* chars;
};

// 将代码中的c字符串转换成ObjString-不分配内存版
ObjString* takeString(char* chars, int length);

// 将代码中的c字符串转换成ObjString
ObjString* copyString(const char* chars, int length);

// 反汇编打印一个对象
void printObject(Value value);

// 判断是否是字符串对象
static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif