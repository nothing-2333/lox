#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

// 申请指定对象类型的内存
#define ALLOCATE_OBJ(type, objectType)  \
    (type*)allocateObject(sizeof(type), objectType)

// 所有对象申请内存都经过这个函数
static Obj* allocateObject(size_t size, ObjType type)
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm.objects;  // 插入头部
    vm.objects = object;

    #ifdef DEBUG_LOG_GC // 对象分配时打印
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
    #endif

    return object;
}

ObjClosure* newClosure(ObjFunction* function)
{
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);

    for (int i = 0; i < function->upvalueCount; ++i)
    {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjClass* newClass(ObjString* name)
{
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjInstance* newInstance(ObjClass* Klass)
{
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = Klass;
    initTable(&instance->fields);
    return instance;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method)
{
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

// 申请字符串类型的内存
static ObjString* allocateString(char* chars, int length, uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL); // 把hash表当集合用，要确保设置相同的字符串指向的地址一样
    pop();

    return string;
}

// 字符串哈希函数FNV-1a
static uint32_t hashString(const char* key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjFunction* newFunction()
{
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(NativeFn function)
{
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

ObjString* takeString(char* chars, int length)  // 字符串连接时使用
{
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);  // 如果拼接出的字符串在hash表中有就将其返回
    if (interned != NULL) 
    {
        FREE_APPLY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length)    // 从源代码到ObjString都要经过这个函数
{
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);    // 确保每个相同的字符串都是同一块内存
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);   // 复制字符串而不是用原有的是因为字符串可以添加字符
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(Value* slot)
{
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

// 打印一个函数
static void printFunction(ObjFunction* function)
{
    if (function->name == NULL)
    {
        printf("<script>");
        return;
    }
    
    printf("<fn %s>", function->name->chars);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
        case OBJ_BOUND_METHOD:  printFunction(AS_BOUND_METHOD(value)->method->function); break;
        case OBJ_INSTANCE:      printf("%s instance", AS_INSTANCE(value)->klass->name->chars); break;
        case OBJ_CLOSURE:       printFunction(AS_CLOSURE(value)->function); break;
        case OBJ_FUNCTION:      printFunction(AS_FUNCTION(value)); break;
        case OBJ_STRING:        printf("%s", AS_CSTRING(value)); break;
        case OBJ_NATIVE:        printf("<native fn>"); break;
        case OBJ_UPVALUE:       printf("upvalue"); break;
        case OBJ_CLASS:         printf("%s", AS_CLASS(value)->name->chars); break;
    }
}