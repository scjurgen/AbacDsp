cmake_minimum_required(VERSION 3.15)

project(BandpassImpulseExplore)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")

    include_directories(../../../src/includes)

    find_package(Threads REQUIRED)
endif ()

add_executable(VelvetNoise VelvetNoise.cpp)


#target_link_libraries(BandpassImpulseExplore Threads::Threads)


if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

message(STATUS "Building for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
