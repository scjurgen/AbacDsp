cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "Tapelooper")
set(TAPELOOPER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
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

# Authoring Mode's embedded HTTP server (see AuthoringHttpServer.h). Loopback-only, HTTP
# never HTTPS - explicitly off, not just left at cpp-httplib's own defaults, so the build
# doesn't silently start linking OpenSSL/zlib/etc. just because they happen to be present
# on whatever machine is building it.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.54.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(httplib)

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
        BUNDLE_ID de.krachpedale.tapelooper
        PLIST_TO_MERGE "${PLIST_EXTRAS_CONTENT}"
        IS_SYNTH  FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE KrPd
        PLUGIN_CODE L072
        FORMATS "${FORMATS}"
        PRODUCT_NAME "${PROJECT_NAME}"
)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

# Lua, compiled as C++ (not C): lua_error then propagates as a real C++ exception
# instead of a longjmp that would skip destructors, which matters once Lua state
# lives next to RAII C++ objects on the audio thread. SOL_USING_CXX_LUA=1 below keeps
# sol2's declarations on the matching (non "extern C") linkage.
set(LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/lua)
set(SOL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/sol2)

file(GLOB LUA_IMPL_SRCS ${LUA_DIR}/*.c)
list(FILTER LUA_IMPL_SRCS EXCLUDE REGEX ".*/(lua|luac|onelua|ltests)\\.c$")
set_source_files_properties(${LUA_IMPL_SRCS} PROPERTIES LANGUAGE CXX)

add_library(tapelooper_lua STATIC ${LUA_IMPL_SRCS})
target_include_directories(tapelooper_lua PUBLIC ${LUA_DIR})
set_target_properties(
    tapelooper_lua
    PROPERTIES FOLDER "Examples/${PROJECT_NAME}"
)

if(APPLE)
    target_compile_definitions(tapelooper_lua PUBLIC LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(tapelooper_lua PUBLIC LUA_USE_LINUX)
elseif(WIN32)
    target_compile_definitions(tapelooper_lua PUBLIC LUA_USE_WINDOWS)
endif()

file(GLOB IncludeFiles "src/inc/*.h" "src/inc/*.cpp")
file(GLOB RootFiles "src/*.h" "src/*.cpp")
file(GLOB ImplFiles "src/impl/*.h" "src/impl/*.cpp")

set(SourceFiles ${IncludeFiles} ${RootFiles} ${ImplFiles})

# exclude StubImpl.h if needed
list(FILTER SourceFiles EXCLUDE REGEX ".*StubImpl\\.h$")

# Include directories for AbacDsp library (from main project)
target_include_directories("${PROJECT_NAME}" PUBLIC ../../src/includes)
target_include_directories("${PROJECT_NAME}" SYSTEM PRIVATE ${SOL2_DIR}/include)
target_compile_definitions("${PROJECT_NAME}" PRIVATE SOL_USING_CXX_LUA=1)
target_compile_definitions(
    "${PROJECT_NAME}"
    PRIVATE TAPELOOPER_BASE_SCRIPTS_DIR="${TAPELOOPER_DIR}/base-scripts"
)
target_compile_definitions(
    "${PROJECT_NAME}"
    PRIVATE TAPELOOPER_FACTORY_PATCHES_DIR="${TAPELOOPER_DIR}/factory-patches"
)

target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

# Fix source_group to use correct root path for this project
source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX ""
    FILES ${SourceFiles}
)

# Use unique binary data target name to avoid conflicts
juce_add_binary_data(TapelooperAssets SOURCES logo.png)
set_target_properties(
    TapelooperAssets
    PROPERTIES POSITION_INDEPENDENT_CODE TRUE
)

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
set(ABACDSP_DEFAULT_GROOVE_NAME "educational/four-four/beat-1-only_v1.mid")
configure_file(
    src/impl/GrooveDefaultPaths.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/GrooveDefaultPaths.h"
    @ONLY
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(
    "${PROJECT_NAME}"
    PRIVATE
        TapelooperAssets
        ${JUCE_DEPENDENCIES}
        nlohmann_json::nlohmann_json
        tapelooper_lua
        httplib::httplib
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
