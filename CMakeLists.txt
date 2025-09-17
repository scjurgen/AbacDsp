cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "AbacDsp")
file(STRINGS VERSION CURRENT_VERSION)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

# Color our warnings and errors
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif ()

# Option to build only DSP tests (for Docker/Valgrind)
option(BUILD_ONLY_TESTS "Build only tests without JUCE examples" OFF)
include_directories(3rdparty/AudioFile)

if (NOT BUILD_ONLY_TESTS)
    # Add JUCE first - this prevents duplicate target errors
    add_subdirectory(3rdparty/JUCE)

    include_directories(src/includes)
    add_executable(${PROJECT_NAME} main.cpp)

    # EXAMPLES - only when building full project
    add_subdirectory(examples/delay)
    add_subdirectory(examples/guisandbox)
    add_subdirectory(examples/juliasynth)
    add_subdirectory(examples/minireverb)
    add_subdirectory(examples/sampleplayer)
else ()
    # For tests-only builds, just include DSP headers
    include_directories(src/includes)
endif ()

# Add other 3rdparty dependencies (always needed for tests)
add_subdirectory(3rdparty/googletest EXCLUDE_FROM_ALL)

option(PACKAGE_TESTS "Build the tests" ON)
if (PACKAGE_TESTS)
    # Prevent CTest from creating dashboard targets but keep DartConfiguration.tcl
    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
    include(CTest)

    # Configure Valgrind for memory checking
    find_program(VALGRIND_PROGRAM NAMES valgrind)
    if (VALGRIND_PROGRAM)
        set(MEMORYCHECK_COMMAND ${VALGRIND_PROGRAM})
        set(MEMORYCHECK_COMMAND_OPTIONS
                "--leak-check=full --show-leak-kinds=all --fair-sched=yes --error-exitcode=1 --track-origins=yes --trace-children=yes")
        set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/valgrind.supp")
    endif ()

    add_subdirectory(test)
endif ()

option(PERFORMANCE_TESTS "Build performance tests" ON)
if (PERFORMANCE_TESTS)
add_subdirectory(documentation/Reverbs/FDN)
endif()

option(EXPLORE_STUFF "Explore" ON)
if (EXPLORE_STUFF)
    add_subdirectory(documentation/Filters/BandpassImpulses)
    add_subdirectory(documentation/VelvetNoise)
endif()
