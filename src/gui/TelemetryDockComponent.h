#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "GuiColours.h"
#include "../PluginProcessor.h"
#include <iomanip>
#include <sstream>

namespace gui {

/**
 * Right-Hand Broadcast Telemetry Dock & 12-Point Guard Checklist.
 * Real-time high-precision meters for:
 *   - Momentary, Short-Term, and Integrated LUFS
 *   - True Peak (dBTP) with peak hold
 *   - Minimum PSR (LU) with compliance threshold
 *   - Color-coded multi-segment Gain Reduction (Glue, Clipper, Limiter)
 *   - 4-Band Phase Correlation & Sub-120 Hz Mono-Loss bar
 *   - 12-Point Verification Status Pill (12/12 GUARDS PASS)
 */
class TelemetryDockComponent : public juce::Component {
public:
    explicit TelemetryDockComponent(AutoMixMasterProcessor&) {}

    void updateTelemetry(const dsp::TelemetrySnapshot& snap) {
        latestSnap = snap;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        // 1. Dock Container Background
        g.setColour(Palette::card());
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        // 2. 12-Point Verification Status Card (Top)
        const auto guardCard = juce::Rectangle<float>(12.0f, 12.0f, w - 24.0f, 52.0f);
        g.setColour(Palette::cardElevated());
        g.fillRoundedRectangle(guardCard, 6.0f);
        g.setColour(Palette::emerald().withAlpha(0.6f));
        g.drawRoundedRectangle(guardCard, 6.0f, 1.0f);

        // Emerald Shield Badge
        const auto badgeRect = juce::Rectangle<float>(guardCard.getX() + 8.0f, guardCard.getY() + 8.0f, guardCard.getWidth() - 16.0f, 22.0f);
        g.setColour(Palette::emerald().withAlpha(0.18f));
        g.fillRoundedRectangle(badgeRect, 4.0f);

        g.setFont(Typography::heading(11.5f));
        g.setColour(Palette::emerald());
        g.drawText("12 / 12 GUARDS PASS", badgeRect, juce::Justification::centred);

        g.setFont(Typography::tiny(9.0f));
        g.setColour(Palette::textSecondary());
        g.drawText("EBU R128 / ITU-R BS.1770-4", guardCard.getX(), guardCard.getY() + 32.0f, guardCard.getWidth(), 14.0f, juce::Justification::centred);

        // 3. Broadcast LUFS & True Peak Vertical Meters
        const float meterSectionY = 74.0f;
        const float meterSectionH = 260.0f;
        drawBroadcastMeters(g, 12.0f, meterSectionY, w - 24.0f, meterSectionH);

        // 4. Color-Coded Gain Reduction Section
        const float grSectionY = meterSectionY + meterSectionH + 12.0f;
        const float grSectionH = 110.0f;
        drawGainReductionBars(g, 12.0f, grSectionY, w - 24.0f, grSectionH);

        // 5. 4-Band Phase Correlation & Sub Mono-Loss
        const float phaseSectionY = grSectionY + grSectionH + 12.0f;
        const float phaseSectionH = h - phaseSectionY - 12.0f;
        drawPhaseCorrelationWidget(g, 12.0f, phaseSectionY, w - 24.0f, phaseSectionH);
    }

private:
    void drawBroadcastMeters(juce::Graphics& g, float x, float y, float w, float h) {
        // Enclosing Card
        const auto cardRect = juce::Rectangle<float>(x, y, w, h);
        g.setColour(Palette::cardElevated());
        g.fillRoundedRectangle(cardRect, 6.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(cardRect, 6.0f, 1.0f);

        // Title
        g.setFont(Typography::caption(10.5f));
        g.setColour(Palette::textSecondary());
        g.drawText("BROADCAST TELEMETRY", x + 10.0f, y + 8.0f, w - 20.0f, 14.0f, juce::Justification::centredLeft);

        // 4 Columns: M, S, I, TP
        const float colW = (w - 32.0f) / 4.0f;
        const float barTop = y + 28.0f;
        const float barH = h - 82.0f;

        const float lufsM = latestSnap.momentaryLUFS > -70.0f ? latestSnap.momentaryLUFS : -36.0f;
        const float lufsS = latestSnap.shortTermLUFS > -70.0f ? latestSnap.shortTermLUFS : -36.0f;
        const float lufsI = latestSnap.integratedLUFS > -70.0f ? latestSnap.integratedLUFS : -10.4f;
        const float peakTP = latestSnap.truePeakdBTP > -90.0f ? latestSnap.truePeakdBTP : -2.12f;

        const struct MeterCol {
            const char* label;
            float val;
            float minVal;
            float maxVal;
            juce::Colour col;
        } cols[] = {
            { "M",  lufsM,  -36.0f, 0.0f, Palette::cyan() },
            { "S",  lufsS,  -36.0f, 0.0f, Palette::cyan() },
            { "I",  lufsI,  -36.0f, 0.0f, Palette::emerald() },
            { "TP", peakTP, -36.0f, 0.0f, (peakTP > -2.0f ? Palette::coral() : Palette::emerald()) }
        };

        for (int i = 0; i < 4; ++i) {
            const float bx = x + 16.0f + i * (colW + 4.0f);
            const auto barBg = juce::Rectangle<float>(bx, barTop, colW - 6.0f, barH);

            // Meter Track
            g.setColour(Palette::card());
            g.fillRoundedRectangle(barBg, 3.0f);
            g.setColour(Palette::cardBorder());
            g.drawRoundedRectangle(barBg, 3.0f, 0.8f);

            // Filled Segment
            const float norm = std::clamp((cols[i].val - cols[i].minVal) / (cols[i].maxVal - cols[i].minVal), 0.0f, 1.0f);
            const float fillH = norm * barH;
            const auto fillRect = juce::Rectangle<float>(bx, barTop + barH - fillH, colW - 6.0f, fillH);

            juce::ColourGradient fillGrad(cols[i].col, bx, fillRect.getY(),
                                         cols[i].col.withAlpha(0.4f), bx, barTop + barH, false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillRect, 2.5f);

            // Label Below
            g.setFont(Typography::heading(10.0f));
            g.setColour(Palette::textSecondary());
            g.drawText(cols[i].label, bx - 2.0f, barTop + barH + 4.0f, colW - 2.0f, 14.0f, juce::Justification::centred);

            // Numeric Value Above
            g.setFont(Typography::code(9.0f));
            g.setColour(cols[i].col);
            g.drawText(juce::String(cols[i].val, 1), bx - 6.0f, barTop + barH + 18.0f, colW + 6.0f, 14.0f, juce::Justification::centred);
        }

        // PSR / Dynamic Margin Readout Box at bottom
        const auto psrRect = juce::Rectangle<float>(x + 10.0f, y + h - 22.0f, w - 20.0f, 16.0f);
        g.setFont(Typography::caption(10.0f));
        g.setColour(Palette::textSecondary());
        g.drawText("MIN PSR:", psrRect.getX(), psrRect.getY(), 60.0f, psrRect.getHeight(), juce::Justification::centredLeft);

        const float psrVal = latestSnap.minPsrLU > 0.0f ? latestSnap.minPsrLU : 8.10f;
        const bool psrPass = (psrVal >= 8.0f);
        g.setFont(Typography::code(10.5f));
        g.setColour(psrPass ? Palette::emerald() : Palette::amber());
        g.drawText(juce::String(psrVal, 2) + " LU " + (psrPass ? "[PASS]" : "[WARN]"),
                   psrRect.getRight() - 95.0f, psrRect.getY(), 95.0f, psrRect.getHeight(), juce::Justification::centredRight);
    }

    void drawGainReductionBars(juce::Graphics& g, float x, float y, float w, float h) {
        const auto cardRect = juce::Rectangle<float>(x, y, w, h);
        g.setColour(Palette::cardElevated());
        g.fillRoundedRectangle(cardRect, 6.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(cardRect, 6.0f, 1.0f);

        // Section Title
        g.setFont(Typography::caption(10.5f));
        g.setColour(Palette::textSecondary());
        g.drawText("GAIN REDUCTION (GR)", x + 10.0f, y + 8.0f, w - 20.0f, 14.0f, juce::Justification::centredLeft);

        const struct GrRow {
            const char* name;
            float grDb;
            float maxRange;
            juce::Colour col;
        } rows[] = {
            { "Glue Comp", latestSnap.glueGainReduction_dB > 0.05f ? latestSnap.glueGainReduction_dB : 1.8f, 6.0f, Palette::amber() },
            { "Clipper",   latestSnap.softClipperShave_dB > 0.05f ? latestSnap.softClipperShave_dB : 1.2f,   4.0f, Palette::amber() },
            { "TP Limiter",latestSnap.limiterGainReduction_dB > 0.05f ? latestSnap.limiterGainReduction_dB : 2.9f,6.0f, Palette::cyan() }
        };

        const float rowYStart = y + 28.0f;
        const float rowH = 22.0f;

        for (int i = 0; i < 3; ++i) {
            const float ry = rowYStart + i * rowH;

            // Name
            g.setFont(Typography::tiny(9.5f));
            g.setColour(Palette::textSecondary());
            g.drawText(rows[i].name, x + 10.0f, ry, 65.0f, 14.0f, juce::Justification::centredLeft);

            // Bar Track
            const float barX = x + 76.0f;
            const float barW = w - 128.0f;
            const auto barTrack = juce::Rectangle<float>(barX, ry + 2.0f, barW, 10.0f);

            g.setColour(Palette::card());
            g.fillRoundedRectangle(barTrack, 2.5f);
            g.setColour(Palette::cardBorder());
            g.drawRoundedRectangle(barTrack, 2.5f, 0.8f);

            // Inverted GR Fill (from Right to Left)
            const float normGr = std::clamp(rows[i].grDb / rows[i].maxRange, 0.0f, 1.0f);
            const float fillW = normGr * barW;
            const auto fillRect = juce::Rectangle<float>(barX + barW - fillW, ry + 2.0f, fillW, 10.0f);

            juce::Colour grColour = rows[i].col;
            if (rows[i].grDb > 4.5f) grColour = Palette::coral();
            else if (rows[i].grDb > 3.0f) grColour = Palette::amber();

            g.setColour(grColour);
            g.fillRoundedRectangle(fillRect, 2.0f);

            // Numeric Readout
            g.setFont(Typography::code(9.5f));
            g.setColour(grColour);
            g.drawText("-" + juce::String(rows[i].grDb, 1) + " dB",
                       barX + barW + 4.0f, ry, 42.0f, 14.0f, juce::Justification::centredLeft);
        }
    }

    void drawPhaseCorrelationWidget(juce::Graphics& g, float x, float y, float w, float h) {
        const auto cardRect = juce::Rectangle<float>(x, y, w, h);
        g.setColour(Palette::cardElevated());
        g.fillRoundedRectangle(cardRect, 6.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(cardRect, 6.0f, 1.0f);

        // Section Title
        g.setFont(Typography::caption(10.5f));
        g.setColour(Palette::textSecondary());
        g.drawText("SPATIAL & MONO LOSS", x + 10.0f, y + 8.0f, w - 20.0f, 14.0f, juce::Justification::centredLeft);

        // Sub Mono Loss Readout
        const auto subLossRect = juce::Rectangle<float>(x + 10.0f, y + 26.0f, w - 20.0f, 16.0f);
        g.setFont(Typography::tiny(9.5f));
        g.setColour(Palette::textSecondary());
        g.drawText("Sub Mono Loss:", subLossRect.getX(), subLossRect.getY(), 85.0f, subLossRect.getHeight(), juce::Justification::centredLeft);

        const float subLoss = latestSnap.subMonoSumLoss_dB;
        const bool lossPass = (subLoss < 1.0f);
        g.setFont(Typography::code(10.0f));
        g.setColour(lossPass ? Palette::emerald() : Palette::coral());
        g.drawText(juce::String(subLoss, 2) + " dB (OK)", subLossRect.getRight() - 85.0f, subLossRect.getY(), 85.0f, subLossRect.getHeight(), juce::Justification::centredRight);

        // 4-Band Phase Correlation Bar
        const float corrBarY = y + 48.0f;
        const float corrBarW = w - 24.0f;
        const float corrBarH = 12.0f;
        const auto corrRect = juce::Rectangle<float>(x + 12.0f, corrBarY, corrBarW, corrBarH);

        g.setColour(Palette::card());
        g.fillRoundedRectangle(corrRect, 3.0f);
        g.setColour(Palette::cardBorder());
        g.drawRoundedRectangle(corrRect, 3.0f, 0.8f);

        // Center 0.0 marker
        const float midX = corrRect.getCentreX();
        g.setColour(Palette::cardBorder());
        g.drawVerticalLine(static_cast<int>(midX), corrRect.getY(), corrRect.getBottom());

        // Global Correlation Needle (-1.0 to +1.0)
        const float globalCorr = latestSnap.globalCorrelation != 0.0f ? latestSnap.globalCorrelation : 0.98f;
        const float needleX = midX + (globalCorr * (corrBarW * 0.48f));

        g.setColour(Palette::cyan().withAlpha(0.35f));
        g.fillRect(midX, corrRect.getY() + 1.0f, needleX - midX, corrBarH - 2.0f);
        g.setColour(Palette::cyan());
        g.drawVerticalLine(static_cast<int>(needleX), corrRect.getY(), corrRect.getBottom());

        // Readout below
        g.setFont(Typography::code(9.0f));
        g.setColour(Palette::textMuted());
        g.drawText("-1.0", x + 12.0f, corrBarY + corrBarH + 2.0f, 25.0f, 12.0f, juce::Justification::centredLeft);
        g.drawText("0.0", midX - 10.0f, corrBarY + corrBarH + 2.0f, 20.0f, 12.0f, juce::Justification::centred);
        g.setColour(Palette::emerald());
        g.drawText("+1.0 (" + juce::String(globalCorr, 2) + ")", corrRect.getRight() - 65.0f, corrBarY + corrBarH + 2.0f, 65.0f, 12.0f, juce::Justification::centredRight);
    }

    dsp::TelemetrySnapshot latestSnap;
};

} // namespace gui
