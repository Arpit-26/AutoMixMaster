cmake_minimum_required(VERSION 3.24)
project(AutoMixMaster VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# 1. Fetch JUCE 8
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# 2. Fetch RTNeural (Real-time safe neural inference engine)
set(RTNEURAL_XSIMD ON CACHE BOOL "Use XSIMD backend for RTNeural" FORCE)
FetchContent_Declare(
    RTNeural
    GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(RTNeural)

# 3. Shared DSP & Telemetry Static Library
add_library(AutoMixMaster_DSP STATIC
    src/dsp/Biquad.h
    src/dsp/KWeightingFilter.h
    src/dsp/TelemetrySnapshot.h
    src/dsp/LoudnessMeter.h
    src/dsp/LoudnessMeter.cpp
    src/dsp/TruePeakMeter.h
    src/dsp/TruePeakMeter.cpp
    src/dsp/SpectralAnalyzer.h
    src/dsp/SpectralAnalyzer.cpp
    src/dsp/SpatialAnalyzer.h
    src/dsp/SpatialAnalyzer.cpp
    src/dsp/InputStage.h
    src/dsp/CorrectiveEQ.h
    src/dsp/MixBusGlueCompressor.h
    src/dsp/LowBandDynamicProcessor.h
    src/dsp/StereoWidthEnforcer.h
    src/dsp/HarmonicSaturator.h
    src/dsp/TonalBalanceEQ.h
    src/dsp/DynamicResonanceSuppressor.h
    src/dsp/OversampledSoftClipper.h
    src/dsp/TruePeakLimiter.h
    src/dsp/TruePeakLimiter.cpp
    src/dsp/MasterChain.h
    src/dsp/MasterChain.cpp
    src/dsp/mix/TrackNodeTypes.h
    src/dsp/mix/InterPluginBus.h
    src/dsp/mix/InterPluginBus.cpp
    src/dsp/mix/GainStager.h
    src/dsp/mix/PhaseRotator.h
    src/dsp/mix/CrossAdaptiveDucker.h
    src/dsp/mix/StemClassifier.h
    src/dsp/mix/StemClassifier.cpp
    src/dsp/mix/SourceChainProcessor.h
    src/dsp/mix/SourceChainProcessor.cpp
    src/gui/GuiColours.h
    src/gui/TopBarComponent.h
    src/gui/SpectrumCanvasComponent.h
    src/gui/MacroBarComponent.h
    src/gui/TelemetryDockComponent.h
    src/gui/PluginEditor.h
    src/gui/PluginEditor.cpp
    src/PluginProcessor.h
    src/PluginProcessor.cpp
)

target_include_directories(AutoMixMaster_DSP PUBLIC src)

target_compile_definitions(AutoMixMaster_DSP PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AutoMixMaster_DSP PUBLIC
    RTNeural
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    ws2_32
    wininet
    version
    shlwapi
    ole32
    uuid
    winmm
    dbghelp
    imm32
    comctl32
    dcomp
    d2d1
    dwrite
    dxgi
    dxguid
    ksguid
    ksuser
    oleaut32
    comdlg32
    dwmapi
    d3d11
    sapi
)

target_link_options(AutoMixMaster_DSP INTERFACE -static)

# 4. Optional VST3 Plugin Target (Enabled when building VST3 with juceaide support)
option(BUILD_VST3_PLUGIN "Build VST3 Plugin Target" ON)
if (BUILD_VST3_PLUGIN)
    juce_add_plugin(AutoMixMaster_VST3
        COMPANY_NAME "AutoMixMasterLab"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Amml
        PLUGIN_CODE Amm1
        FORMATS VST3 Standalone
        PRODUCT_NAME "AutoMixMaster"
        VST3_AUTO_MANIFEST FALSE
    )

    target_link_libraries(AutoMixMaster_VST3 PRIVATE
        AutoMixMaster_DSP
    )

    target_link_options(AutoMixMaster_VST3 INTERFACE -static)
    if (TARGET AutoMixMaster_VST3_VST3)
        target_link_options(AutoMixMaster_VST3_VST3 PRIVATE -static)
    endif()
    if (TARGET AutoMixMaster_VST3_Standalone)
        target_link_options(AutoMixMaster_VST3_Standalone PRIVATE -static)
    endif()

    add_custom_command(TARGET AutoMixMaster_VST3_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove -f
            "${CMAKE_BINARY_DIR}/AutoMixMaster_VST3_artefacts/Release/VST3/AutoMixMaster.vst3/Contents/Resources/moduleinfo.json"
            "${CMAKE_BINARY_DIR}/AutoMixMaster_VST3_artefacts/Release/VST3/AutoMixMaster.vst3/Contents/moduleinfo.json"
        COMMENT "Ensuring moduleinfo.json is removed for clean FL Studio VST3 factory scan"
    )
endif()

# 5. Headless CLI Verification Runner
juce_add_console_app(AutoMixMaster_CLI
    _NO_RESOURCERC
    PRODUCT_NAME "AutoMixMaster_CLI"
)

target_sources(AutoMixMaster_CLI PRIVATE
    src/cli/main.cpp
    src/cli/TestSignalGenerator.h
)

target_link_options(AutoMixMaster_CLI PRIVATE -static)

target_link_libraries(AutoMixMaster_CLI PRIVATE
    AutoMixMaster_DSP
    ws2_32
    wininet
    version
    shlwapi
    ole32
    uuid
    winmm
    dbghelp
    imm32
    comctl32
    dcomp
    d2d1
    dwrite
    dxgi
    oleaut32
    comdlg32
    dwmapi
    d3d11
    sapi
)
