#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// 存储的key-value
typedef struct
{
    ObjString* key;
    Value value;
} Entry;

// hash表-动态数组
typedef struct
{
    int count;
    int capacity;
    Entry* entries;
} Table;

// 初始化
void initTable(Table* table);
// 释放内存
void freeTable(Table* table);
// 设置
bool tableSet(Table* table, ObjString* key, Value value);
// 获取
bool tableGet(Table* table, ObjString* key, Value* value);
// 删除
bool tableDelete(Table* table, ObjString* key);
// 将一个hash表的内容拷贝到另一个hash表
void tableAddAll(Table* from, Table* to);
// 严格查找，key的每一个字节都要一样
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

// 标记表中对象
void markTable(Table* table); 
// 清除即将被删除的字符串
void tableRemoveWhite(Table* table);

#endif