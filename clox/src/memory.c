#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR     2

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    vm.bytesAllocated += newSize - oldSize;

    if (newSize > oldSize)      // 区分是GC调用还是扩容
    {
        #ifdef DEBUG_STRESS_GC  // 每次分配内存的时候强制GC一次
        collectGarbage();
        #endif

        if (vm.bytesAllocated > vm.nextGC)
        {
            collectGarbage();
        }
    }

    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void markObject(Obj* object)
{
    if (object == NULL) return;
    if (object->isMarked) return;

    #ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
    #endif

    object->isMarked = true;

    if (vm.grayCapacity < vm.grayCount + 1)
    {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);    // 这个可不能回收

        if (vm.grayStack == NULL) exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value)
{
    if (IS_OBJ(value)) markObject(AS_OBJ(value));   // 只管理堆上的Object
}

// 释放单个对象
static void freeObject(Obj* object)
{
    #ifdef DEBUG_LOG_GC // 释放对象时打印
    printf("%p free type %d\n", (void*)object, object->type);
    #endif

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

// 标记大根
static void markRoots()
{
    // 栈上的局部变量与临时变量
    for (Value* slot = vm.stack; slot < vm.stackTop; ++slot)
    {
        markValue(*slot);
    }

    // 函数封装成的闭包对象也要标记
    for (int i = 0; i < vm.frameCount; ++i)
    {
        markObject((Obj*)vm.frames[i].closure); 
    }

    // 当前所有的上值（感谢昨天勤劳的我）
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        markObject((Obj*)upvalue);
    }

    // hash表中的全局变量
    markTable(&vm.globals);

    // 编译期间用到的内存
    markCompilerRoots();
}

// 标记对象数组
static void markArray(ValueArray* array)
{
    for (int i = 0; i < array->count; ++i)
    {
        markValue(array->values[i]);
    }
}

// 跟踪不同的对象
static void blackenObject(Obj* object)
{
    #ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
    #endif

    switch (object->type)
    {
        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; ++i)
            {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

// 三色抽象
static void traceReferences()
{
    while (vm.grayCount > 0)
    {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
    
}

// 清理垃圾
static void sweep()
{
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL)
    {
        if (object->isMarked)
        {
            object->isMarked = false;   // 为下一轮做准备

            previous = object;
            object = object->next;
        }
        else
        {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
    
}

void collectGarbage()
{
    #ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
    #endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

    #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm.bytesAllocated, before, vm.bytesAllocated,
        vm.nextGC);
    #endif
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

    free(vm.grayStack);
}