#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

// 释放单个对象
static void freeObject(Obj* object)
{
    switch (object->type)
    {
        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_STRING:
        {
            ObjString* string = (ObjString*)object; // 多态
            FREE_APPLY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_NATIVE:
        {
            FREE(ObjNative, object);
            break;
        }
        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_APPLY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);  // 只释放指针，值留着取悦即将到来的GC神灵
            FREE(ObjClosure, object);   // 不释放闭包中的函数内存，因为闭包不拥有函数
            break;
        }
        case OBJ_UPVALUE:
        {
            FREE(ObjUpvalue, object);
            break;
        }
    }
}

void freeObjects()
{
    Obj* object = vm.objects;
    while (object != NULL)
    {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}