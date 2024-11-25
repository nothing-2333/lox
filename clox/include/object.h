#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

// 获取对象类型具体是哪个：字符串、实例、函数...
#define OBJ_TYPE(value)             (AS_OBJ(value)->type)

// 判断
#define IS_BOUND_METHOD(value)      isObjType(value, OBJ_BOUND_METHOD)
#define IS_INSTANCE(value)          isObjType(value, OBJ_INSTANCE)
#define IS_CLASS(value)             isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)           isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)          isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)            isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)            isObjType(value, OBJ_STRING)

// 转换
#define AS_BOUND_METHOD(value)      ((ObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value)          ((ObjInstance*)AS_OBJ(value))
#define AS_CLASS(value)             ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)           ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)          ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)            (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)            ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)           (((ObjString*)AS_OBJ(value))->chars)

// 对象类型包含的类型
typedef enum
{
    OBJ_BOUND_METHOD,
    OBJ_NATIVE,
    OBJ_INSTANCE,
    OBJ_CLASS,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

// 各个类型的实现
struct Obj
{
    ObjType type;
    bool isMarked;      // 标记
    struct Obj* next;   // 用于GC
};

// 上值的运行时表示，将闭包在栈上剔除掉值保存在堆中
typedef struct ObjUpvalue
{
    Obj obj;
    Value* location;    // 栈上变量地址
    Value closed;       // 存储值
    struct ObjUpvalue* next;
} ObjUpvalue;

struct ObjString    
{
    Obj obj;            // 实现多态
    int length;         // 字符串
    char* chars;
    uint32_t hash;      // 字符串hash
};

typedef struct
{
    Obj obj;            // 多态
    int arity;          // 参数数量
    int upvalueCount;   // 上值数
    Chunk chunk;        // 字节码
    ObjString* name;    // 函数名
} ObjFunction;

typedef struct          // 将独立的函数作用域串起来
{
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;  // 每个函数都要拖着自己的上值前行emmm
    int upvalueCount;
} ObjClosure;

// 类
typedef struct 
{
    Obj obj;
    ObjString* name;
    Table methods;          // 虚表
} ObjClass;

// 实例
typedef struct 
{
    Obj obj;
    ObjClass* klass;
    Table fields;           // 属性
} ObjInstance;

// 绑定了实例的方法对象
typedef struct 
{
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

// 本地函数
typedef Value (*NativeFn)(int argCount, Value* args);
typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

// 开辟
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);

// 开辟一个类的空间
ObjClass* newClass(ObjString* name);

// 实例
ObjInstance* newInstance(ObjClass* Klass);

// 创建函数闭包
ObjClosure* newClosure(ObjFunction* function);

// new一个新函数
ObjFunction* newFunction();

// 为本地函数开辟内存
ObjNative* newNative(NativeFn function);

// 将代码中的c字符串转换成ObjString-不分配内存版
ObjString* takeString(char* chars, int length);

// 将代码中的c字符串转换成ObjString
ObjString* copyString(const char* chars, int length);

// 新建一个上值对象
ObjUpvalue* newUpvalue(Value* slot);

// 反汇编打印一个对象
void printObject(Value value);

// 判断是否是字符串对象
static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif