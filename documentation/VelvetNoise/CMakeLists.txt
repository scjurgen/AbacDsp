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

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        target_compile_options(PerformanceFdn PRIVATE -march=native -mtune=native)
    elseif (MSVC)
        target_compile_options(PerformanceFdn PRIVATE /arch:AVX2)
    endif ()
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        target_compile_options(PerformanceFdn PRIVATE -mcpu=native)
    elseif (MSVC)
        # MSVC ARM64 doesn't need specific flags for NEON (always available)
    endif ()
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

message(STATUS "Building for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
