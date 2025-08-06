cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
#cmake_policy(SET CMP0174 NEW)

include_directories("${PROJECT_SOURCE_DIR}/src/includes")

add_executable(SamplerateConverter CheckSinc.cpp
        ${PROJECT_SOURCE_DIR}/src/includes/Samplerate/SrConverter.cpp
)
