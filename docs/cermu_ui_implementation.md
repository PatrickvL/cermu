# Cermu — Launcher UI Implementation Plan

## Purpose

This document maps the UI redesign concept ([cermu_ui_spec.md](cermu_ui_spec.md)) onto
the existing cermu architecture. It replaces both `SystemSelectionDialog` and
`CermuFileDialog` with a single unified launcher panel. The concept spec is
**inspiration**, not a verbatim contract — data models, color tokens, and
structural details follow what already exists in the codebase.

---

## 1. Relationship to Existing Code

### What This Replaces

| Current component | File(s) | Replacement |
|---|---|---|
| `SystemSelectionDialog` | `system_selection_dialog.hpp/.cpp` | Left panel (system list) + config strip |
| `CermuFileDialog` (IGFD wrapper) | `cermu_file_dialog.hpp`, `vfs_file_system.hpp` | Right panel (file browser) |

### What This Preserves

| Concern | Existing code | Reuse strategy |
|---|---|---|
| System registration & discovery | `SystemRegistry`, `SystemDescriptor`, `REGISTER_SYSTEM` | Read-only — enumerate systems from registry |
| Hardware traits & config | `HardwareTraits`, `SystemConfiguration` | Config strip binds directly to these structs |
| System identification / probe | `SystemMatch`, `probe_file()` | Used when user selects a file to auto-detect best system |
| Video standards | `VideoStandard` enum, `VideoStandardConfig` | Region combos map to `video_standard_configs` indices |
| Memory options | `MemoryOption` | Memory combos map to `memory_options` indices |
| Peripheral options | `PeripheralOption` | Peripheral toggles map to `enabled_peripherals` map |
| Custom options | `CustomOption` | Custom combos map to `custom_settings` map |
| File dialog VFS | `VfsFileSystem`, archive browsing | File list in right panel uses VFS for container navigation |
| Session/system switching | `SessionGUI::switch_system()` | Called on Launch with assembled `SystemConfiguration` |
| Format descriptors | `format_descriptor_t`, `supported_formats` | Used for file-type filtering in the file list |

### New Code Required

| Component | Location | Purpose |
|---|---|---|
| `LauncherPanel` | `src/gui/launcher_panel.hpp/.cpp` | Unified launcher UI (replaces both dialogs) |
| `LauncherTheme` | `src/gui/launcher_theme.hpp` | Named color constants for the launcher palette |
| `SharedConfigStore` | `src/gui/shared_config_store.hpp` | Cross-system configuration memory |

---

## 2. Overall Layout

```
┌─ Top bar ──────────────────────────────────────────────────────────────────┐
│ ◈ CERMU  │  [All][Home][Console][Arcade][Other]  Maker▾  │  N/M systems    │
├─ System list (248 px) ───┬─ Content panel (flex) ──────────────────────────┤
│                          │ System header + config strip                    │
│  [search input]          ├─────────────────────────────────────────────────┤
│                          │ File browser (path bar + file list)             │
│  Row: Name | Meta        │ ┌ Path: /home/user/roms/c64/ ────────────────┐ │
│  Row: …                  │ │ [..] [folder/] [game.d64] [game2.prg] …   │ │
│  …                       │ └────────────────────────────────────────────┘ │
├──────────────────────────┴─────────────────────────────────────────────────┤
│ Status bar: key hints                                                      │
└────────────────────────────────────────────────────────────────────────────┘
```

The left panel has a **fixed width of 248 px**. The right panel fills
the remainder and contains a system header with config strip, then a
file browser that supports normal directory navigation (including into
archive/container files via VFS).

---

## 3. Color Tokens

All colors **must** be defined as named constants in `LauncherTheme`, never
as inline literals. The concept spec provides a starting palette; actual
values will be tuned during implementation. Use `constexpr ImVec4` or
`constexpr ImU32` with descriptive names.

```cpp
// launcher_theme.hpp — example structure (values are starting points)
namespace launcher_theme {

    // Backgrounds
    inline constexpr ImVec4 kWindowBg          = {0.027f, 0.031f, 0.055f, 1.0f};
    inline constexpr ImVec4 kBarBg             = {0.020f, 0.027f, 0.063f, 1.0f};
    inline constexpr ImVec4 kLeftPanelBg       = {0.024f, 0.031f, 0.063f, 1.0f};
    inline constexpr ImVec4 kSystemRowSelected = {0.047f, 0.110f, 0.180f, 1.0f};
    inline constexpr ImVec4 kFileRowSelected   = {0.039f, 0.110f, 0.188f, 1.0f};
    inline constexpr ImVec4 kFileRowHover      = {0.035f, 0.047f, 0.094f, 1.0f};
    inline constexpr ImVec4 kSearchInputBg     = {0.035f, 0.047f, 0.110f, 1.0f};

    // Borders
    inline constexpr ImVec4 kPanelBorder       = {0.055f, 0.078f, 0.133f, 1.0f};
    inline constexpr ImVec4 kPanelBorderFocus  = {0.102f, 0.196f, 0.322f, 1.0f};
    inline constexpr ImVec4 kFilterActiveBorder= {0.102f, 0.188f, 0.314f, 1.0f};

    // Text
    inline constexpr ImVec4 kTextPrimary       = {0.867f, 0.933f, 1.000f, 1.0f};
    inline constexpr ImVec4 kTextSecondary     = {0.753f, 0.816f, 0.894f, 1.0f};
    inline constexpr ImVec4 kTextMuted         = {0.416f, 0.494f, 0.596f, 1.0f};
    inline constexpr ImVec4 kTextDimmed        = {0.216f, 0.298f, 0.392f, 1.0f};

    // Accents
    inline constexpr ImVec4 kAccentTeal        = {0.180f, 0.769f, 0.627f, 1.0f};
    inline constexpr ImVec4 kAccentBlue        = {0.353f, 0.682f, 0.910f, 1.0f};

    // Config strip
    inline constexpr ImVec4 kCfgBgDefault      = {0.031f, 0.047f, 0.110f, 1.0f};
    inline constexpr ImVec4 kCfgBgChanged      = {0.039f, 0.078f, 0.149f, 1.0f};
    inline constexpr ImVec4 kCfgBorderDefault  = {0.067f, 0.094f, 0.149f, 1.0f};
    inline constexpr ImVec4 kCfgBorderChanged  = {0.102f, 0.188f, 0.314f, 1.0f};
    inline constexpr ImVec4 kCfgTextDefault    = {0.376f, 0.471f, 0.596f, 1.0f};
    inline constexpr ImVec4 kCfgTextChanged    = {0.604f, 0.784f, 0.933f, 1.0f};

    // Manufacturer accent colors (index by maker string → color)
    // Populated at init from a static table; not inlined here.

} // namespace launcher_theme
```

The manufacturer color table maps from `SystemDescriptor` metadata
(manufacturer substring) to accent colors — **not** from a hardcoded
enum. New systems added via `REGISTER_SYSTEM` automatically pick up
a color match or fall back to a neutral accent.

---

## 4. Data Model — Mapping to Existing Structures

### System List

The system list is populated from `SystemRegistry::instance().get_systems()`.
**No separate `System` struct is needed.** Each entry is a
`std::pair<SystemDescriptor, SystemFactory>` from the registry.

Fields used from `SystemDescriptor`:

| UI element | Source |
|---|---|
| Display name | `descriptor.name` |
| Short ID | `descriptor.short_name` |
| Description | `descriptor.description` |
| CPU info | `descriptor.hardware_traits.audio.chip_name` (audio chip) or extract from description |
| Year / maker | Not currently in `SystemDescriptor` — see §4.1 |

#### 4.1 Missing Metadata

`SystemDescriptor` currently lacks:

- `year` — manufacture year (for display and sorting)
- `maker` — manufacturer name (for filtering and accent color)
- `system_type` — Home / Console / Arcade / Other (for type filter tabs)

**Recommendation:** Add these as optional fields to `SystemDescriptor`:

```cpp
struct SystemDescriptor {
    // ... existing fields ...

    // Optional UI metadata (nullptr / 0 = not displayed)
    const char* maker;        // "Commodore", "Nintendo", …
    int         year;         // 1982, 0 = unknown
    const char* cpu_summary;  // "MOS 6510" — short string for display
    SystemType  type;         // Home, Console, Arcade, Other
};
```

Where `SystemType` is:

```cpp
enum class SystemType : uint8_t { Home, Console, Arcade, Other };
```

These fields are display-only and have no effect on emulation. Systems
that don't set them simply show no year/maker badge.

### File List

The file list is a **real filesystem browser**, not a static ROM database.
It uses the existing VFS layer (`VfsFileSystem`) to navigate directories
and browse into container files (D64, T64, ZIP, etc.).

| UI element | Source |
|---|---|
| File name | Filesystem entry name |
| File size | `stat` / VFS metadata |
| File type | Extension from `format_descriptor_t` matching |
| Directory indicator | VFS `is_dir` |

**Key difference from the concept spec:** The concept shows a static ROM
catalog with region/year/tags metadata. Our implementation uses **live
filesystem browsing** instead. The file list shows actual directory
contents, not a curated database. This preserves the existing file dialog
workflow while presenting it in the new unified layout.

### Configuration

Configuration uses the existing `SystemConfiguration` struct exactly:

| Config strip element | Maps to |
|---|---|
| Region combo | `config.region_option_index` → `hardware_traits.video_standard_configs` |
| Memory combo | `config.memory_option_index` → `hardware_traits.memory_options` |
| Peripheral toggles | `config.enabled_peripherals` → `hardware_traits.peripheral_options` |
| Custom option combos | `config.custom_settings` → `hardware_traits.custom_options` |

---

## 5. Shared Configuration Persistence

### Problem

When switching between systems, configuration choices that conceptually
apply to multiple systems (e.g., PAL vs NTSC region, joystick attachment)
should be remembered. Currently, `SystemSelectionDialog` resets all
options to defaults on `open()`.

### Design: SharedConfigStore

A lightweight store that remembers the user's last choice for each
configuration option **by semantic key**, not by system. When a system is
selected, its config strip is initialized by:

1. For each `VideoStandardConfig`: look up `"region"` in the store.
   If a stored `VideoStandard` value matches one of this system's
   configs, select it. Otherwise, use the system's `is_default`.

2. For each `PeripheralOption`: look up `periph.id` in the store.
   If found, use the stored enabled/disabled state. Otherwise, use
   `enabled_by_default`.

3. For each `CustomOption`: look up `custom.id` in the store.
   If found and the stored value is a valid choice for this system,
   use it. Otherwise, use `default_index`.

4. For each `MemoryOption`: memory configs are system-specific
   (16KB VIC-20 RAM has nothing to do with 128KB C128 RAM), so these
   are **not** shared. They persist per-system using the system's
   `short_name` as key.

```cpp
// shared_config_store.hpp
#pragma once
#include "core/hardware_traits.hpp"
#include <map>
#include <string>

/// Remembers user configuration choices across system switches.
///
/// Shared options (region, peripherals, custom settings) are keyed by
/// their semantic ID so that e.g. choosing PAL on the C64 also presets
/// PAL on the VIC-20.  Memory options are keyed per-system since they
/// are system-specific.
class SharedConfigStore {
public:
    /// Store the user's region preference (as a VideoStandard value).
    void set_region(VideoStandard standard);

    /// Retrieve stored region, or nullopt if never set.
    std::optional<VideoStandard> get_region() const;

    /// Store a peripheral enabled/disabled state by peripheral ID.
    void set_peripheral(const std::string& periph_id, bool enabled);

    /// Retrieve stored peripheral state, or nullopt if never set.
    std::optional<bool> get_peripheral(const std::string& periph_id) const;

    /// Store a custom option value by option ID.
    void set_custom(const std::string& option_id, const std::string& value);

    /// Retrieve stored custom option value, or nullopt if never set.
    std::optional<std::string> get_custom(const std::string& option_id) const;

    /// Store memory option index for a specific system.
    void set_memory(const std::string& system_short_name, int index);

    /// Retrieve stored memory option index, or nullopt if never set.
    std::optional<int> get_memory(const std::string& system_short_name) const;

    /// Apply stored preferences to a SystemConfiguration, given the
    /// system's HardwareTraits.  Returns the populated config.
    SystemConfiguration build_config(
        const std::string& system_short_name,
        const HardwareTraits& traits) const;

    /// Record all choices from a SystemConfiguration back into the store.
    void record_config(
        const std::string& system_short_name,
        const HardwareTraits& traits,
        const SystemConfiguration& config);

private:
    std::optional<VideoStandard> region_;
    std::map<std::string, bool> peripherals_;          // periph_id → enabled
    std::map<std::string, std::string> custom_;        // option_id → value
    std::map<std::string, int> memory_per_system_;     // short_name → index
};
```

**Lifecycle:** `SharedConfigStore` is owned by `LauncherPanel` (or by
`SessionGUI` and passed to the launcher). It lives for the application
lifetime. It can optionally be serialized to a TOML/JSON user config
file for persistence across sessions — but that is a separate concern
and not required for the initial implementation.

### Interaction with `build_config`

```
build_config("C64", c64_traits):
  1. region_  == PAL  → scan c64_traits.video_standard_configs for PAL
     → found at index 0 → region_option_index = 0
  2. peripherals_["joystick_port2"] == true
     → enabled_peripherals["joystick_port2"] = true
  3. custom_["sid_revision"] == "MOS 8580"
     → "MOS 8580" is a valid choice → custom_settings["sid_revision"] = "MOS 8580"
  4. memory_per_system_["C64"] == 0 → memory_option_index = 0
```

When the user changes any config strip value, `record_config` is called
immediately so the choice is available for the next system switch.

---

## 6. File Browser (Right Panel — Lower Zone)

The file browser replaces `CermuFileDialog` with an integrated panel.
It provides the same functionality:

### 6.1 Path Bar

A breadcrumb-style path bar showing the current directory. Each segment
is clickable to navigate up. A "default folder" button jumps to the
system's data folder (`SystemDescriptor::data_folder` resolved via
`PathDiscovery`).

### 6.2 File List

Columns:

| Column | Width | Content |
|---|---|---|
| Name | flex | File/directory name, icon for type |
| Size | 70 px | Human-readable file size |
| Type | 60 px | Extension or format short name |

- Directories appear first, sorted alphabetically.
- Files are sorted alphabetically by default.
- Clicking a directory navigates into it.
- Clicking a container file (D64, ZIP, etc.) selects it; double-clicking
  navigates into it via VFS (same behavior as current `CermuFileDialog`).
- Clicking a file selects it.
- Double-clicking a file (or pressing Enter) triggers Launch.

### 6.3 Format Filtering

A filter combo populated from the selected system's
`descriptor.supported_formats`. "All supported" is the default; the
user can narrow to a specific format. This replaces the IGFD filter
string that `load_file_dialog()` currently builds.

### 6.4 Remembering Paths

`last_file_path_` (already in `SessionGUI`) stores the last-visited
directory across file browser sessions. Per-system default paths can
be derived from `data_folder` via `PathDiscovery`.

### 6.5 Empty State

When no system is selected or the directory is empty:

```
No files found in this directory
[Browse…]  ← opens a native OS folder picker to set the default path
```

---

## 7. System Header & Config Strip

The system header (top of the right panel) shows:

- System name (from `descriptor.name`)
- Year, CPU summary, maker badge (from the new `SystemDescriptor` fields)
- **Config strip**: horizontal row of combo boxes, one per configuration
  option exposed by `HardwareTraits`

### Config Strip Rendering

```
For each video_standard_configs entry   →  "Region" combo
For each memory_options entry           →  "Memory" combo
For each peripheral_options entry       →  Checkbox / toggle
For each custom_options entry           →  Combo with choices
```

Each control shows a visual "changed from default" indicator (border
color shift) using the `kCfgBorderChanged` / `kCfgBorderDefault` tokens.

When a value changes:
1. Update the working `SystemConfiguration`.
2. Call `shared_config_store_.record_config(...)` to persist the choice.

### Launch Button

The `▶ Launch` button:
1. Assembles final `SystemConfiguration` from the config strip.
2. If a file is selected, calls `SessionGUI::switch_system()` with the
   file as `pending_file`.
3. If no file is selected, calls `switch_system()` without a file
   (bare system boot).

---

## 8. System List (Left Panel)

### Population

```cpp
const auto& systems = SystemRegistry::instance().get_systems();
// Filter by type, maker, search query
// Sort alphabetically by name (or by year, maker)
```

### Filtering

1. **Type filter tabs**: All / Home / Console / Arcade / Other
   → matches `descriptor.type` (new field)
2. **Maker combo**: populated from unique `descriptor.maker` values
3. **Search input**: case-insensitive substring across `name`,
   `short_name`, `description`, `maker`, `cpu_summary`, aliases

### Row Display

Each row shows:
- System name (from `descriptor.name`)
- Year + maker badge (when metadata is available)
- Compact indicator if the system has no `supported_formats` (no files loadable)

### Selection

- Single click: select system, populate config strip and file browser
  with the system's data folder.
- Double click: select system and move focus to the file browser.

---

## 9. Keyboard Navigation

Same scheme as the concept spec, adapted for the file browser:

| Key | Context | Action |
|---|---|---|
| `↑/↓` | System list focused | Navigate systems |
| `↑/↓` | File browser focused | Navigate files |
| `Tab` | Any | Toggle focus between system list and file browser |
| `Enter` | System list | Move focus to file browser |
| `Enter` | File browser, file selected | Launch |
| `Enter` | File browser, dir selected | Navigate into directory |
| `/` | Any (no text input focused) | Focus search input of active panel |
| `Esc` | File browser | Return focus to system list |
| `Backspace` | File browser (no text input) | Navigate to parent directory |

---

## 10. Status Bar

Bottom bar showing context-sensitive keyboard hints. Uses the same
key-hint pill format from the concept spec but with color tokens, not
literals.

---

## 11. Rendering Approach

### Window Setup

Full-window mode identical to the concept spec:

```cpp
ImGui::SetNextWindowPos({0, 0});
ImGui::SetNextWindowSize(io.DisplaySize);
ImGui::Begin("##launcher", nullptr,
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus);
```

### Panel Split

```cpp
ImGui::BeginChild("##syspanel", {248, contentHeight}, false);
// ... system list ...
ImGui::EndChild();
ImGui::SameLine();
ImGui::BeginChild("##contentpanel", {0, contentHeight}, false);
// ... header + config strip + file browser ...
ImGui::EndChild();
```

### Integration Point

`LauncherPanel` is shown instead of the emulation screen when:
- No system is loaded (startup without CLI arguments).
- User selects File → Switch System / Browse.

It replaces both `system_selection_dialog_.render()` and the IGFD file
dialog block in `SessionGUI::render_frame()`.

---

## 12. Implementation Phases

### Phase 1: Foundation
- Add `SystemType`, `maker`, `year`, `cpu_summary` to `SystemDescriptor`.
- Populate these fields in existing system registrations.
- Create `LauncherTheme` with color tokens.
- Create `SharedConfigStore`.

### Phase 2: System List Panel
- Create `LauncherPanel` with system list rendering.
- Wire up filtering (type, maker, search).
- Config strip rendering from `HardwareTraits`.
- `SharedConfigStore` integration for config persistence.

### Phase 3: File Browser
- Integrate VFS-based directory listing in the right panel.
- Path bar with breadcrumb navigation.
- Format filtering from `supported_formats`.
- Container file browsing (D64, ZIP, etc.) via VFS.
- Preserve `last_file_path_` behavior.

### Phase 4: Integration
- Replace `SystemSelectionDialog` and `CermuFileDialog` usage in
  `SessionGUI` with `LauncherPanel`.
- Wire Launch button to `switch_system()`.
- Handle drag-and-drop file paths through the launcher.
- Keyboard navigation.

### Phase 5: Polish
- Tune color tokens.
- Add manufacturer accent colors / badges.
- System header gradient.
- Status bar with key hints.

---

## 13. Open Questions

1. **Fonts**: The concept spec mentions Sora + JetBrains Mono. Do we
   want to bundle these, or rely on system fallbacks? Current code
   uses ImGui's default font.

2. **CRT thumbnails**: The concept spec describes procedurally generated
   mini CRT screens. This is a nice-to-have but not essential for the
   initial implementation. Defer to Phase 5 or later.

3. **Favourites**: The concept spec has a favourites system. This could
   be stored in `SharedConfigStore` or a separate user prefs file.
   Not critical for initial implementation.

4. **TOML serialization of SharedConfigStore**: Persist config choices
   across application restarts. Can be added after basic functionality
   works.

---

*End of implementation plan.*
