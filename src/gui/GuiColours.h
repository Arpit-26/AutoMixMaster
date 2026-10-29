#pragma once
#include <juce_graphics/juce_graphics.h>

namespace gui {

/**
 * Modern Ultra-Flagship Color Palette & Design Tokens
 * Inspired by FabFilter Pro-Q 4, oeksound Soothe 2, and iZotope Ozone 11.
 */
struct Palette {
    static constexpr juce::uint32 kObsidianBg       = 0xff0B0D12; // Deep obsidian matte background
    static constexpr juce::uint32 kCardBg           = 0xff131722; // Elevated glassmorphic card
    static constexpr juce::uint32 kCardBgElevated   = 0xff181E2E; // Higher elevation card / module
    static constexpr juce::uint32 kCardBorder       = 0xff232A3B; // Subtle 1px card border
    static constexpr juce::uint32 kCardBorderHover  = 0xff3A4660; // Card border on hover/focus

    static constexpr juce::uint32 kNeonCyan         = 0xff00F0FF; // Master Spectrum & Mid signal
    static constexpr juce::uint32 kNeonCyanAlpha25  = 0x4000F0FF; // 25% alpha under curve
    static constexpr juce::uint32 kNeonCyanAlpha05  = 0x0D00F0FF; // 5% alpha base fill

    static constexpr juce::uint32 kElectricViolet   = 0xff9D4EDD; // Dynamic Resonance & Ducking
    static constexpr juce::uint32 kElectricVioletAlpha = 0x449D4EDD;
    static constexpr juce::uint32 kWarmAmber        = 0xffFF9F1C; // Glue & Limiter Gain Reduction
    static constexpr juce::uint32 kWarmAmberAlpha   = 0x44FF9F1C;
    static constexpr juce::uint32 kEmeraldGreen     = 0xff10B981; // PASS compliance / Active indicator
    static constexpr juce::uint32 kEmeraldGreenAlpha= 0x3310B981;
    static constexpr juce::uint32 kCoralRed         = 0xffEF4444; // Warning / Over / Heavy GR
    static constexpr juce::uint32 kPreCurveSlate    = 0xff475569; // Pre-processing subtle curve

    static constexpr juce::uint32 kTextPrimary      = 0xffF8FAFC; // Crisp off-white
    static constexpr juce::uint32 kTextSecondary    = 0xff94A3B8; // Cool slate gray
    static constexpr juce::uint32 kTextMuted        = 0xff64748B; // Darker slate for labels
    static constexpr juce::uint32 kGridLine         = 0xff161B26; // Grid line on spectrum canvas
    static constexpr juce::uint32 kGridLineMajor    = 0xff222A3C; // Major frequency/dB grid line

    static inline juce::Colour obsidian()       { return juce::Colour(kObsidianBg); }
    static inline juce::Colour card()           { return juce::Colour(kCardBg); }
    static inline juce::Colour cardElevated()   { return juce::Colour(kCardBgElevated); }
    static inline juce::Colour cardBorder()     { return juce::Colour(kCardBorder); }
    static inline juce::Colour cardBorderHover(){ return juce::Colour(kCardBorderHover); }
    static inline juce::Colour cyan()           { return juce::Colour(kNeonCyan); }
    static inline juce::Colour violet()         { return juce::Colour(kElectricViolet); }
    static inline juce::Colour amber()          { return juce::Colour(kWarmAmber); }
    static inline juce::Colour emerald()        { return juce::Colour(kEmeraldGreen); }
    static inline juce::Colour coral()          { return juce::Colour(kCoralRed); }
    static inline juce::Colour preCurve()       { return juce::Colour(kPreCurveSlate); }
    static inline juce::Colour textPrimary()    { return juce::Colour(kTextPrimary); }
    static inline juce::Colour textSecondary()  { return juce::Colour(kTextSecondary); }
    static inline juce::Colour textMuted()      { return juce::Colour(kTextMuted); }
    static inline juce::Colour gridLine()       { return juce::Colour(kGridLine); }
    static inline juce::Colour gridMajor()      { return juce::Colour(kGridLineMajor); }
};

struct Typography {
    static inline juce::Font title(float size = 15.0f) {
        return juce::Font(juce::FontOptions(size, juce::Font::bold));
    }
    static inline juce::Font heading(float size = 13.0f) {
        return juce::Font(juce::FontOptions(size, juce::Font::bold));
    }
    static inline juce::Font body(float size = 12.0f) {
        return juce::Font(juce::FontOptions(size, juce::Font::plain));
    }
    static inline juce::Font caption(float size = 10.5f) {
        return juce::Font(juce::FontOptions(size, juce::Font::plain));
    }
    static inline juce::Font code(float size = 10.0f) {
        return juce::Font(juce::FontOptions("Consolas", size, juce::Font::plain));
    }
    static inline juce::Font tiny(float size = 9.0f) {
        return juce::Font(juce::FontOptions(size, juce::Font::plain));
    }
    static inline juce::Font meter(float size = 11.5f) {
        return juce::Font(juce::FontOptions("Consolas", size, juce::Font::bold));
    }
};

} // namespace gui
