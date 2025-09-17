cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
include(GoogleTest)

# Add pffft as a static library from submodule
set(PFFFT_DIR "${PROJECT_SOURCE_DIR}/3rdparty/pffft")

# Check which pffft files exist
if(EXISTS "${PFFFT_DIR}/pffft_common.c")
    set(PFFFT_SOURCES ${PFFFT_DIR}/pffft.c ${PFFFT_DIR}/pffft_common.c)
else()
    set(PFFFT_SOURCES ${PFFFT_DIR}/pffft.c)
endif()

add_library(pffft_lib STATIC ${PFFFT_SOURCES})
target_include_directories(pffft_lib PUBLIC ${PFFFT_DIR})
set_target_properties(pffft_lib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Dependencies")

# Create static library from implementation files
file(GLOB_RECURSE LIB_SOURCES
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/includes/**/*.cpp")

# Only create library if we have implementation files
if (LIB_SOURCES)
    add_library(audio_dsp_lib STATIC ${LIB_SOURCES})
    target_include_directories(audio_dsp_lib PUBLIC
            "${PROJECT_SOURCE_DIR}/src/includes"
            ${PFFFT_DIR})

    target_link_libraries(audio_dsp_lib PUBLIC pffft_lib)

    target_compile_features(audio_dsp_lib PUBLIC cxx_std_20)

    message(STATUS "Created audio_dsp_lib with ${CMAKE_MATCH_COUNT} source files")
    foreach (source ${LIB_SOURCES})
        message(STATUS "  - ${source}")
    endforeach ()
else ()
    message(STATUS "No implementation files found, library will be header-only")
endif ()

# Updated macro that links against the library
macro(package_add_test TESTNAME)
    if (${ARGC} EQUAL 2)
        if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}")
            file(GLOB_RECURSE TEST_SOURCES
                    CONFIGURE_DEPENDS
                    "${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}/*_test.cpp")
            if (NOT TEST_SOURCES)
                message(WARNING "No test files found in directory ${ARGV1}")
                return()
            endif ()
            set(SOURCE_FILES ${TEST_SOURCES})
        else ()
            set(SOURCE_FILES ${ARGN})
        endif ()
    else ()
        set(SOURCE_FILES ${ARGN})
    endif ()

    add_executable(${TESTNAME} ${SOURCE_FILES})

    if (TARGET audio_dsp_lib)
        target_link_libraries(${TESTNAME} audio_dsp_lib pffft_lib gtest gmock gtest_main)
    else ()
        target_link_libraries(${TESTNAME} pffft_lib gtest gmock gtest_main)
        target_include_directories(${TESTNAME} PRIVATE
                "${PROJECT_SOURCE_DIR}/src/includes"
                ${PFFFT_DIR})
    endif ()

    add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

include_directories("${PROJECT_SOURCE_DIR}/src/includes")

mark_as_advanced(
        BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
        gmock_build_tests gtest_build_samples gtest_build_tests
        gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)

macro(SUBDIRLIST result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach (child ${children})
        if (IS_DIRECTORY ${curdir}/${child})
            list(APPEND dirlist ${child})
        endif ()
    endforeach ()
    set(${result} ${dirlist})
endmacro()

set(ALL_TEST_TARGETS "")

subdirlist(TEST_SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach (subdir ${TEST_SUBDIRS})
    file(GLOB_RECURSE test_files
            "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/*_test.cpp")
    if (test_files)
        package_add_test(${subdir}Tests ${subdir})
        message(STATUS "Added test target: ${subdir}Tests for directory: ${subdir}")

        list(APPEND ALL_TEST_TARGETS ${subdir}Tests)
    endif ()
endforeach ()

add_custom_target(unit_tests
        DEPENDS ${ALL_TEST_TARGETS}
        COMMENT "Building all unit test executables"
)

add_custom_target(run_unit_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS unit_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building and running all unit tests"
)