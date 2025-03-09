cmake_minimum_required(VERSION 3.10)

project(DTMF C)

find_path(SNDFILE_INCLUDE_DIR sndfile.h REQUIRED)
find_library(SNDFILE_LIBRARY sndfile REQUIRED)

if(NOT SNDFILE_INCLUDE_DIR OR NOT SNDFILE_LIBRARY)
    message(FATAL_ERROR "libsndfile non trouvé. Assurez-vous que libsndfile1 et libsndfile1-dev sont installés.")
endif()

add_executable(dtmf_encdec main.c codec.c audio.c file.c)
target_include_directories(dtmf_encdec PRIVATE ${SNDFILE_INCLUDE_DIR})
target_link_libraries(dtmf_encdec PRIVATE ${SNDFILE_LIBRARY} -lm)

target_compile_options(dtmf_encdec PRIVATE -Wall -Wextra -pedantic -g)

# Sandbox targets
add_executable(sandbox sandbox.c codec.c audio.c file.c)
target_include_directories(sandbox PRIVATE ${SNDFILE_INCLUDE_DIR})
target_link_libraries(sandbox PRIVATE ${SNDFILE_LIBRARY} -lm)

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
