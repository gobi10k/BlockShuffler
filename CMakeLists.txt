cmake_minimum_required(VERSION 3.22)
project(BlockShuffler VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(/Users/alecgordon/JUCE libs/JUCE)

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
)

# Re-sign the Standalone app with ad-hoc signature after build
add_custom_command(TARGET BlockShuffler_Standalone POST_BUILD
    COMMAND codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:BlockShuffler_Standalone>"
    COMMENT "Ad-hoc signing BlockShuffler.app"
)

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
