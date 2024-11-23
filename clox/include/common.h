#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_PRINT_CODE        // 编译打印反汇编
#define DEBUG_TRACE_EXECUTION   // 运行打印反汇编

#define DEBUG_STRESS_GC         // GC的压力测试模式
#define DEBUG_LOG_GC            // 打印GC日志

#define UINT8_COUNT     (UINT8_MAX + 1) // 最大局部变量数

#endif