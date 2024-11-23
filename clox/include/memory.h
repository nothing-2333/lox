#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

// 申请一个对象
#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

// 释放一个对象
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

// 动态数组扩容
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)
#define GROW_APPLY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

// 动态数组释放内存
#define FREE_APPLY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// 分配内存实现函数，所有内存操作都经过此函数
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void markObject(Obj* Object);

// 标记一个变量
void markValue(Value value);

// GC
void collectGarbage();

// 释放堆上为对象申请的内存
void freeObjects();

#endif