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
    DOCUMENT_EXTENSIONS bsp
    # Square RiverMix app icon (matches Resources/app.icns / the in-app wordmark).
    # JUCE rasterises this PNG into the macOS .icns and the Windows .ico for us, so
    # a single square source drives the icon on BOTH platforms.
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/appicon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/appicon.png"
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
    Source/Audio/EntryMixer.h
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

# Windows .exe icon: JUCE auto-generates an icon resource from ICON_BIG/ICON_SMALL
# above (Resources/appicon.png) and embeds it, so no hand-written .rc is needed.
# (Resources/BlockShuffler.rc + app.ico are retained in-repo only for reference.)

target_compile_definitions(BlockShuffler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)

# macOS .app icon: JUCE generates Icon.icns from ICON_BIG/ICON_SMALL (appicon.png)
# and sets CFBundleIconFile in the bundle's Info.plist automatically — the previous
# hand-rolled app.icns copy + patch_icon.sh are no longer needed.

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

# ── ResolverDiag: PERMANENT regression harness (Step 7B deviation: kept).
# Excluded from default builds — build explicitly with:
#   cmake --build build-diag --target ResolverDiag
# Run the FULL harness before ANY change to resolver, stack, or link code. ──
juce_add_console_app(ResolverDiag PRODUCT_NAME "ResolverDiag")
set_target_properties(ResolverDiag PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(ResolverDiag PRIVATE
    diag/ResolverDiag.cpp
    Source/Model/Project.cpp
    Source/Model/Block.cpp
    Source/Model/Clip.cpp
    Source/Model/BlockLink.cpp
    Source/Model/Serialization.cpp
    Source/Audio/ArrangementResolver.cpp
    Source/Audio/TempoStretcher.cpp
    Source/Audio/ExportRenderer.cpp
    Source/Audio/PlaybackEngine.cpp
    Source/UI/LookAndFeel_BlockShuffler.cpp
    Source/UI/BlockLinkOverlay.cpp
    Source/UI/InspectorPanel.cpp
)
target_include_directories(ResolverDiag PRIVATE Source)
target_compile_definitions(ResolverDiag PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(ResolverDiag
    PRIVATE
        BlockShufflerResources
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
)
