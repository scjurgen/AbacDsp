cmake_minimum_required(VERSION 3.15)

project(PerformanceSuite)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

include_directories(../../src/includes)

# pffft from the submodule, needed by the phase-vocoder pitch shifter (BlockProc::Pitch's
# phase-vocoder engine, via Spectral/PhaseVocoderPitcher.h). Not header-only: pffft.c must
# be compiled in too.
set(PFFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/pffft")
include_directories(${PFFFT_DIR})

add_executable(
    PerformanceSuite
    PerformanceSuite.cpp
    ../../src/includes/Wavetables/WaveTableStorage.cpp
    ${PFFFT_DIR}/pffft.c
    ${PFFFT_DIR}/pffft_common.c
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Building for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
