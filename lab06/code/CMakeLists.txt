cmake_minimum_required(VERSION 3.12)
project(HPC_lab06_powercap)

add_subdirectory(powercap)

add_definitions(-DCODE_VERSION=1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -ggdb -Wall -fno-tree-vectorize")
add_executable(floatsparty_basic main.c)
target_link_libraries(floatsparty_basic PRIVATE powercap)

add_definitions(-DCODE_VERSION=2)
set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -O2 -g -ggdb -Wall -fno-tree-vectorize -mavx2")
add_executable(floatsparty_simd main.c)
target_link_libraries(floatsparty_simd PRIVATE powercap)
