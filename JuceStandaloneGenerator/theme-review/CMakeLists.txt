cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

juce_add_gui_app(ThemeReviewTool
    PRODUCT_NAME "ThemeReviewTool"
)

target_compile_features(ThemeReviewTool PRIVATE cxx_std_20)

target_sources(ThemeReviewTool PRIVATE main.cpp)

target_include_directories(
    ThemeReviewTool
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../templates/sourcefiles/inc"
        "${PROJECT_SOURCE_DIR}/src/includes"
)

target_compile_definitions(
    ThemeReviewTool
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ThemeReviewTool,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ThemeReviewTool,JUCE_VERSION>"
        THEME_REVIEW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    ThemeReviewTool
    PRIVATE juce::juce_gui_extra juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
