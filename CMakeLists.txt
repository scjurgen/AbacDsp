cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "AbacDsp")
file(STRINGS VERSION CURRENT_VERSION)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wno-dev")
#set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

# Color our warnings and errors
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif ()

include_directories(src/includes)
include_directories(3rdparty/AudioFile)
add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(3rdparty)
option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    enable_testing()

    add_subdirectory(test)
endif()


