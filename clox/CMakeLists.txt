cmake_minimum_required(VERSION 3.18)

project(clox)

include_directories(include)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -g)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(src)