#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "GuiColours.h"
#include "TopBarComponent.h"
#include "SpectrumCanvasComponent.h"
#include "MacroBarComponent.h"
#include "TelemetryDockComponent.h"
#include "../PluginProcessor.h"

namespace gui {

/**
 * AutoMixMaster Modern Flagship AudioProcessorEditor
 * Hardware-accelerated, high-DPI resizable dark UI (1100x680 default, 85% to 150% scale)
 * 60 FPS real-time visual analyzers and level-matched monitoring.
 */
class AutoMixMasterEditor : public juce::AudioProcessorEditor,
                            private juce::Timer {
public:
    explicit AutoMixMasterEditor(AutoMixMasterProcessor& processor);
    ~AutoMixMasterEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /**
     * Render and export a high-resolution snapshot of this editor to PNG file.
     */
    bool saveSnapshotToPng(const juce::File& destinationFile, float scaleFactor = 1.0f);

private:
    void timerCallback() override;

    AutoMixMasterProcessor& ammProcessor;

    TopBarComponent         topBar;
    SpectrumCanvasComponent spectrumCanvas;
    MacroBarComponent       macroBar;
    TelemetryDockComponent  telemetryDock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoMixMasterEditor)
};

} // namespace gui
