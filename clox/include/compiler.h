#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"
#include "object.h"

// 编译
ObjFunction* compile(const char* source);

// 标记编译用到的对象
void markCompilerRoots();

#endif