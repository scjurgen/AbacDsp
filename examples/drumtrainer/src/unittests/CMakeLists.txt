cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
#cmake_policy(SET CMP0174 NEW)
include(GoogleTest)
macro(package_add_test TESTNAME)
    add_executable(${TESTNAME} ${ARGN})
    target_link_libraries(${TESTNAME} gtest gmock gtest_main drumtrainer_lua nlohmann_json::nlohmann_json)
    target_include_directories(${TESTNAME} PRIVATE ${SOL2_DIR}/include)
    target_compile_definitions(${TESTNAME} PRIVATE SOL_USING_CXX_LUA=1)
    gtest_discover_tests(
        ${TESTNAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

include_directories("${DRUMTRAINER_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/3rdparty/AbacDsp/src/includes")
include_directories("${PROJECT_SOURCE_DIR}/3rdparty/googletest")

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

package_add_test(DrumtrainerTests
        DrumTrainer_tests.cpp
        DrumPattern_tests.cpp
        DrumTrainerScriptEngine_tests.cpp
        DrumMap_tests.cpp
        HihatClassifier_tests.cpp
        DrumMidiDecoder_tests.cpp
        DrumHitMatcher_tests.cpp
        DrumAnalysisReport_tests.cpp
)

target_compile_definitions(
    DrumtrainerTests
    PRIVATE DRUMTRAINER_BASE_SCRIPTS_DIR="${DRUMTRAINER_DIR}/base-scripts"
            DRUMTRAINER_DRUM_MAPS_DIR="${DRUMTRAINER_DIR}/drum-maps"
)
