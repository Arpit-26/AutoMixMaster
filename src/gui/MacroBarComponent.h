#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "GuiColours.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>

namespace gui {

/**
 * Custom Flagship Glassmorphic Rotary Dial with Glowing Arc Meter.
 */
class ModernMacroSlider : public juce::Slider {
public:
    ModernMacroSlider(const juce::String& nameText,
                      const juce::String& subText,
                      juce::Colour activeColour)
        : title(nameText), subtitle(subText), glowColour(activeColour) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setRange(0.0, 1.0, 0.01);
        setValue(0.5);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const float size = std::min(bounds.getWidth(), bounds.getHeight() - 32.0f);
        const float x = (bounds.getWidth() - size) * 0.5f;
        const float y = 4.0f;
        const auto dialBounds = juce::Rectangle<float>(x, y, size, size).reduced(4.0f);

        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        const float currentVal = static_cast<float>(getValue());
        const float currentAngle = startAngle + currentVal * (endAngle - startAngle);

        // 1. Background Arc Track
        juce::Path bgArc;
        bgArc.addCentredArc(dialBounds.getCentreX(), dialBounds.getCentreY(),
                            dialBounds.getWidth() * 0.5f, dialBounds.getHeight() * 0.5f,
                            0.0f, startAngle, endAngle, true);
        g.setColour(Palette::cardBorder());
        g.strokePath(bgArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Active Glowing Meter Arc
        if (currentVal > 0.01f) {
            juce::Path activeArc;
            activeArc.addCentredArc(dialBounds.getCentreX(), dialBounds.getCentreY(),
                                   dialBounds.getWidth() * 0.5f, dialBounds.getHeight() * 0.5f,
                                   0.0f, startAngle, currentAngle, true);
            // Outer Glow
            g.setColour(glowColour.withAlpha(0.25f));
            g.strokePath(activeArc, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // Core Line
            g.setColour(glowColour);
            g.strokePath(activeArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Inner Glass Knob Cap
        const auto knobBounds = dialBounds.reduced(10.0f);
        juce::ColourGradient knobGrad(Palette::cardElevated(), knobBounds.getX(), knobBounds.getY(),
                                      Palette::card(), knobBounds.getRight(), knobBounds.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(knobBounds);
        g.setColour(Palette::cardBorder());
        g.drawEllipse(knobBounds, 1.0f);

        // Knob Pointer Indicator
        const float pointerLength = knobBounds.getWidth() * 0.35f;
        const float px = knobBounds.getCentreX() + pointerLength * std::cos(currentAngle - juce::MathConstants<float>::halfPi);
        const float py = knobBounds.getCentreY() + pointerLength * std::sin(currentAngle - juce::MathConstants<float>::halfPi);
        g.setColour(glowColour);
        g.drawLine(knobBounds.getCentreX(), knobBounds.getCentreY(), px, py, 2.0f);
        g.fillEllipse(knobBounds.getCentreX() - 2.5f, knobBounds.getCentreY() - 2.5f, 5.0f, 5.0f);

        // 4. Value Text Readout in Center
        g.setFont(Typography::meter(11.0f));
        g.setColour(Palette::textPrimary());
        juce::String valStr;
        if (title == "STYLE") {
            valStr = (currentVal < 0.33f) ? "WARM" : ((currentVal > 0.66f) ? "OPEN" : "BALANCED");
        } else {
            valStr = juce::String(static_cast<int>(std::round(currentVal * 100.0f))) + "%";
        }
        g.drawText(valStr, dialBounds.getX(), dialBounds.getY() + dialBounds.getHeight() * 0.60f, dialBounds.getWidth(), 14.0f, juce::Justification::centred);

        // 5. Title & Subtitle Labels below Dial
        g.setFont(Typography::heading(11.5f));
        g.setColour(Palette::textPrimary());
        g.drawText(title, 0.0f, bounds.getBottom() - 26.0f, bounds.getWidth(), 14.0f, juce::Justification::centred);

        g.setFont(Typography::tiny(9.0f));
        g.setColour(Palette::textMuted());
        g.drawText(subtitle, 0.0f, bounds.getBottom() - 13.0f, bounds.getWidth(), 12.0f, juce::Justification::centred);
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::Colour glowColour;
};

/**
 * Bottom Macro Controls & Expandable 10-Stage Expert Drawer Component.
 */
class MacroBarComponent : public juce::Component {
public:
    explicit MacroBarComponent(AutoMixMasterProcessor& proc)
        : processor(proc),
          styleDial("STYLE", "Warm / Balanced / Open", Palette::cyan()),
          intensityDial("INTENSITY", "0 - 100% Wet Scaling", Palette::cyan()),
          punchGlueDial("PUNCH & GLUE", "VCA Glue & Clipper Drive", Palette::amber()),
          clarityDial("CLARITY & DE-HARSH", "Dynamic Resonance Depth", Palette::violet()) {

        styleDial.onValueChange = [this]() {
            processor.setMacroStyle(static_cast<float>(styleDial.getValue()));
        };
        addAndMakeVisible(styleDial);

        intensityDial.onValueChange = [this]() {
            processor.setMacroIntensity(static_cast<float>(intensityDial.getValue()));
        };
        addAndMakeVisible(intensityDial);

        punchGlueDial.onValueChange = [this]() {
            processor.setMacroPunchGlue(static_cast<float>(punchGlueDial.getValue()));
        };
        addAndMakeVisible(punchGlueDial);

        clarityDial.onValueChange = [this]() {
            processor.setMacroClarity(static_cast<float>(clarityDial.getValue()));
        };
        addAndMakeVisible(clarityDial);

        // Expert Drawer Toggle Button
        drawerToggleBtn.setButtonText("10-STAGE CHAIN [v]");
        drawerToggleBtn.setClickingTogglesState(true);
        drawerToggleBtn.setToggleState(false, juce::dontSendNotification);
        drawerToggleBtn.onClick = [this]() {
            expertDrawerOpen = drawerToggleBtn.getToggleState();
            drawerToggleBtn.setButtonText(expertDrawerOpen ? "10-STAGE CHAIN [^]" : "10-STAGE CHAIN [v]");
            repaint();
        };
        addAndMakeVisible(drawerToggleBtn);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // Elevated Glassmorphic Card Surface
        g.setColour(Palette::card());
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        // If Expert Drawer is open, render the 10-stage status strip
        if (expertDrawerOpen) {
            drawExpertStagesStrip(g);
        }
    }

    void resized() override {
        const int w = getWidth();
        const int h = getHeight();

        // 4 Macro Dials across left side
        const int dialW = 145;
        const int dialH = 120;
        const int startX = 18;
        const int dialY = 12;

        styleDial.setBounds(startX, dialY, dialW, dialH);
        intensityDial.setBounds(startX + dialW + 10, dialY, dialW, dialH);
        punchGlueDial.setBounds(startX + (dialW + 10) * 2, dialY, dialW, dialH);
        clarityDial.setBounds(startX + (dialW + 10) * 3, dialY, dialW, dialH);

        drawerToggleBtn.setBounds(w - 150, 16, 134, 28);
        drawerToggleBtn.setColour(juce::TextButton::buttonColourId, Palette::cardElevated());
        drawerToggleBtn.setColour(juce::TextButton::textColourOffId, Palette::textSecondary());
    }

private:
    void drawExpertStagesStrip(juce::Graphics& g) {
        const auto stripBounds = juce::Rectangle<float>(14.0f, 136.0f, getWidth() - 28.0f, 22.0f);
        g.setColour(Palette::cardElevated());
        g.fillRoundedRectangle(stripBounds, 4.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(stripBounds, 4.0f, 1.0f);

        const char* stageNames[] = {
            "1.TRIM", "2.NOTCH", "3.GLUE", "4.LOW-DYN", "5.WIDTH",
            "6.SAT", "7.EQ", "8.DE-RES", "9.CLIP", "10.LIMIT"
        };
        const float colW = stripBounds.getWidth() / 10.0f;
        g.setFont(Typography::tiny(8.5f));

        for (int i = 0; i < 10; ++i) {
            const float sx = stripBounds.getX() + i * colW;
            const auto colRect = juce::Rectangle<float>(sx, stripBounds.getY(), colW, stripBounds.getHeight());

            g.setColour(Palette::emerald().withAlpha(0.85f));
            g.drawText(stageNames[i], colRect, juce::Justification::centred);

            if (i < 9) {
                g.setColour(Palette::cardBorder());
                g.drawVerticalLine(static_cast<int>(sx + colW), stripBounds.getY() + 3.0f, stripBounds.getBottom() - 3.0f);
            }
        }
    }

    AutoMixMasterProcessor& processor;
    ModernMacroSlider styleDial;
    ModernMacroSlider intensityDial;
    ModernMacroSlider punchGlueDial;
    ModernMacroSlider clarityDial;

    juce::TextButton drawerToggleBtn;
    bool expertDrawerOpen = false;
};

} // namespace gui
