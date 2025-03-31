cmake_minimum_required(VERSION 3.10)
project(lab02 C)

# Définir les options de compilation
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O0 -g -Wall -fno-inline -DLIKWID_PERFMON")
set(DEBUG_FLAGS "-std=c11 -Wall -Wextra -pedantic -g")

# Inclure les répertoires d'en-têtes
include_directories(../../lib/stb/ ../include)

# # Chercher tous les fichiers sources
# file(GLOB SOURCES_test "src/test_likwid.c")
#
# # Créer l'exécutable
# add_executable(test_likwid ${SOURCES_test})
#
# # Lier les bibliothèques
# target_link_libraries(test_likwid m likwid)

# Cible pour le mode debug
# add_custom_target(debug
#     COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR} && ${CMAKE_BUILD_TOOL}
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )
#
# # Spécifier les cibles comme PHONY
# target_sources(test_likwid PRIVATE ${SOURCES_test})

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
)
target_link_libraries(dtmf_encdec_buffers m likwid)

# Second target for fft variant
add_executable(dtmf_encdec_fft main.c encoder.c decoder.c audio.c file.c fft.c)
target_compile_definitions(dtmf_encdec_fft PRIVATE DECODER_VARIANT=1)
target_include_directories(dtmf_encdec_fft PRIVATE 
    ${SNDFILE_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)
target_link_libraries(dtmf_encdec_fft PRIVATE 
    ${SNDFILE_LIBRARY}
    fftw3
    m
)
target_link_libraries(dtmf_encdec_buffers m likwid)

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
