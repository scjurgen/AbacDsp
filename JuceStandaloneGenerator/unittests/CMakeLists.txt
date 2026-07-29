cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)

file(
    GLOB_RECURSE JUCE_GENERATOR_TEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp"
)

if(JUCE_GENERATOR_TEST_SOURCES)
    add_executable(JuceStandaloneGeneratorTests ${JUCE_GENERATOR_TEST_SOURCES})

    target_include_directories(
        JuceStandaloneGeneratorTests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/JuceStandaloneGenerator/templates/sourcefiles/inc"
    )

    target_link_libraries(JuceStandaloneGeneratorTests PRIVATE gmock gtest_main)

    add_test(
        NAME JuceStandaloneGeneratorTests
        COMMAND JuceStandaloneGeneratorTests
    )
    set_target_properties(JuceStandaloneGeneratorTests PROPERTIES FOLDER tests)
endif()
