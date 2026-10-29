#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace gui {

AutoMixMasterEditor::AutoMixMasterEditor(AutoMixMasterProcessor& p)
    : juce::AudioProcessorEditor(&p),
      ammProcessor(p),
      topBar(p),
      spectrumCanvas(p),
      macroBar(p),
      telemetryDock(p) {

    addAndMakeVisible(topBar);
    addAndMakeVisible(spectrumCanvas);
    addAndMakeVisible(macroBar);
    addAndMakeVisible(telemetryDock);

    // Resizability: 85% (935x578) to 150% (1650x1020), default 1100x680
    setResizable(true, true);
    setResizeLimits(935, 578, 1650, 1020);
    setSize(1100, 680);

    // Butter-smooth 60 FPS real-time analyzer timer
    startTimerHz(60);
}

AutoMixMasterEditor::~AutoMixMasterEditor() {
    stopTimer();
}

void AutoMixMasterEditor::paint(juce::Graphics& g) {
    // Deep obsidian matte background
    g.fillAll(Palette::obsidian());
}

void AutoMixMasterEditor::resized() {
    const int w = getWidth();
    const int h = getHeight();

    // 1. Top Bar
    const int topBarH = 52;
    topBar.setBounds(0, 0, w, topBarH);

    // 2. Right-Hand Telemetry Dock
    const int dockW = 240;
    const int margin = 12;
    const int dockX = w - dockW - margin;
    const int dockY = topBarH + margin;
    const int dockH = h - dockY - margin;
    telemetryDock.setBounds(dockX, dockY, dockW, dockH);

    // 3. Center Spectrum & Bottom Macro Area
    const int mainX = margin;
    const int mainW = dockX - mainX - 10;
    const int mainH = dockH;

    // Macro Bar height is fixed at ~160px, Spectrum Canvas takes remaining
    const int macroH = 162;
    const int spectrumH = mainH - macroH - 10;

    spectrumCanvas.setBounds(mainX, dockY, mainW, spectrumH);
    macroBar.setBounds(mainX, dockY + spectrumH + 10, mainW, macroH);
}

void AutoMixMasterEditor::timerCallback() {
    const auto snap = ammProcessor.getTelemetrySnapshot();
    spectrumCanvas.updateCurves(snap);
    telemetryDock.updateTelemetry(snap);
}

bool AutoMixMasterEditor::saveSnapshotToPng(const juce::File& destinationFile, float scaleFactor) {
    destinationFile.deleteFile();

    // Force layout update if necessary
    if (getWidth() <= 0 || getHeight() <= 0) {
        setSize(1100, 680);
    }

    // Trigger one manual timer update pass
    timerCallback();

    juce::Image snapshot = createComponentSnapshot(getLocalBounds(), true, scaleFactor);
    if (snapshot.isValid()) {
        std::unique_ptr<juce::FileOutputStream> outStream(destinationFile.createOutputStream());
        if (outStream) {
            juce::PNGImageFormat pngFormat;
            return pngFormat.writeImageToStream(snapshot, *outStream);
        }
    }
    return false;
}

} // namespace gui
