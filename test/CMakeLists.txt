cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
include(GoogleTest)
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure" "-V")
enable_testing()

set(PFFFT_DIR "${PROJECT_SOURCE_DIR}/3rdparty/pffft")

if(EXISTS "${PFFFT_DIR}/pffft_common.c")
    set(PFFFT_SOURCES ${PFFFT_DIR}/pffft.c ${PFFFT_DIR}/pffft_common.c)
else()
    set(PFFFT_SOURCES ${PFFFT_DIR}/pffft.c)
endif()

add_library(pffft_lib STATIC ${PFFFT_SOURCES})
target_include_directories(pffft_lib PUBLIC ${PFFFT_DIR})
set_target_properties(
    pffft_lib
    PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "Dependencies"
)

file(
    GLOB_RECURSE LIB_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/includes/**/*.cpp"
)

if(LIB_SOURCES)
    add_library(audio_dsp_lib STATIC ${LIB_SOURCES})
    target_include_directories(
        audio_dsp_lib
        PUBLIC "${PROJECT_SOURCE_DIR}/src/includes" ${PFFFT_DIR}
    )

    target_link_libraries(audio_dsp_lib PUBLIC pffft_lib)

    target_compile_features(audio_dsp_lib PUBLIC cxx_std_20)

    if(NOT APPLE)
        target_compile_options(
            audio_dsp_lib
            PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-msse3>
        )
    endif()

    if(ENABLE_COVERAGE)
        target_compile_options(audio_dsp_lib PRIVATE --coverage -O0 -g)
        target_link_options(audio_dsp_lib PUBLIC --coverage)
    endif()

    message(
        STATUS
        "Created audio_dsp_lib with ${CMAKE_MATCH_COUNT} source files"
    )
    foreach(source ${LIB_SOURCES})
        message(STATUS "  - ${source}")
    endforeach()
else()
    message(STATUS "No implementation files found, library will be header-only")
endif()

macro(package_add_test TESTNAME)
    if(${ARGC} EQUAL 2)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}")
            file(
                GLOB_RECURSE TEST_SOURCES
                CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}/*_test.cpp"
            )
            if(NOT TEST_SOURCES)
                message(WARNING "No test files found in directory ${ARGV1}")
                return()
            endif()
            set(SOURCE_FILES ${TEST_SOURCES})
        else()
            set(SOURCE_FILES ${ARGN})
        endif()
    else()
        set(SOURCE_FILES ${ARGN})
    endif()

    add_executable(${TESTNAME} ${SOURCE_FILES})

    if(ENABLE_COVERAGE)
        target_compile_options(${TESTNAME} PRIVATE --coverage -O0 -g)
        target_link_options(${TESTNAME} PRIVATE --coverage)
    endif()

    if(TARGET audio_dsp_lib)
        target_link_libraries(
            ${TESTNAME}
            audio_dsp_lib
            pffft_lib
            gmock
            gtest_main
        )
        if(NOT APPLE)
            target_compile_options(
                ${TESTNAME}
                PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-msse3>
            )
        endif()
    else()
        target_link_libraries(${TESTNAME} pffft_lib gmock gtest_main)
        target_include_directories(
            ${TESTNAME}
            PRIVATE "${PROJECT_SOURCE_DIR}/src/includes" ${PFFFT_DIR}
        )
    endif()

    add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

include_directories("${PROJECT_SOURCE_DIR}/src/includes")
include_directories("${PROJECT_SOURCE_DIR}/refsrc")

mark_as_advanced(
    BUILD_GMOCK
    BUILD_GTEST
    BUILD_SHARED_LIBS
    gmock_build_tests
    gtest_build_samples
    gtest_build_tests
    gtest_disable_pthreads
    gtest_force_shared_crt
    gtest_hide_internal_symbols
)

macro(SUBDIRLIST result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            list(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

set(ALL_TEST_TARGETS "")

subdirlist(TEST_SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach(subdir ${TEST_SUBDIRS})
    file(
        GLOB_RECURSE test_files
        "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/*_test.cpp"
    )
    if(test_files)
        package_add_test(${subdir}Tests ${subdir})
        message(
            STATUS
            "Added test target: ${subdir}Tests for directory: ${subdir}"
        )

        list(APPEND ALL_TEST_TARGETS ${subdir}Tests)
    endif()
endforeach()

set(CMAKE_FOLDER "tests")

add_custom_target(
    unit_tests
    DEPENDS ${ALL_TEST_TARGETS}
    COMMENT "Building all unit test executables"
)

add_custom_target(
    run_unit_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -V --output-on-failure
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running all unit tests"
)

if(ENABLE_COVERAGE)
    find_program(GCOVR_PATH gcovr)
    if(GCOVR_PATH)
        set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
        set(COVERAGE_MD "${PROJECT_SOURCE_DIR}/COVERAGE.md")
        add_custom_target(
            coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND
                ${GCOVR_PATH} --root ${PROJECT_SOURCE_DIR} --filter
                "${PROJECT_SOURCE_DIR}/src/includes/"
                --exclude-unreachable-branches --exclude-throw-branches
                --decisions --print-summary --html-details
                "${COVERAGE_DIR}/index.html" --markdown --markdown-title
                "AbacDsp Coverage Report" -o "${COVERAGE_MD}"
                "${CMAKE_BINARY_DIR}"
            DEPENDS unit_tests
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT
                "Running tests and generating coverage report at ${COVERAGE_MD}"
        )
    else()
        message(
            WARNING
            "ENABLE_COVERAGE is ON but gcovr was not found on PATH; install it (brew install gcovr) to use the 'coverage' target"
        )
    endif()
endif()
