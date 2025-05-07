cmake_minimum_required(VERSION 3.6)
project(HPC_lab04_smid)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SEG_SRC src/main.c src/k-means.c src/image.c)
set(WEIRD_SRC src/weirdmain.c src/weird.c src/image.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g -Wall -mavx2")

include_directories(../lib/stb include/)

add_executable(segmentation ${SEG_SRC})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -Wall")
add_executable(weirdimg ${WEIRD_SRC})

target_link_libraries(segmentation m)
target_link_libraries(weirdimg m)
