cmake_minimum_required(VERSION 3.6)
project(HPC_lab04_smid)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WEIRD_SRC src/main.c src/weird.c src/image.c)

include_directories(../lib/stb include/)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -Wall -mavx2")
add_executable(weirdimg ${WEIRD_SRC})

target_link_libraries(weirdimg m)
