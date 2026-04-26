cmake_minimum_required(VERSION 3.22)
project(BlockShuffler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(BlockShufflerResources
    SOURCES
        Resources/icon.svg
        Resources/icon.png
        Resources/fonts/Inter-Regular.ttf
        Resources/fonts/Inter-Bold.ttf
        Resources/fonts/JetBrainsMono-Regular.ttf
)

juce_add_plugin(BlockShuffler
    COMPANY_NAME "gobi10k"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Gb1k
    PLUGIN_CODE BkSh
    FORMATS Standalone VST3
    PRODUCT_NAME "BlockShuffler"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
)

target_sources(BlockShuffler PRIVATE
    # Entry
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp

    # Model
    Source/Model/Project.h
    Source/Model/Project.cpp
    Source/Model/Block.h
    Source/Model/Block.cpp
    Source/Model/Clip.h
    Source/Model/Clip.cpp
    Source/Model/BlockLink.h
    Source/Model/BlockLink.cpp
    Source/Model/Serialization.h
    Source/Model/Serialization.cpp

    # Audio
    Source/Audio/ArrangementResolver.h
    Source/Audio/ArrangementResolver.cpp
    Source/Audio/PlaybackEngine.h
    Source/Audio/PlaybackEngine.cpp
    Source/Audio/TempoStretcher.h
    Source/Audio/TempoStretcher.cpp
    Source/Audio/ExportRenderer.h
    Source/Audio/ExportRenderer.cpp

    # UI
    Source/UI/BlockStrip.h
    Source/UI/BlockStrip.cpp
    Source/UI/BlockComponent.h
    Source/UI/BlockComponent.cpp
    Source/UI/BlockStackView.h
    Source/UI/BlockStackView.cpp
    Source/UI/ClipListPanel.h
    Source/UI/ClipListPanel.cpp
    Source/UI/ClipWaveformView.h
    Source/UI/ClipWaveformView.cpp
    Source/UI/InspectorPanel.h
    Source/UI/InspectorPanel.cpp
    Source/UI/TransportBar.h
    Source/UI/TransportBar.cpp
    Source/UI/BlockLinkOverlay.h
    Source/UI/BlockLinkOverlay.cpp
    Source/UI/LookAndFeel_BlockShuffler.h
    Source/UI/LookAndFeel_BlockShuffler.cpp

    # Utils
    Source/Utils/WeightedRandom.h
    Source/Utils/GridSnap.h
    Source/Utils/UuidGenerator.h
)

if(WIN32)
    target_sources(BlockShuffler PRIVATE Resources/BlockShuffler.rc)
endif()

target_compile_definitions(BlockShuffler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)

# Copy app.icns into the bundle and set CFBundleIconFile (macOS only)
if(APPLE)
    add_custom_command(TARGET BlockShuffler_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app.icns"
                "$<TARGET_BUNDLE_DIR:BlockShuffler_Standalone>/Contents/Resources/app.icns"
        COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_icon.sh"
                "$<TARGET_BUNDLE_DIR:BlockShuffler_Standalone>/Contents/Info.plist"
        COMMENT "Installing app.icns into bundle"
    )
endif()

# Re-sign the Standalone app with ad-hoc signature after build (macOS only)
if(APPLE)
    add_custom_command(TARGET BlockShuffler_Standalone POST_BUILD
        COMMAND codesign --force --deep --sign -
                "$<TARGET_BUNDLE_DIR:BlockShuffler_Standalone>"
        COMMENT "Ad-hoc signing BlockShuffler.app"
    )
endif()

if(UNIX AND NOT APPLE)
    find_package(ALSA REQUIRED)
endif()

target_link_libraries(BlockShuffler
    PRIVATE
        BlockShufflerResources
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(UNIX AND NOT APPLE)
    target_link_libraries(BlockShuffler PRIVATE ALSA::ALSA)
endif()
