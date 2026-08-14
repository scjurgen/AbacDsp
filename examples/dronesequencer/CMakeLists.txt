cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "Dronesequencer")
set(FORMATS AU VST3 AUv3 Standalone)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS ../../VERSION CURRENT_VERSION)
else()
    set(CURRENT_VERSION "0.1.0")
endif()

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

set(DRONESEQUENCER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

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

set(PLIST_EXTRAS_CONTENT
    [=[
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
</dict>
</plist>
]=]
)

set(CMAKE_FOLDER "Examples/${PROJECT_NAME}")

# Lua, compiled as C++ (not C): lua_error then propagates as a real C++ exception
# instead of a longjmp that would skip destructors, which matters once Lua state
# lives next to RAII C++ objects on the audio thread. SOL_USING_CXX_LUA=1 below keeps
# sol2's declarations on the matching (non "extern C") linkage.
set(LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/lua)
set(SOL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/sol2)

file(GLOB LUA_IMPL_SRCS ${LUA_DIR}/*.c)
list(FILTER LUA_IMPL_SRCS EXCLUDE REGEX ".*/(lua|luac|onelua|ltests)\\.c$")
set_source_files_properties(${LUA_IMPL_SRCS} PROPERTIES LANGUAGE CXX)

add_library(dronesequencer_lua STATIC ${LUA_IMPL_SRCS})
target_include_directories(dronesequencer_lua PUBLIC ${LUA_DIR})
set_target_properties(
    dronesequencer_lua
    PROPERTIES FOLDER "Examples/${PROJECT_NAME}"
)

if(APPLE)
    target_compile_definitions(dronesequencer_lua PUBLIC LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(dronesequencer_lua PUBLIC LUA_USE_LINUX)
elseif(WIN32)
    target_compile_definitions(dronesequencer_lua PUBLIC LUA_USE_WINDOWS)
endif()

juce_add_plugin("${PROJECT_NAME}"
        VERSION "0.0.0"
        ICON_BIG "logo.png"
        ICON_SMALL "logo.png"
        COMPANY_NAME "krachpedale"
        COMPANY_WEBSITE "https://krachpedale.de"
        COMPANY_EMAIL "info@krachpedale.de"
        BUNDLE_ID de.krachpedale.dronesequencer
        PLIST_TO_MERGE "${PLIST_EXTRAS_CONTENT}"
        IS_SYNTH  TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE KrPd
        PLUGIN_CODE I785
        FORMATS "${FORMATS}"
        PRODUCT_NAME "${PROJECT_NAME}"
)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

file(GLOB IncludeFiles "src/inc/*.h" "src/inc/*.cpp")
file(GLOB RootFiles "src/*.h" "src/*.cpp")
file(GLOB ImplFiles "src/impl/*.h" "src/impl/*.cpp")

set(SourceFiles ${IncludeFiles} ${RootFiles} ${ImplFiles})

# exclude StubImpl.h: the real DSP engine (DroneSequencerImpl.h) is hand-written
list(FILTER SourceFiles EXCLUDE REGEX ".*StubImpl\\.h$")

target_include_directories("${PROJECT_NAME}" PUBLIC ../../src/includes)
target_include_directories("${PROJECT_NAME}" SYSTEM PRIVATE ${SOL2_DIR}/include)
target_compile_definitions("${PROJECT_NAME}" PRIVATE SOL_USING_CXX_LUA=1)
target_compile_definitions(
    "${PROJECT_NAME}"
    PRIVATE DRONESEQUENCER_BASE_SCRIPTS_DIR="${DRONESEQUENCER_DIR}/base-scripts"
)

target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX ""
    FILES ${SourceFiles}
)

juce_add_binary_data(DronesequencerAssets SOURCES logo.png)
set_target_properties(
    DronesequencerAssets
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

set(JUCE_DEPENDENCIES juce::juce_audio_utils juce::juce_gui_extra)

target_compile_definitions(
    "${PROJECT_NAME}"
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0
)

target_link_libraries(
    "${PROJECT_NAME}"
    PRIVATE
        DronesequencerAssets
        ${JUCE_DEPENDENCIES}
        nlohmann_json::nlohmann_json
        dronesequencer_lua
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
