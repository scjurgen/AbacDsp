cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "Delay")
set(FORMATS AU VST3 AUv3 Standalone)

# Check if we're building as part of main project or standalone
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS ../../VERSION CURRENT_VERSION)
else ()
    set(CURRENT_VERSION "0.1.0")
endif ()

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Support macOS down to High Sierra")
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

# Only add JUCE if not already available (standalone build)
if (NOT TARGET juce::juce_core)
    add_subdirectory(../../3rdparty/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
endif ()

juce_add_plugin("${PROJECT_NAME}"
        VERSION "0.0.0"
        ICON_BIG "logo.png"
        ICON_SMALL "logo.png"
        COMPANY_NAME "krachpedale"
        COMPANY_WEBSITE "https://krachpedale.de"
        COMPANY_EMAIL "info@krachpedale.de"
        BUNDLE_ID de.krachpedale.delay
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE KrPd
        PLUGIN_CODE DE16
        FORMATS "${FORMATS}"
        PRODUCT_NAME "${PROJECT_NAME}")

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

file(GLOB IncludeFiles "src/inc/*.h" "src/inc/*.cpp")
file(GLOB RootFiles "src/*.h" "src/*.cpp")
file(GLOB ImplFiles "src/impl/*.h" "src/impl/*.cpp")

set(SourceFiles ${IncludeFiles} ${RootFiles} ${ImplFiles})

# exclude GenericImpl.h if needed
list(FILTER SourceFiles EXCLUDE REGEX ".*GenericImpl\\.h$")

# Include directories for AbacDsp library (from main project)
target_include_directories("${PROJECT_NAME}" PUBLIC ../../src/includes)

target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

# Fix source_group to use correct root path for this project
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "" FILES ${SourceFiles})

# Use unique binary data target name to avoid conflicts
juce_add_binary_data(DelayAssets SOURCES logo.png)
set_target_properties(DelayAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Set folder properties for all formats
foreach (target ${FORMATS} "All")
    if (TARGET ${PROJECT_NAME}_${target})
        set_target_properties(${PROJECT_NAME}_${target} PROPERTIES
                FOLDER "Targets"
                XCODE_SCHEME_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/JUCE/extras/AudioPluginHost/Builds/MacOSX/build/Debug/AudioPluginHost.app"
                XCODE_GENERATE_SCHEME ON)
    endif ()
endforeach ()
set_target_properties(DelayAssets PROPERTIES FOLDER "Targets")

set(JUCE_DEPENDENCIES
        juce::juce_audio_utils
)

target_compile_definitions("${PROJECT_NAME}"
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=0
)

target_link_libraries("${PROJECT_NAME}"
        PRIVATE
        DelayAssets
        ${JUCE_DEPENDENCIES}
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Color warnings and errors
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif ()
