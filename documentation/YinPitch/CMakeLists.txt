cmake_minimum_required(VERSION 3.15)

project(YinPitchExplore)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

include_directories(../../src/includes)
include_directories(../../3rdparty/AudioFile)

add_executable(StampedYinExplore StampedYinExplore.cpp)
add_executable(StampedYinGuitarExplore StampedYinGuitarExplore.cpp)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Building for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
