#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

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
static ObjString* allocateString(char* chars, int length)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* takeString(char* chars, int length)
{
    return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);   // 复制字符串而不是用原有的是因为字符串可以添加字符
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}