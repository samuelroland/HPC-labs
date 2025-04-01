cmake_minimum_required(VERSION 3.10)
project(lab02 C)

# Définir les options de compilation
set(CMAKE_C_STANDARD 11)
# set(CMAKE_C_FLAGS "-O0 -g -Wall -fno-inline -DLIKWID_PERFMON")
set(CMAKE_C_FLAGS "-O2 -Wall -DLIKWID_PERFMON")
set(DEBUG_FLAGS "-std=c11 -Wall -Wextra -pedantic -g")

# Inclure les répertoires d'en-têtes
include_directories(../../lib/stb/ ../include)

find_package(FFTW3 REQUIRED)
find_path(SNDFILE_INCLUDE_DIR sndfile.h REQUIRED)
find_library(SNDFILE_LIBRARY sndfile REQUIRED)

if(NOT SNDFILE_INCLUDE_DIR OR NOT SNDFILE_LIBRARY)
    message(FATAL_ERROR "libsndfile not found")
endif()

# First target with buffers variant
add_executable(dtmf_encdec_buffers main.c encoder.c decoder.c audio.c file.c fft.c)
target_compile_definitions(dtmf_encdec_buffers PRIVATE DECODER_VARIANT=1)
target_include_directories(dtmf_encdec_buffers PRIVATE 
    ${SNDFILE_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries(dtmf_encdec_buffers PRIVATE 
    ${SNDFILE_LIBRARY}
    fftw3
    m
    likwid
)

# Second target for fft variant
add_executable(dtmf_encdec_fft main.c encoder.c decoder.c audio.c file.c fft.c)
target_compile_definitions(dtmf_encdec_fft PRIVATE DECODER_VARIANT=2)
target_include_directories(dtmf_encdec_fft PRIVATE 
    ${SNDFILE_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries(dtmf_encdec_fft PRIVATE 
    ${SNDFILE_LIBRARY}
    fftw3
    m
    likwid
)

# Enable all colors
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure a Debug and Release flag for CMake
if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
  add_compile_options(-g3 -fPIE -pie)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  add_definitions(-DNDEBUG)
  add_compile_options(-O3 -Werror)
endif()
