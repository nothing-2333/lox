#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

// 动态数组扩容
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)
#define GROW_APPLY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))
// 动态数组释放内存
#define FREE_APPLY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// 分配内存实现函数，所有内存操作都经过此函数，方便 gc
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif