cmake_minimum_required(VERSION 3.22)
project(BlockShuffler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable optional X11 extension headers so the project builds in environments
# that only have base X11 libs installed (no -dev packages for Xrandr/Xinerama/etc).
# These flags must be set globally (before FetchContent_MakeAvailable) because JUCE
# builds the juceaide host tool in a subprocess during cmake configure — the tool
# compiles juce_gui_basics and therefore needs these flags too.
#
# TRADE-OFF: JUCE_USE_XRANDR=0 disables compile-time Xrandr support which includes
# multi-monitor awareness code.  In a proper developer environment with
# libxrandr-dev / libxinerama-dev / libxcursor-dev installed, remove these lines
# and the corresponding entries in target_compile_definitions below to restore
# full Linux multi-monitor support.
if(UNIX AND NOT APPLE)
    add_compile_definitions(
        JUCE_USE_XRANDR=0
        JUCE_USE_XINERAMA=0
        JUCE_USE_XSHM=0
        JUCE_USE_XRENDER=0
        JUCE_USE_XCURSOR=0
    )
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(BlockShuffler
    COMPANY_NAME "gobi10k"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Gb1k
    PLUGIN_CODE BkSh
    FORMATS Standalone VST3
    PRODUCT_NAME "BlockShuffler"
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

target_compile_definitions(BlockShuffler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    # Mirror the global X11 extension flags (see top of file).
    JUCE_USE_XRANDR=0
    JUCE_USE_XINERAMA=0
    JUCE_USE_XSHM=0
    JUCE_USE_XRENDER=0
    JUCE_USE_XCURSOR=0
)

# Re-sign the Standalone app with ad-hoc signature after build (macOS only)
if(APPLE)
    add_custom_command(TARGET BlockShuffler_Standalone POST_BUILD
        COMMAND codesign --force --deep --sign -
                "$<TARGET_BUNDLE_DIR:BlockShuffler_Standalone>"
        COMMENT "Ad-hoc signing BlockShuffler.app"
    )
endif()

target_link_libraries(BlockShuffler
    PRIVATE
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
