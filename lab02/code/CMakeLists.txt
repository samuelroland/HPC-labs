cmake_minimum_required(VERSION 3.10)
project(lab02 C)

# Définir les options de compilation
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O0 -g -Wall -fno-inline -DLIKWID_PERFMON")
set(DEBUG_FLAGS "-std=c11 -Wall -Wextra -pedantic -g")

# Inclure les répertoires d'en-têtes
include_directories(../../lib/stb/ ../include)

# Chercher tous les fichiers sources
file(GLOB SOURCES_test "src/test_likwid.c")

# Créer l'exécutable
add_executable(test_likwid ${SOURCES_test})

# Lier les bibliothèques
target_link_libraries(test_likwid m likwid)

# Cible pour le mode debug
add_custom_target(debug
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR} && ${CMAKE_BUILD_TOOL}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Spécifier les cibles comme PHONY
target_sources(test_likwid PRIVATE ${SOURCES_test})
