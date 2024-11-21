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

    object->next = vm.objects;  // 插入头部
    vm.objects = object;

    return object;
}

// 申请字符串类型的内存
static ObjString* allocateString(char* chars, int length, uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NIL_VAL); // 把hash表当集合用，要确保设置相同的字符串指向的地址一样
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
        case OBJ_FUNCTION:      printFunction(AS_FUNCTION(value)); break;
        case OBJ_STRING:        printf("%s", AS_CSTRING(value)); break;
        case OBJ_NATIVE:        printf("<native fn>"); break;
    }
}