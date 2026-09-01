cmake_minimum_required(VERSION 3.21)

set_property(GLOBAL PROPERTY USE_FOLDERS YES)

set(PROJECT_NAME "AbacDsp")
file(STRINGS VERSION CURRENT_VERSION)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

# Color our warnings and errors
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

# Option to build only DSP tests (for Docker/Valgrind)
option(BUILD_FULL_PROJECT "Build all, test and JUCE examples" OFF)
include_directories(3rdparty/AudioFile)

if(BUILD_FULL_PROJECT)
    # Add JUCE first - this prevents duplicate target errors
    set(CMAKE_FOLDER "Dependencies")
    add_subdirectory(3rdparty/JUCE)
    unset(CMAKE_FOLDER)

    include_directories(src/includes)
    add_executable(${PROJECT_NAME} main.cpp)

    # EXAMPLES - only when building full project
    add_subdirectory(examples/SamplePlayer)
    add_subdirectory(examples/delay)
    add_subdirectory(examples/dronesequencer)
    add_subdirectory(examples/groover)
    add_subdirectory(examples/guisandbox)
    add_subdirectory(examples/looper)
    add_subdirectory(examples/maxdiffuser)
    add_subdirectory(examples/metronome)
    add_subdirectory(examples/minireverb)
    add_subdirectory(examples/morphexsynth)
    add_subdirectory(examples/pingsynth)
    add_subdirectory(examples/plaingain)
    add_subdirectory(examples/resonik)
    add_subdirectory(examples/sampleplayertimestretched)
    add_subdirectory(examples/spectraltap)
    add_subdirectory(examples/tanpura)
    add_subdirectory(examples/tapelooper)
    add_subdirectory(JuceStandaloneGenerator/theme-review)
else()
    # For tests-only builds, just include DSP headers
    include_directories(src/includes)
endif()

# Add other 3rdparty dependencies (always needed for tests)
set(CMAKE_FOLDER "Dependencies")
add_subdirectory(3rdparty/googletest EXCLUDE_FROM_ALL)
unset(CMAKE_FOLDER)

option(PACKAGE_TESTS "Build the tests" ON)
option(
    ENABLE_COVERAGE
    "Instrument tests with --coverage and add a 'coverage' target (gcovr)"
    OFF
)

if(PACKAGE_TESTS)
    # Prevent CTest from creating dashboard targets but keep DartConfiguration.tcl
    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
    include(CTest)

    # Configure Valgrind for memory checking
    find_program(VALGRIND_PROGRAM NAMES valgrind)
    if(VALGRIND_PROGRAM)
        set(MEMORYCHECK_COMMAND ${VALGRIND_PROGRAM})
        set(MEMORYCHECK_COMMAND_OPTIONS
            "--leak-check=full --show-leak-kinds=all --fair-sched=yes --error-exitcode=1 --track-origins=yes --trace-children=yes"
        )
        set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/valgrind.supp")
    endif()

    add_subdirectory(test)
    add_subdirectory(JuceStandaloneGenerator/unittests)
    if(TARGET unit_tests AND TARGET JuceStandaloneGeneratorTests)
        add_dependencies(unit_tests JuceStandaloneGeneratorTests)
    endif()
endif()

option(EXPLORE_STUFF "Explore" OFF)
if(EXPLORE_STUFF)
    add_subdirectory(documentation/Delays)
    add_subdirectory(documentation/Diffuser)
    add_subdirectory(documentation/Filters/BandpassImpulses)
    add_subdirectory(documentation/Filters/Biquads)
    add_subdirectory(documentation/Filters/OnePoleFilter)
    add_subdirectory(documentation/Filters/PinkFilter)
    add_subdirectory(documentation/Filters/PoleMixing)
    add_subdirectory(documentation/KarplusStrong)
    add_subdirectory(documentation/Modulation)
    add_subdirectory(documentation/OrnsteinUhlenbeck)
    add_subdirectory(documentation/Reverbs/Tanks)
    add_subdirectory(documentation/Slicer)
    add_subdirectory(documentation/VelvetNoise)
endif()
