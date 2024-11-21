cmake_minimum_required(VERSION 3.18)

project(clox)

# 添加头文件搜索路径
include_directories(include)

# 设置调试信息
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

# 设置可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 添加子目录
add_subdirectory(src)