cmake_minimum_required(VERSION 3.10)
project(DTMF C)

find_package(FFTW3 REQUIRED)
find_path(SNDFILE_INCLUDE_DIR sndfile.h REQUIRED)
find_library(SNDFILE_LIBRARY sndfile REQUIRED)

if(NOT SNDFILE_INCLUDE_DIR OR NOT SNDFILE_LIBRARY)
    message(FATAL_ERROR "libsndfile not found")
endif()

add_executable(dtmf_encdec main.c encoder.c decoder.c audio.c file.c fft.c)
target_include_directories(dtmf_encdec PRIVATE 
    ${SNDFILE_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries(dtmf_encdec PRIVATE 
    ${SNDFILE_LIBRARY}
    fftw3
    m
)
# target_compile_options(dtmf_encdec PRIVATE -Wall -Wextra -pedantic -g)

# Sandbox target with FFTW3
add_executable(sandbox sandbox.c encoder.c decoder.c audio.c file.c fft.c)
target_include_directories(sandbox PRIVATE 
    ${SNDFILE_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries(sandbox PRIVATE 
    ${SNDFILE_LIBRARY}
    fftw3
    m
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
