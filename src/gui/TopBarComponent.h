#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "GuiColours.h"
#include "../PluginProcessor.h"

namespace gui {

/**
 * Top Telemetry & Transport Bar
 * Hosts Mode Switcher, Stem Class Selector, Preset Manager,
 * Level-Matched A/B Monitoring, Delta Difference Listening, and FL Studio Sidechain Badge.
 */
class TopBarComponent : public juce::Component {
public:
    explicit TopBarComponent(AutoMixMasterProcessor& proc) : processor(proc) {
        // Mode Buttons
        masterModeBtn.setButtonText("MASTER BUS");
        masterModeBtn.setClickingTogglesState(false);
        masterModeBtn.onClick = [this]() {
            processor.setOperatingMode(dsp::mix::PluginOperatingMode::MasterBus);
            updateButtonStates();
        };
        addAndMakeVisible(masterModeBtn);

        trackModeBtn.setButtonText("TRACK NODE");
        trackModeBtn.setClickingTogglesState(false);
        trackModeBtn.onClick = [this]() {
            processor.setOperatingMode(dsp::mix::PluginOperatingMode::TrackNode);
            updateButtonStates();
        };
        addAndMakeVisible(trackModeBtn);

        // Stem Class Dropdown
        stemClassBox.addItem("Auto-Detect", 1);
        stemClassBox.addItem("Lead Vocal", 2);
        stemClassBox.addItem("Backing Vocal", 3);
        stemClassBox.addItem("Kick", 4);
        stemClassBox.addItem("Snare", 5);
        stemClassBox.addItem("808 / Sub", 6);
        stemClassBox.addItem("Bass Guitar", 7);
        stemClassBox.addItem("Hi-Hats", 8);
        stemClassBox.addItem("Pads / Keys", 9);
        stemClassBox.addItem("Lead Synth", 10);
        stemClassBox.addItem("Drum Bus", 11);
        stemClassBox.setSelectedId(1, juce::dontSendNotification);
        stemClassBox.onChange = [this]() {
            const int id = stemClassBox.getSelectedId();
            using dsp::mix::TrackClass;
            TrackClass tc = TrackClass::Unknown;
            switch (id) {
                case 2: tc = TrackClass::LeadVocal; break;
                case 3: tc = TrackClass::BackingVocal; break;
                case 4: tc = TrackClass::Kick; break;
                case 5: tc = TrackClass::Snare; break;
                case 6: tc = TrackClass::Sub808; break;
                case 7: tc = TrackClass::Bass; break;
                case 8: tc = TrackClass::HiHats; break;
                case 9: tc = TrackClass::PadsKeysGuitars; break;
                case 10: tc = TrackClass::LeadSynth; break;
                case 11: tc = TrackClass::DrumBus; break;
                default: tc = TrackClass::Unknown; break;
            }
            processor.setTrackClass(tc);
        };
        addAndMakeVisible(stemClassBox);

        // Preset Pill Selector
        presetStreamingBtn.setButtonText("STREAMING (-14)");
        presetStreamingBtn.onClick = [this]() {
            processor.getMasterChain().setPreset(dsp::MasterChain::MasteringPreset::StreamingSafe);
            updateButtonStates();
        };
        addAndMakeVisible(presetStreamingBtn);

        presetCompBtn.setButtonText("COMPETITIVE (-10)");
        presetCompBtn.onClick = [this]() {
            processor.getMasterChain().setPreset(dsp::MasterChain::MasteringPreset::Competitive);
            updateButtonStates();
        };
        addAndMakeVisible(presetCompBtn);

        presetDynamicBtn.setButtonText("DYNAMIC (-16)");
        presetDynamicBtn.onClick = [this]() {
            processor.getMasterChain().setPreset(dsp::MasterChain::MasteringPreset::Dynamic);
            updateButtonStates();
        };
        addAndMakeVisible(presetDynamicBtn);

        loadRefBtn.setButtonText("LOAD REFERENCE");
        loadRefBtn.onClick = [this]() {
            loadReferenceFile();
        };
        addAndMakeVisible(loadRefBtn);

        // Monitoring Controls
        levelMatchBtn.setButtonText("LEVEL-MATCH A/B");
        levelMatchBtn.setClickingTogglesState(true);
        levelMatchBtn.setToggleState(false, juce::dontSendNotification);
        levelMatchBtn.onClick = [this]() {
            const bool active = levelMatchBtn.getToggleState();
            processor.setLevelMatchedAB(active);
            processor.setListenBypassA(active);
            repaint();
        };
        addAndMakeVisible(levelMatchBtn);

        deltaBtn.setButtonText("DELTA AUDITION");
        deltaBtn.setClickingTogglesState(true);
        deltaBtn.setToggleState(false, juce::dontSendNotification);
        deltaBtn.onClick = [this]() {
            const bool delta = deltaBtn.getToggleState();
            processor.setDeltaListening(delta);
            repaint();
        };
        addAndMakeVisible(deltaBtn);

        updateButtonStates();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // 1. Header Background & Glassmorphic Border
        g.setColour(Palette::card());
        g.fillRect(bounds);
        g.setColour(Palette::cardBorder());
        g.drawHorizontalLine(static_cast<int>(bounds.getBottom() - 1.0f), 0.0f, bounds.getWidth());

        // 2. Logo & Brand Title
        const float logoX = 14.0f;
        const float logoY = 12.0f;
        const float logoSize = 28.0f;

        // Glowing Hexagonal Icon
        juce::Path hex;
        for (int i = 0; i < 6; ++i) {
            const float angle = static_cast<float>(i * juce::MathConstants<double>::pi / 3.0);
            const float px = logoX + logoSize * 0.5f + (logoSize * 0.45f) * std::cos(angle);
            const float py = logoY + logoSize * 0.5f + (logoSize * 0.45f) * std::sin(angle);
            if (i == 0) hex.startNewSubPath(px, py);
            else hex.lineTo(px, py);
        }
        hex.closeSubPath();
        g.setColour(Palette::cyan().withAlpha(0.2f));
        g.fillPath(hex);
        g.setColour(Palette::cyan());
        g.strokePath(hex, juce::PathStrokeType(1.5f));

        g.setFont(Typography::title(15.0f));
        g.setColour(Palette::textPrimary());
        g.drawText("AUTOMIXMASTER", logoX + logoSize + 8.0f, 10.0f, 135.0f, 18.0f, juce::Justification::centredLeft);

        g.setFont(Typography::tiny(9.0f));
        g.setColour(Palette::emerald());
        g.drawText("PRO VST3", logoX + logoSize + 8.0f, 28.0f, 60.0f, 12.0f, juce::Justification::centredLeft);

        // 3. FL Studio Sidechain Badge
        const auto snap = processor.getTelemetrySnapshot();
        const bool scActive = snap.sidechainActive;
        const auto scBounds = juce::Rectangle<float>(bounds.getRight() - 165.0f, 12.0f, 150.0f, 28.0f);

        g.setColour(scActive ? Palette::emerald().withAlpha(0.12f) : Palette::cardElevated());
        g.fillRoundedRectangle(scBounds, 6.0f);
        g.setColour(scActive ? Palette::emerald() : Palette::cardBorder());
        g.drawRoundedRectangle(scBounds, 6.0f, 1.0f);

        // Status indicator dot
        const float dotX = scBounds.getX() + 10.0f;
        const float dotY = scBounds.getCentreY();
        g.setColour(scActive ? Palette::emerald() : Palette::textMuted());
        g.fillEllipse(dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
        if (scActive) {
            g.setColour(Palette::emerald().withAlpha(0.35f));
            g.fillEllipse(dotX - 6.0f, dotY - 6.0f, 12.0f, 12.0f);
        }

        g.setFont(Typography::caption(10.0f));
        g.setColour(scActive ? Palette::textPrimary() : Palette::textSecondary());
        g.drawText(scActive ? "AUX SC: ACTIVE" : "AUX SC: INACTIVE", dotX + 10.0f, scBounds.getY(), scBounds.getWidth() - 25.0f, scBounds.getHeight(), juce::Justification::centredLeft);
    }

    void resized() override {
        const int h = getHeight();
        const int y = (h - 28) / 2;

        const bool isMaster = (processor.getOperatingMode() == dsp::mix::PluginOperatingMode::MasterBus);

        // Mode switch buttons
        int x = 195;
        masterModeBtn.setBounds(x, y, 92, 28);
        x += 94;
        trackModeBtn.setBounds(x, y, 92, 28);
        x += 96;

        if (!isMaster) {
            stemClassBox.setBounds(x, y, 108, 28);
            x += 114;
        }

        // Presets
        presetStreamingBtn.setBounds(x, y, 112, 28);
        x += 114;
        presetCompBtn.setBounds(x, y, 122, 28);
        x += 124;
        presetDynamicBtn.setBounds(x, y, 102, 28);
        x += 104;
        loadRefBtn.setBounds(x, y, 106, 28);
        x += 110;

        // Monitoring
        levelMatchBtn.setBounds(x, y, 130, 28);
        x += 134;
        deltaBtn.setBounds(x, y, 115, 28);
    }

    void updateButtonStates() {
        const bool isMaster = (processor.getOperatingMode() == dsp::mix::PluginOperatingMode::MasterBus);
        masterModeBtn.setColour(juce::TextButton::buttonColourId, isMaster ? Palette::cyan().withAlpha(0.25f) : Palette::cardElevated());
        masterModeBtn.setColour(juce::TextButton::textColourOnId, isMaster ? Palette::cyan() : Palette::textSecondary());
        masterModeBtn.setColour(juce::TextButton::textColourOffId, isMaster ? Palette::cyan() : Palette::textSecondary());

        trackModeBtn.setColour(juce::TextButton::buttonColourId, !isMaster ? Palette::violet().withAlpha(0.25f) : Palette::cardElevated());
        trackModeBtn.setColour(juce::TextButton::textColourOnId, !isMaster ? Palette::violet() : Palette::textSecondary());
        trackModeBtn.setColour(juce::TextButton::textColourOffId, !isMaster ? Palette::violet() : Palette::textSecondary());

        stemClassBox.setVisible(!isMaster);

        // Highlight active preset
        const auto preset = processor.getMasterChain().getCurrentPreset();
        presetStreamingBtn.setColour(juce::TextButton::buttonColourId, (preset == dsp::MasterChain::MasteringPreset::StreamingSafe) ? Palette::cyan().withAlpha(0.2f) : Palette::cardElevated());
        presetCompBtn.setColour(juce::TextButton::buttonColourId, (preset == dsp::MasterChain::MasteringPreset::Competitive) ? Palette::amber().withAlpha(0.2f) : Palette::cardElevated());
        presetDynamicBtn.setColour(juce::TextButton::buttonColourId, (preset == dsp::MasterChain::MasteringPreset::Dynamic) ? Palette::emerald().withAlpha(0.2f) : Palette::cardElevated());

        levelMatchBtn.setColour(juce::TextButton::buttonColourId, levelMatchBtn.getToggleState() ? Palette::amber().withAlpha(0.3f) : Palette::cardElevated());
        deltaBtn.setColour(juce::TextButton::buttonColourId, deltaBtn.getToggleState() ? Palette::violet().withAlpha(0.35f) : Palette::cardElevated());

        repaint();
    }

private:
    void loadReferenceFile() {
        fileChooser = std::make_unique<juce::FileChooser>("Select Reference Master Track (.wav / .flac / .mp3)...",
                                                           juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                           "*.wav;*.flac;*.mp3");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                const auto file = fc.getResult();
                if (file.existsAsFile()) {
                    loadRefBtn.setButtonText("REF: " + file.getFileNameWithoutExtension().substring(0, 8) + "...");
                    repaint();
                }
            });
    }

    AutoMixMasterProcessor& processor;

    juce::TextButton masterModeBtn;
    juce::TextButton trackModeBtn;
    juce::ComboBox   stemClassBox;

    juce::TextButton presetStreamingBtn;
    juce::TextButton presetCompBtn;
    juce::TextButton presetDynamicBtn;
    juce::TextButton loadRefBtn;

    juce::TextButton levelMatchBtn;
    juce::TextButton deltaBtn;

    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace gui
