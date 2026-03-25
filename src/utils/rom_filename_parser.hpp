#pragma once

// =============================================================================
// rom_filename_parser.hpp — Parse TOSEC / No-Intro / GoodTools ROM filenames
// =============================================================================
//
// Extracts structured metadata from ROM filenames following common naming
// conventions (TOSEC, No-Intro, GoodTools, and freeform variants).
//
// Usage:
//   auto info = rom_filename::parse("Super Mario Bros. (USA) (Rev A).nes");
//   // info.title  == "Super Mario Bros."
//   // info.region == "USA"
//   // info.year   == ""
//   // info.tags   == {}
//
// =============================================================================

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace rom_filename {

/// Parsed metadata from a ROM filename.
struct ParsedInfo {
    std::string              title;    ///< Cleaned game title
    std::string              region;   ///< Region code or full name (e.g. "USA", "Europe")
    std::string              year;     ///< Release year if found (e.g. "1985")
    std::vector<std::string> tags;     ///< Bracket tags: [!], [b], [h], etc.
    std::vector<std::string> flags;    ///< Parenthesized flags: (Unl), (Proto), (Rev A), etc.
};

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace detail {

/// Known region strings (case-insensitive match).
inline bool is_region(std::string_view s) {
    // Normalize to lower for comparison
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Full region names
    static constexpr const char* regions[] = {
        "usa", "europe", "japan", "world", "germany", "france", "spain",
        "italy", "netherlands", "sweden", "australia", "brazil", "canada",
        "china", "korea", "asia", "taiwan", "hong kong", "uk",
        "usa, europe", "japan, usa", "japan, europe", "europe, usa",
        "usa, japan", "europe, australia",
    };
    for (auto r : regions) {
        if (lower == r) return true;
    }

    // Short codes (exact match, 1-2 chars)
    static constexpr const char* codes[] = {
        "u", "e", "j", "w", "g", "f", "s", "i", "nl", "sw",
        "a", "b", "c", "k", "ue", "je", "uj", "ju",
    };
    for (auto c : codes) {
        if (lower == c) return true;
    }

    // Multi-region with commas: "USA, Europe" etc. already handled above
    // Also handle "NTSC" / "PAL" as region hints
    if (lower == "ntsc" || lower == "pal" || lower == "ntsc-u" ||
        lower == "ntsc-j" || lower == "pal-a" || lower == "pal-e") {
        return true;
    }

    return false;
}

/// Check if a parenthesized group is a year (4-digit number 1970-2039, or partial like 19xx).
inline bool is_year(std::string_view s) {
    if (s.size() != 4) return false;
    // 19xx or 20xx style
    if ((s[0] == '1' && s[1] == '9') || (s[0] == '2' && s[1] == '0')) {
        return (std::isdigit(s[2]) || s[2] == 'x') &&
               (std::isdigit(s[3]) || s[3] == 'x');
    }
    return false;
}

} // namespace detail

// ── Main parser ─────────────────────────────────────────────────────────────

/// Parse a ROM filename into structured metadata.
/// Input should be the filename only (not the full path), with or without extension.
inline ParsedInfo parse(std::string_view filename) {
    ParsedInfo info;

    // Strip extension
    std::string name(filename);
    auto dot = name.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        // Make sure this looks like an extension (not too long)
        if (name.size() - dot <= 5) {
            name = name.substr(0, dot);
        }
    }

    // Extract bracket tags: [tag]
    std::string remaining;
    remaining.reserve(name.size());
    for (size_t i = 0; i < name.size(); ) {
        if (name[i] == '[') {
            auto close = name.find(']', i + 1);
            if (close != std::string::npos) {
                info.tags.emplace_back(name.substr(i + 1, close - i - 1));
                i = close + 1;
                continue;
            }
        }
        remaining += name[i];
        ++i;
    }

    // Extract parenthesized groups: (group)
    std::string title_buf;
    title_buf.reserve(remaining.size());
    std::vector<std::string> paren_groups;

    for (size_t i = 0; i < remaining.size(); ) {
        if (remaining[i] == '(') {
            auto close = remaining.find(')', i + 1);
            if (close != std::string::npos) {
                paren_groups.emplace_back(remaining.substr(i + 1, close - i - 1));
                i = close + 1;
                continue;
            }
        }
        title_buf += remaining[i];
        ++i;
    }

    // Classify parenthesized groups
    for (const auto& g : paren_groups) {
        if (detail::is_year(g)) {
            if (info.year.empty()) info.year = g;
            else info.flags.push_back(g);
        } else if (detail::is_region(g)) {
            if (info.region.empty()) info.region = g;
            else info.flags.push_back(g);
        } else {
            info.flags.push_back(g);
        }
    }

    // Clean up title: trim trailing/leading whitespace, collapse internal spaces
    // Remove trailing " - " or " — " fragments from stripped parentheses
    while (!title_buf.empty() && (title_buf.back() == ' ' || title_buf.back() == '-'))
        title_buf.pop_back();
    while (!title_buf.empty() && title_buf.front() == ' ')
        title_buf.erase(title_buf.begin());

    // Collapse multiple spaces
    std::string clean;
    clean.reserve(title_buf.size());
    bool prev_space = false;
    for (char c : title_buf) {
        if (c == ' ') {
            if (!prev_space) clean += c;
            prev_space = true;
        } else {
            clean += c;
            prev_space = false;
        }
    }

    info.title = clean.empty() ? std::string(filename) : clean;
    return info;
}

/// Short region code for column display (3 chars max).
inline std::string_view short_region(std::string_view region) {
    if (region.empty()) return "";

    std::string lower(region);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Full names → short codes
    if (lower == "usa")            return "USA";
    if (lower == "europe")         return "EUR";
    if (lower == "japan")          return "JPN";
    if (lower == "world")          return "WLD";
    if (lower == "germany")        return "DEU";
    if (lower == "france")         return "FRA";
    if (lower == "spain")          return "ESP";
    if (lower == "italy")          return "ITA";
    if (lower == "netherlands")    return "NLD";
    if (lower == "sweden")         return "SWE";
    if (lower == "australia")      return "AUS";
    if (lower == "brazil")         return "BRA";
    if (lower == "canada")         return "CAN";
    if (lower == "china")          return "CHN";
    if (lower == "korea")          return "KOR";
    if (lower == "asia")           return "ASI";
    if (lower == "taiwan")         return "TWN";
    if (lower == "hong kong")      return "HKG";
    if (lower == "uk")             return "UK";

    // Short codes → uppercase
    if (lower == "u")    return "USA";
    if (lower == "e")    return "EUR";
    if (lower == "j")    return "JPN";
    if (lower == "w")    return "WLD";
    if (lower == "g")    return "DEU";
    if (lower == "f")    return "FRA";
    if (lower == "s")    return "ESP";
    if (lower == "i")    return "ITA";
    if (lower == "a")    return "AUS";
    if (lower == "b")    return "BRA";
    if (lower == "c")    return "CAN";
    if (lower == "k")    return "KOR";

    // Multi-region
    if (lower == "usa, europe" || lower == "ue") return "USA+";
    if (lower == "japan, usa" || lower == "ju" || lower == "uj") return "JPN+";
    if (lower == "japan, europe" || lower == "je") return "JPN+";
    if (lower == "europe, usa" || lower == "eu") return "EUR+";
    if (lower == "europe, australia") return "EUR+";

    // NTSC/PAL
    if (lower == "ntsc" || lower == "ntsc-u") return "NTSC";
    if (lower == "ntsc-j") return "JPN";
    if (lower == "pal" || lower == "pal-e" || lower == "pal-a") return "PAL";

    // Return as-is if short enough, truncate otherwise
    return region.size() <= 4 ? region : region.substr(0, 3);
}

} // namespace rom_filename
