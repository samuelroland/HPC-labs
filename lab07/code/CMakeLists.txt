cmake_minimum_required(VERSION 3.10)
project(FindKmers)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g -Wall")
add_executable(k-mer src/main.c)
