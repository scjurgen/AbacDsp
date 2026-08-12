cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
include(GoogleTest)
macro(package_add_test TESTNAME)
    add_executable(${TESTNAME} ${ARGN})
    target_link_libraries(${TESTNAME} gtest gmock gtest_main dronesequencer_lua)
    target_include_directories(${TESTNAME} PRIVATE ${SOL2_DIR}/include)
    target_compile_definitions(${TESTNAME} PRIVATE SOL_USING_CXX_LUA=1)
    gtest_discover_tests(
        ${TESTNAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(
        ${TESTNAME}
        PROPERTIES FOLDER "Examples/dronesequencer/tests"
    )
endmacro()

include_directories("${DRONESEQUENCER_DIR}/src")
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

package_add_test(DroneScriptMemoryPoolTests
        DroneScriptMemoryPool_tests.cpp
)

package_add_test(DroneScriptEngineTests
        DroneScriptEngine_tests.cpp
)

package_add_test(LuaParamRangeMathTests
        LuaParamRangeMath_tests.cpp
)

package_add_test(LuaMusicMathLibTests
        LuaMusicMathLib_tests.cpp
)

package_add_test(DroneSequencerTests
        DroneSequencer_tests.cpp
)

package_add_test(DroneSequencerImplTests
        DroneSequencerImpl_tests.cpp
)

package_add_test(LlmAssistWatcherCoreTests
        LlmAssistWatcherCore_tests.cpp
)

package_add_test(FileIoTests
        FileIo_tests.cpp
)
target_link_libraries(FileIoTests juce::juce_core nlohmann_json::nlohmann_json)
target_compile_definitions(
    FileIoTests
    PRIVATE
        DRONESEQUENCER_BASE_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/base-scripts-fixture"
)

package_add_test(LlmAssistWatcherTests
        LlmAssistWatcher_tests.cpp
)
target_link_libraries(LlmAssistWatcherTests juce::juce_core)
