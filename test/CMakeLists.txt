cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
#cmake_policy(SET CMP0174 NEW)
include(GoogleTest)
macro(package_add_test TESTNAME)
    add_executable(${TESTNAME} ${ARGN})
    target_link_libraries(${TESTNAME} gtest gmock gtest_main)
    gtest_discover_tests(${TESTNAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

include_directories("${PROJECT_SOURCE_DIR}/src/includes")

mark_as_advanced(
        BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
        gmock_build_tests gtest_build_samples gtest_build_tests
        gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)

#keep test suites divided E.g. filter, delay, numbers, spectral analysis etc.
#N.B.: keeping alphabetical order helps...

package_add_test(AnalysisTests
        Analysis/EnvelopeFollower_test.cpp
)

package_add_test(AudioTests
        Audio/AudioBufferTest.cpp
        Audio/FixedSizeProcessorTest.cpp
)

package_add_test(FiltersTests
        Filters/Biquad_test.cpp
        Filters/LadderFilter_test.cpp
        Filters/OnePoleFilter_test.cpp
)

package_add_test(NaiveGeneratorsTests
        NaiveGenerators/NaiveGenerators_test.cpp
)

package_add_test(NumbersTests
        Numbers/Conversions_test.cpp
)

package_add_test(ParametersTests
        Parameters/LinearParameterTest.cpp
)

