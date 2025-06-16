cmake_minimum_required(VERSION 3.10)
project(lab02 C)

# Définir les options de compilation
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3 -g -Wall -fopenmp")

# Inclure les répertoires d'en-têtes
include_directories(../../lib/stb/ ../include)

find_path(SNDFILE_INCLUDE_DIR sndfile.h REQUIRED)
find_library(SNDFILE_LIBRARY sndfile REQUIRED)

if(NOT SNDFILE_INCLUDE_DIR OR NOT SNDFILE_LIBRARY)
  message(FATAL_ERROR "libsndfile not found")
endif()

# First target with buffers variant
add_executable(dtmf_encdec_buffers main.c encoder.c decoder.c audio.c file.c)
target_compile_definitions(dtmf_encdec_buffers PRIVATE DECODER_VARIANT=1)
target_include_directories(dtmf_encdec_buffers PRIVATE ${SNDFILE_INCLUDE_DIR})
target_link_libraries(dtmf_encdec_buffers PRIVATE ${SNDFILE_LIBRARY} m)

# Enable all colors
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
