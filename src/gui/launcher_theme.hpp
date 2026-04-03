#pragma once

// =============================================================================
// launcher_theme.hpp — Named color constants for the Cermu launcher UI
// =============================================================================
//
// All launcher colors are defined here as inline constexpr ImVec4. Never use
// inline color literals in launcher rendering code. The manufacturer color
// table maps from SystemDescriptor.maker to accent color automatically.
//
// =============================================================================

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include <string_view>
#include <algorithm>
#include <cctype>

namespace launcher_theme {

    // =========================================================================
    // Backgrounds
    // =========================================================================
    inline constexpr ImVec4 kWindowBg           = {0.027f, 0.031f, 0.055f, 1.0f};
    inline constexpr ImVec4 kBarBg              = {0.020f, 0.027f, 0.063f, 1.0f};
    inline constexpr ImVec4 kLeftPanelBg        = {0.024f, 0.031f, 0.063f, 1.0f};
    inline constexpr ImVec4 kSystemRowSelected  = {0.047f, 0.110f, 0.180f, 1.0f};
    inline constexpr ImVec4 kFileRowSelected    = {0.039f, 0.110f, 0.188f, 1.0f};
    inline constexpr ImVec4 kFileRowHover       = {0.035f, 0.047f, 0.094f, 1.0f};
    inline constexpr ImVec4 kSearchInputBg      = {0.035f, 0.047f, 0.110f, 1.0f};
    inline constexpr ImVec4 kCardBgDefault      = {0.031f, 0.039f, 0.078f, 1.0f};

    // =========================================================================
    // Borders
    // =========================================================================
    inline constexpr ImVec4 kPanelBorder        = {0.055f, 0.078f, 0.133f, 1.0f};
    inline constexpr ImVec4 kPanelBorderFocus   = {0.102f, 0.196f, 0.322f, 1.0f};
    inline constexpr ImVec4 kFilterActiveBorder = {0.102f, 0.188f, 0.314f, 1.0f};

    // =========================================================================
    // Text
    // =========================================================================
    inline constexpr ImVec4 kTextPrimary        = {0.867f, 0.933f, 1.000f, 1.0f};
    inline constexpr ImVec4 kTextSecondary      = {0.753f, 0.816f, 0.894f, 1.0f};
    inline constexpr ImVec4 kTextMuted          = {0.416f, 0.494f, 0.596f, 1.0f};
    inline constexpr ImVec4 kTextDimmed         = {0.216f, 0.298f, 0.392f, 1.0f};

    // =========================================================================
    // Accents
    // =========================================================================
    inline constexpr ImVec4 kAccentTeal         = {0.180f, 0.769f, 0.627f, 1.0f};
    inline constexpr ImVec4 kAccentBlue         = {0.353f, 0.682f, 0.910f, 1.0f};

    // =========================================================================
    // Config strip
    // =========================================================================
    inline constexpr ImVec4 kCfgBgDefault       = {0.031f, 0.047f, 0.110f, 1.0f};
    inline constexpr ImVec4 kCfgBgChanged       = {0.039f, 0.078f, 0.149f, 1.0f};
    inline constexpr ImVec4 kCfgBorderDefault   = {0.067f, 0.094f, 0.149f, 1.0f};
    inline constexpr ImVec4 kCfgBorderChanged   = {0.102f, 0.188f, 0.314f, 1.0f};
    inline constexpr ImVec4 kCfgTextDefault     = {0.376f, 0.471f, 0.596f, 1.0f};
    inline constexpr ImVec4 kCfgTextChanged     = {0.604f, 0.784f, 0.933f, 1.0f};

    // =========================================================================
    // Probe result bar (Zone C)
    // =========================================================================
    inline constexpr ImVec4 kProbeBarBg         = {0.031f, 0.059f, 0.047f, 1.0f};
    inline constexpr ImVec4 kProbeBarBorder     = {0.071f, 0.180f, 0.133f, 1.0f};
    inline constexpr ImVec4 kProbeMatch         = {0.180f, 0.769f, 0.627f, 1.0f};
    inline constexpr ImVec4 kProbeAmbiguous     = {0.780f, 0.600f, 0.133f, 1.0f};
    inline constexpr ImVec4 kProbeNoMatch       = {0.800f, 0.267f, 0.267f, 1.0f};

    // =========================================================================
    // Archive breadcrumb segment
    // =========================================================================
    inline constexpr ImVec4 kBreadcrumbArchive  = {0.400f, 0.300f, 0.600f, 1.0f};

    // =========================================================================
    // Tag badge colours (file browser)
    // =========================================================================
    inline constexpr ImVec4 kTagVerified    = {0.180f, 0.769f, 0.460f, 1.0f};  // [!] — green
    inline constexpr ImVec4 kTagBadDump     = {0.800f, 0.267f, 0.267f, 1.0f};  // [b] — red
    inline constexpr ImVec4 kTagHack        = {0.850f, 0.500f, 0.150f, 1.0f};  // [h] — orange
    inline constexpr ImVec4 kTagOverdump    = {0.780f, 0.700f, 0.200f, 1.0f};  // [o] — yellow
    inline constexpr ImVec4 kTagAlternate   = {0.353f, 0.580f, 0.910f, 1.0f};  // [a] — blue
    inline constexpr ImVec4 kTagPirate      = {0.580f, 0.350f, 0.750f, 1.0f};  // [p] — purple
    inline constexpr ImVec4 kTagProto       = {0.180f, 0.650f, 0.627f, 1.0f};  // (Proto)/(Beta) — teal
    inline constexpr ImVec4 kTagUnlicensed  = {0.780f, 0.600f, 0.133f, 1.0f};  // (Unl) — amber
    inline constexpr ImVec4 kTagDefault     = {0.300f, 0.380f, 0.480f, 1.0f};  // other — dim
    inline constexpr ImVec4 kTagBg          = {0.039f, 0.047f, 0.094f, 0.8f};  // pill background

    // File from a different system than the one selected
    inline constexpr ImVec4 kTextForeignFormat = {0.55f, 0.30f, 0.30f, 1.0f};

    // =========================================================================
    // Manufacturer accent colours
    // =========================================================================
    // Each entry: substring to match against SystemDescriptor::maker, then color.
    struct MakerAccent {
        const char* substr;
        ImVec4      color;
    };

    inline constexpr MakerAccent kMakerAccents[] = {
        {"Commodore",    {0.318f, 0.471f, 0.831f, 1.0f}},  // CBM blue
        {"Nintendo",     {0.898f, 0.173f, 0.173f, 1.0f}},  // Nintendo red
        {"Atari",        {0.800f, 0.290f, 0.200f, 1.0f}},  // Atari red-orange
        {"Sinclair",     {0.200f, 0.200f, 0.200f, 1.0f}},  // Sinclair dark
        {"Acorn",        {0.600f, 0.730f, 0.200f, 1.0f}},  // Acorn green
        {"Apple",        {0.500f, 0.500f, 0.500f, 1.0f}},  // Apple grey
        {"Sega",         {0.200f, 0.400f, 0.800f, 1.0f}},  // Sega blue
        {"Amstrad",      {0.300f, 0.600f, 0.400f, 1.0f}},  // Amstrad green
        {"Coleco",       {0.580f, 0.380f, 0.200f, 1.0f}},  // ColecoVision brown
        {"Namco",        {0.900f, 0.300f, 0.300f, 1.0f}},  // Namco red
        {"Robotron",     {0.400f, 0.500f, 0.600f, 1.0f}},  // DDR steel blue
        {"VTech",        {0.600f, 0.400f, 0.200f, 1.0f}},  // VTech orange
        {"Tatung",       {0.300f, 0.500f, 0.700f, 1.0f}},  // Tatung blue
        {"Spectravideo", {0.500f, 0.300f, 0.600f, 1.0f}},  // SVI purple
        {"Memotech",     {0.400f, 0.400f, 0.500f, 1.0f}},  // Memotech grey
        {"Oric",         {0.450f, 0.500f, 0.200f, 1.0f}},  // Oric olive
        {"Microsoft",    {0.200f, 0.500f, 0.400f, 1.0f}},  // MSX teal
        {"ASCII",        {0.200f, 0.500f, 0.400f, 1.0f}},  // MSX teal (ASCII Corp)
    };

    /// Look up accent color for a maker string. Falls back to kAccentBlue.
    inline ImVec4 accent_for_maker(const char* maker) {
        if (!maker) return kAccentBlue;
        std::string_view sv(maker);
        for (const auto& entry : kMakerAccents) {
            if (sv.find(entry.substr) != std::string_view::npos)
                return entry.color;
        }
        return kAccentBlue;
    }

    // =========================================================================
    // Left panel width constant
    // =========================================================================
    inline constexpr float kLeftPanelWidth = 248.0f;

    // =========================================================================
    // Config strip constants
    // =========================================================================
    inline constexpr float kConfigStripHeight   = 36.0f;
    inline constexpr float kProbeBarHeight      = 44.0f;

    // =========================================================================
    // Probe thresholds
    // =========================================================================
    inline constexpr float kSingleMatchThreshold = 0.85f;
    inline constexpr float kNoMatchThreshold     = 0.30f;

} // namespace launcher_theme

#endif // CERMU_HAS_GUI
