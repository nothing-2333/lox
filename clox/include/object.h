#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

// 获取对象类型具体是哪个：字符串、实例、函数...
#define OBJ_TYPE(value)             (AS_OBJ(value)->type)

// 判断
#define IS_FUNCTION(value)          isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)            isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)            isObjType(value, OBJ_STRING)

// 转换
#define AS_FUNCTION(value)          ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)            (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)            ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)           (((ObjString*)AS_OBJ(value))->chars)

// 对象类型包含的类型
typedef enum
{
    OBJ_NATIVE,
    OBJ_FUNCTION,
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
    uint32_t hash;
};
typedef struct
{
    Obj obj;    // 多态
    int arity;  // 参数数量
    Chunk chunk;    // 字节码
    ObjString* name;    // 函数名
} ObjFunction;

// 本地函数
typedef Value (*NativeFn)(int argCount, Value* args);
typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;


// new一个新函数
ObjFunction* newFunction();

// 为本地函数开辟内存
ObjNative* newNative(NativeFn function);

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