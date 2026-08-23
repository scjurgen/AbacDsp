cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "Looper")
set(FORMATS AU VST3 AUv3 Standalone)

# Check if we're building as part of main project or standalone
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS ../../VERSION CURRENT_VERSION)
else()
    set(CURRENT_VERSION "0.1.0")
endif()

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET
    "10.13"
    CACHE STRING
    "Support macOS down to High Sierra"
)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

set(CMAKE_FOLDER "Dependencies")

option(
    JUCE_ENABLE_MODULE_SOURCE_GROUPS
    "Show all module sources in IDE projects"
    ON
)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# Only add JUCE if not already available (standalone build)
if(NOT TARGET juce::juce_core)
    add_subdirectory(../../3rdparty/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
endif()

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

set(PLIST_EXTRAS_CONTENT
    [=[
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>NSMicrophoneUsageDescription</key>
    <string>This app requires microphone access for audio input.</string>
</dict>
</plist>
]=]
)

set(CMAKE_FOLDER "Examples/${PROJECT_NAME}")

juce_add_plugin("${PROJECT_NAME}"
        VERSION "0.0.0"
        ICON_BIG "logo.png"
        ICON_SMALL "logo.png"
        COMPANY_NAME "krachpedale"
        COMPANY_WEBSITE "https://krachpedale.de"
        COMPANY_EMAIL "info@krachpedale.de"
        BUNDLE_ID de.krachpedale.looper
        PLIST_TO_MERGE "${PLIST_EXTRAS_CONTENT}"
        IS_SYNTH  FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE KrPd
        PLUGIN_CODE L3A2
        FORMATS "${FORMATS}"
        PRODUCT_NAME "${PROJECT_NAME}"
)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

file(GLOB IncludeFiles "src/inc/*.h" "src/inc/*.cpp")
file(GLOB RootFiles "src/*.h" "src/*.cpp")
file(GLOB ImplFiles "src/impl/*.h" "src/impl/*.cpp")

set(SourceFiles ${IncludeFiles} ${RootFiles} ${ImplFiles})

# exclude StubImpl.h if needed
list(FILTER SourceFiles EXCLUDE REGEX ".*StubImpl\\.h$")

# Include directories for AbacDsp library (from main project)
target_include_directories("${PROJECT_NAME}" PUBLIC ../../src/includes)

target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

# Fix source_group to use correct root path for this project
source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX ""
    FILES ${SourceFiles}
)

# Use unique binary data target name to avoid conflicts
juce_add_binary_data(LooperAssets SOURCES logo.png)
set_target_properties(LooperAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

foreach(target ${FORMATS} "All")
    if(TARGET ${PROJECT_NAME}_${target})
        set_target_properties(
            ${PROJECT_NAME}_${target}
            PROPERTIES
                XCODE_SCHEME_EXECUTABLE
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/JUCE/extras/AudioPluginHost/Builds/MacOSX/build/Debug/AudioPluginHost.app"
                XCODE_GENERATE_SCHEME ON
        )
    endif()
endforeach()

set(JUCE_DEPENDENCIES juce::juce_audio_utils)

target_compile_definitions(
    "${PROJECT_NAME}"
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0
)

# Groove kit / default groove file: user-supplied, copyrighted content that lives
# outside the repo (see samples/README.md and MidiDrums/README.md), so it is
# referenced by absolute path rather than bundled as binary data. Baked into a
# generated header (not a -D define): CMake refuses to pass a '#' character
# through a compiler command line, and MidiDrums filenames contain one.
set(ABACDSP_DRUM_SAMPLES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/drums/reggae"
)
set(ABACDSP_MIDI_DRUMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../MidiDrums")
set(ABACDSP_DEFAULT_GROOVE_NAME "Session Drums/str_4#4_1_c_v1.mid")
configure_file(
    src/impl/GrooveDefaultPaths.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/GrooveDefaultPaths.h"
    @ONLY
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(
    "${PROJECT_NAME}"
    PRIVATE LooperAssets ${JUCE_DEPENDENCIES} nlohmann_json::nlohmann_json
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Color warnings and errors
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(src/unittests)
endif()
