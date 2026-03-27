# Cermu — Launcher UI Implementation Plan (rev 3)

## Purpose

This document is the definitive implementation plan for the Cermu launcher UI. It
incorporates all design decisions made through the full design review, replacing
both rev 1 and rev 2. It covers the unified launcher panel, file-first probe flow,
emulation HUD and auto-hide menu bar, title browser, cataloging pipeline, scan root
management, and orphan/relocation handling.

All paths throughout the system — launcher, probe, CLI, drag-and-drop — are treated
as VFS paths. No special-casing exists for real filesystem vs archive vs container
entry anywhere.

---

## 1. Relationship to Existing Code

### What This Replaces

| Current component | File(s) | Replacement |
|---|---|---|
| `SystemSelectionDialog` | `system_selection_dialog.hpp/.cpp` | Left panel (system list) + config strip |
| `CermuFileDialog` (IGFD wrapper) | `cermu_file_dialog.hpp`, `vfs_file_system.hpp` | Right panel (file browser and title browser) |

### What This Preserves

| Concern | Existing code | Reuse strategy |
|---|---|---|
| System registration & discovery | `SystemRegistry`, `SystemDescriptor`, `REGISTER_SYSTEM` | Read-only — enumerate systems from registry |
| Hardware traits & config | `HardwareTraits`, `SystemConfiguration` | Config strip binds directly to these structs |
| System probe | `probe_file()`, `SystemProbeResult` | Used for file-first launch and catalog building |
| Video standards | `VideoStandard` enum, `VideoStandardConfig` | Region combos map to `video_standard_configs` indices |
| Memory options | `MemoryOption` | Memory combos map to `memory_options` indices |
| Peripheral options | `PeripheralOption` | Peripheral toggles map to `enabled_peripherals` map |
| Custom options | `CustomOption` | Custom combos map to `custom_settings` map |
| File dialog VFS | `VfsFileSystem`, archive browsing | All path operations use VFS; real FS paths are VFS paths |
| Session/system switching | `SessionGUI::switch_system()` | Called on Launch with assembled `SystemConfiguration` |
| Format descriptors | `format_descriptor_t`, `supported_formats` | Used for file-type filtering in browser and catalog |

### New Code Required

| Component | Planned location | Actual location | Status |
|---|---|---|---|
| `LauncherPanel` | `src/gui/launcher_panel.hpp/.cpp` | `src/gui/launcher_panel.hpp` (header-only) | ✅ Done |
| `LauncherTheme` | `src/gui/launcher_theme.hpp` | `src/gui/launcher_theme.hpp` | ✅ Done |
| `SharedConfigStore` | `src/gui/shared_config_store.hpp` | `src/gui/shared_config_store.hpp` | ✅ Done |
| `ProbeResultPopup` | `src/gui/probe_result_popup.hpp/.cpp` | Inline in `LauncherPanel` (Zone C probe bar) | ✅ Merged |
| `FileBrowser` | `src/gui/file_browser.hpp/.cpp` | `src/gui/file_browser.hpp` (header-only) | ✅ Done |
| `TitleBrowser` | `src/gui/title_browser.hpp/.cpp` | `src/gui/catalog/title_browser.hpp` (header-only) | ✅ Done |
| `CatalogPipeline` | `src/catalog/catalog_pipeline.hpp/.cpp` | `src/gui/catalog/catalog_pipeline.hpp` (header-only) | ✅ Done |
| `CatalogStore` | `src/catalog/catalog_store.hpp/.cpp` | `src/gui/catalog/catalog_store.hpp` (header-only) | ✅ Done |
| `ScanRootManager` | `src/catalog/scan_root_manager.hpp/.cpp` | `src/gui/scan_root_manager.hpp` (header-only) | ✅ Done |
| `AutoHideMenuBar` | `src/gui/auto_hide_menu_bar.hpp/.cpp` | Integrated into `session_gui.cpp` | ✅ Merged |
| `EmulationHUD` | `src/gui/emulation_hud.hpp/.cpp` | Integrated into `session_gui.cpp` | ✅ Merged |

**Implementation notes:**
- All new UI components are header-only (.hpp) rather than .hpp/.cpp split,
  following the project convention for side-effect-free GUI code.
- `ProbeResultPopup`, `AutoHideMenuBar`, and `EmulationHUD` were merged into their
  parent components rather than standalone files — the complexity did not warrant
  separate translation units.
- Catalog files live under `src/gui/catalog/` (not `src/catalog/`) to keep all GUI
  code co-located.
- `ScanRootManager` lives in `src/gui/` alongside the launcher components that use it.
- All catalog code is guarded by `#ifndef CERMU_NO_SQLITE` for builds without SQLite3.

---

## 2. Overall Layout

### 2.1 Launcher layout

The launcher is shown instead of the emulation screen when no system is loaded, or
when the user invokes File → Switch System / Browse. Emulation is paused while the
launcher is open. Scroll position and selected system are preserved when returning
to the launcher within the same application lifetime.

The right panel has three zones. Zone A and Zone C are conditional; Zone B is always
present.

```
┌─ Top bar ──────────────────────────────────────────────────────────────────────┐
│ ◈ CERMU │ [All][Home][Console][Arcade][Other]  Maker▾ │ [File] [Title] │ N/M  │
├─ System list (248 px) ───┬─ Content panel (flex) ──────────────────────────────┤
│                          │ [A] System header + config strip                    │
│  [search input]          │     (hidden when no system selected)                │
│                          ├─────────────────────────────────────────────────────┤
│  Row: Name | Meta        │ [B] File browser  OR  Title browser                 │
│  Row: …                  │     (always visible; toggled by top bar)            │
│                          │                                                     │
│  (system selection not   ├─────────────────────────────────────────────────────┤
│   required to browse)    │ [C] Probe result bar                                │
│                          │     (visible only after probe, no system selected)  │
├──────────────────────────┴─────────────────────────────────────────────────────┤
│ Status bar: context-sensitive key hints                                        │
└────────────────────────────────────────────────────────────────────────────────┘
```

View toggle (File / Title) lives in the top bar. The left panel shifts contextually
in title browser mode to show filter facets (system, maker, year range, free text)
rather than the system list.

### 2.2 Emulation layout

```
┌─ auto-hide bar (F12 or dwell at top edge) ─────────────────────────────────────┐
│ ◈ CERMU  │  File  Library  Hardware  View  │                    [Graph ⬚]      │
└────────────────────────────────────────────────────────────────────────────────┘

┌─ emulated screen ──────────────────────────────────────────────────────────────┐
│                                                                                 │
│                                              ┌─ HUD (draggable, always on) ──┐ │
│                                              │ ⬤⬤○○  6510  98%              │ │
│                                              │ PORT1  PORT2                  │ │
│                                              └───────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────── ┘

  floating chip panel (optional, draggable, closeable)
┌─ VIC-II ──────────────┐
│  [visualization]      │
└───────────────────────┘
```

Window title during emulation: `CERMU — Commodore 64 — Turrican II.d64`

---

## 3. Color Tokens

All colors are defined as named constants in `LauncherTheme`. Never use inline
literals. The manufacturer color table maps from `SystemDescriptor.maker` substring
to accent color at init time; new systems added via `REGISTER_SYSTEM` pick up a
match automatically or fall back to a neutral accent.

```cpp
namespace launcher_theme {

    // Backgrounds
    inline constexpr ImVec4 kWindowBg           = {0.027f, 0.031f, 0.055f, 1.0f};
    inline constexpr ImVec4 kBarBg              = {0.020f, 0.027f, 0.063f, 1.0f};
    inline constexpr ImVec4 kLeftPanelBg        = {0.024f, 0.031f, 0.063f, 1.0f};
    inline constexpr ImVec4 kSystemRowSelected  = {0.047f, 0.110f, 0.180f, 1.0f};
    inline constexpr ImVec4 kFileRowSelected    = {0.039f, 0.110f, 0.188f, 1.0f};
    inline constexpr ImVec4 kFileRowHover       = {0.035f, 0.047f, 0.094f, 1.0f};
    inline constexpr ImVec4 kSearchInputBg      = {0.035f, 0.047f, 0.110f, 1.0f};

    // Borders
    inline constexpr ImVec4 kPanelBorder        = {0.055f, 0.078f, 0.133f, 1.0f};
    inline constexpr ImVec4 kPanelBorderFocus   = {0.102f, 0.196f, 0.322f, 1.0f};
    inline constexpr ImVec4 kFilterActiveBorder = {0.102f, 0.188f, 0.314f, 1.0f};

    // Text
    inline constexpr ImVec4 kTextPrimary        = {0.867f, 0.933f, 1.000f, 1.0f};
    inline constexpr ImVec4 kTextSecondary      = {0.753f, 0.816f, 0.894f, 1.0f};
    inline constexpr ImVec4 kTextMuted          = {0.416f, 0.494f, 0.596f, 1.0f};
    inline constexpr ImVec4 kTextDimmed         = {0.216f, 0.298f, 0.392f, 1.0f};

    // Accents
    inline constexpr ImVec4 kAccentTeal         = {0.180f, 0.769f, 0.627f, 1.0f};
    inline constexpr ImVec4 kAccentBlue         = {0.353f, 0.682f, 0.910f, 1.0f};

    // Config strip
    inline constexpr ImVec4 kCfgBgDefault       = {0.031f, 0.047f, 0.110f, 1.0f};
    inline constexpr ImVec4 kCfgBgChanged       = {0.039f, 0.078f, 0.149f, 1.0f};
    inline constexpr ImVec4 kCfgBorderDefault   = {0.067f, 0.094f, 0.149f, 1.0f};
    inline constexpr ImVec4 kCfgBorderChanged   = {0.102f, 0.188f, 0.314f, 1.0f};
    inline constexpr ImVec4 kCfgTextDefault     = {0.376f, 0.471f, 0.596f, 1.0f};
    inline constexpr ImVec4 kCfgTextChanged     = {0.604f, 0.784f, 0.933f, 1.0f};

    // Probe result bar (Zone C)
    inline constexpr ImVec4 kProbeBarBg         = {0.031f, 0.059f, 0.047f, 1.0f};
    inline constexpr ImVec4 kProbeBarBorder     = {0.071f, 0.180f, 0.133f, 1.0f};
    inline constexpr ImVec4 kProbeMatch         = {0.180f, 0.769f, 0.627f, 1.0f};
    inline constexpr ImVec4 kProbeAmbiguous     = {0.780f, 0.600f, 0.133f, 1.0f};
    inline constexpr ImVec4 kProbeNoMatch       = {0.800f, 0.267f, 0.267f, 1.0f};

    // Archive breadcrumb segment
    inline constexpr ImVec4 kBreadcrumbArchive  = {0.400f, 0.300f, 0.600f, 1.0f};

} // namespace launcher_theme
```

---

## 4. Data Model

### 4.1 SystemDescriptor additions

`SystemDescriptor` requires four new optional display-only fields. These have no
effect on emulation.

```cpp
struct SystemDescriptor {
    // ... existing fields ...
    const char* maker;        // "Commodore", "Nintendo", … (nullptr = not displayed)
    int         year;         // 1982; 0 = unknown
    const char* cpu_summary;  // "MOS 6510" — short display string
    SystemType  type;         // Home, Console, Arcade, Other
};

enum class SystemType : uint8_t { Home, Console, Arcade, Other };
```

### 4.2 SystemProbeResult

```cpp
struct SystemProbeResult {
    float               confidence;     // 0.0–1.0
    SystemConfiguration configuration; // optimal config determined from file evidence
};
```

`probe_file(vfs_path)` returns `std::vector<SystemProbeResult>` sorted by
descending confidence. All paths passed to `probe_file()` are VFS paths,
including real filesystem paths (which are valid VFS paths) and paths into
open archives. No path type special-casing exists.

The probe aggregates across all accessible content under the given path. If every
file agrees on the same system, one result is returned. If files disagree, multiple
results are returned. A single result means the path is a launchable unit.

### 4.3 SharedConfigStore

In-session only. No persistence. Useful for carrying preferences (PAL/NTSC,
joystick attachment) across system switches within one session.

```cpp
class SharedConfigStore {
public:
    void set_region(VideoStandard standard);
    std::optional<VideoStandard> get_region() const;

    void set_peripheral(const std::string& periph_id, bool enabled);
    std::optional<bool> get_peripheral(const std::string& periph_id) const;

    void set_custom(const std::string& option_id, const std::string& value);
    std::optional<std::string> get_custom(const std::string& option_id) const;

    // Memory options are system-specific — keyed by short_name
    void set_memory(const std::string& system_short_name, int index);
    std::optional<int> get_memory(const std::string& system_short_name) const;

    // Populate a SystemConfiguration from stored preferences
    SystemConfiguration build_config(
        const std::string& system_short_name,
        const HardwareTraits& traits) const;

    // Record all choices from a config back into the store
    void record_config(
        const std::string& system_short_name,
        const HardwareTraits& traits,
        const SystemConfiguration& config);

private:
    std::optional<VideoStandard>         region_;
    std::map<std::string, bool>          peripherals_;
    std::map<std::string, std::string>   custom_;
    std::map<std::string, int>           memory_per_system_;
};
```

### 4.4 Configuration authority

```
Probe confidence ≥ threshold   →  use probe's SystemConfiguration directly
                                   SharedConfigStore is NOT applied on top
                                   Fields sourced from probe get an "auto" indicator
                                   in Zone A

Probe confidence < threshold   →  use SharedConfigStore.build_config() for the
                                   top candidate system

System-first, no probe         →  SharedConfigStore.build_config() only

User changes any config value  →  record_config() called immediately regardless
                                   of whether initial values came from probe or store
```

Confidence threshold is a `constexpr float kSingleMatchThreshold = 0.85f` in
`LauncherPanel`. Adjustable without touching logic.

---

## 5. File Browser (Zone B — File view)

### 5.1 Always accessible

The file browser is visible regardless of whether a system is selected. It is never
blocked behind system selection.

### 5.2 Path bar

Breadcrumb-style. Each segment is clickable to navigate up. Segments that represent
archive containers (ZIP, D64, T64, etc.) are rendered in `kBreadcrumbArchive` colour
with a container icon so the user always knows when they are inside a VFS container
rather than a real directory.

The `⌂ default` button:
- When a system is selected: navigates to the system's `data_folder` via
  `PathDiscovery`.
- When no system is selected: navigates to the configured global ROMs root.
- When no ROMs root is configured: opens the scan roots configuration panel
  (same flow as first-run, see §12.1).

### 5.3 File list

Three columns: Name (flex), Size (70 px), Type (60 px). Directories appear first,
sorted alphabetically. Files sorted alphabetically by default. Both Name and Size
columns are sortable (click header to toggle asc/desc).

A search input above the list filters by filename substring within the current
directory. The `/` hotkey focuses this input when the file browser panel is active.

Format filter combo: when a system is selected, populated from
`descriptor.supported_formats`. When no system is selected, populated from the
union of all registered systems' `supported_formats`.

### 5.4 Navigation behaviour

- Single-click directory: select (no navigation)
- Single-click file: select; if no system is selected, trigger probe (§7)
- Double-click directory or container with multiple probe results: navigate into
- Double-click file or container with single probe result and no further navigable
  content: launch immediately
- Double-click container with single probe result but navigable content inside:
  navigate into (user selects specific entry)
- `Backspace` (no text input focused): navigate to parent

The double-click launch rule in full:

```
launch immediately on double-click if and only if:
    probe_file(P) returns exactly one result
    AND P contains no further navigable content
        (no sibling files, no subdirectories containing files)
otherwise → navigate into P
```

### 5.5 Empty state

```
No files found in this directory
[Browse…]  ← native OS folder picker; sets global ROMs root if none configured
```

### 5.6 Remembering paths

`last_file_path_` preserved across browser sessions. Per-system default paths
derived from `data_folder` via `PathDiscovery`.

---

## 6. Title Browser (Zone B — Title view)

The title browser is a library view showing cataloged titles as an aesthetic card
grid. It requires the catalog pipeline (§13) to have run at least partially. It is
always accessible regardless of system selection; the left panel shifts to filter
facets in this mode.

### 6.1 Title cards

Each card shows: title name, system badge(s) for each available system, region /
revision count badge, thumbnail placeholder (cover art if available, procedural CRT
if not). Cards for titles available on multiple systems show one badge per system,
each as a separate launch target.

### 6.2 Filter facets (left panel in title browser mode)

- Free text search (name tokens)
- System multi-select
- Maker multi-select
- Year range slider
- Region filter

System selection in the left panel is not a prerequisite for browsing. The facets
are independent filters.

### 6.3 Selection and launch

- Select title with one launchable version: shows config strip in Zone A, launches
  on Enter or double-click
- Select title with multiple versions: expands inline variant picker (system, region,
  format, revision) before launching
- Double-click title with single unambiguous version: launches immediately

### 6.4 Scan progress state

While the catalog pipeline is running, a progress bar appears at the top of the
title browser:

```
Scanning /home/user/roms…   2,847 / ~12,000 files   [Pause] [×]
```

Cards appear progressively as probe results arrive. The browser is usable during
scanning; launching from a partial catalog is allowed. Pause reclaims CPU/IO
resources without discarding progress.

### 6.5 Empty / first-run state

When no catalog exists and no scan roots are configured:

```
No library configured
[Set up scan roots…]  ← opens scan roots configuration (§12.1)
```

When scan roots are configured but scan has not run yet, scanning starts
automatically on first entry into the title browser.

---

## 7. Probe Flow

### 7.1 Trigger conditions

A probe is triggered when:
- User single-clicks a file in the file browser and no system is currently selected
- A file path arrives via CLI argument
- A file is dropped onto the launcher
- A file is dropped onto a running emulation instance (see §9)

A probe is NOT triggered when:
- A system is already selected (single-click just selects the file)
- The selected entry is a directory or archive container (probe fires on enter,
  not select, for containers)

### 7.2 Probe states

```cpp
enum class ProbeState { None, Probing, SingleMatch, Ambiguous, NoMatch };
```

State transitions:

```
None → Probing → SingleMatch  (one result, confidence ≥ kSingleMatchThreshold)
               → Ambiguous    (multiple results, or top result < threshold)
               → NoMatch      (empty result or all confidence < 0.30)
```

`kNoMatchThreshold = 0.30f` — configurable `constexpr`.

`probe_file()` is called synchronously for now. If profiling reveals it is slow for
large archives, move to a background thread with a `Probing` spinner in Zone C.

### 7.3 Probe result cleared when

- User navigates to a different file
- User navigates to a different directory
- User selects a system in the left panel (switches to system-first mode,
  probed file remains pre-selected in the browser)
- User presses Escape in file browser (clears selection and probe result)
- New path arrives via drop or CLI

### 7.4 Zone C — probe result bar

Zone C is a fixed-height bar (≈ 44 px) pinned at the bottom of the right panel,
above the status bar. Visible only when `probe_state != None`.

**SingleMatch:**
```
✓  Commodore 64      PAL · 1541 (fast) · Joystick port 2
```
Once Zone A is visible (populated from probe), Zone C reduces to a status strip —
system name + confidence indicator only, no Launch button. The authoritative Launch
action is in Zone A where the config strip is also accessible.

**Ambiguous:**
```
⚠  3 systems match — Commodore 64 (91%)  VIC-20 (62%)  C16 (41%)     [▼ Choose…]
```
Zone A is NOT populated in ambiguous state. Selecting a candidate from Zone C or
from `ProbeResultPopup` populates Zone A with that system's config.

**NoMatch:**
```
✗  Format not recognised      [Choose system manually…]
```
Link focuses the system list left panel.

### 7.5 ProbeResultPopup

A non-modal `ImGui::BeginPopup` (rest of UI remains accessible). Contains:
- File name being probed (header)
- Ranked list of `SystemProbeResult` entries: system name, confidence bar,
  config summary, `[Launch ▶]` per row
- "Launch with any system" section: system combo (all registered systems) +
  config strip for the selected system

Dismissed by Escape or click outside.

### 7.6 Config strip interaction during probe

When `SingleMatch` and Zone A becomes visible:
- Config strip pre-filled from `probe.configuration`
- Fields sourced from probe show a small "auto" indicator
- User may adjust any value; `record_config()` called on any change
- Values changed by user lose the "auto" indicator

### 7.7 File-first launch — complete flow

```
1. Launcher open, no system selected
2. File browser shows union of all supported formats
3. User navigates to game.d64, single-clicks
4. probe_file("game.d64") → [{ C64, 0.94, {PAL, 1541-fast, joy2} }]
5. ProbeState = SingleMatch
6. Zone C shows: "✓ Commodore 64  PAL · 1541 (fast) · Joystick port 2"
7. Zone A appears: C64 header, config strip pre-filled from probe
8. User optionally changes "Drive" to "1541 (accurate)"
   → record_config("C64", ...) called
9. User presses Enter or double-clicks game.d64
10. switch_system("C64", config, "game.d64") called
11. Launcher closes, emulation starts
```

---

## 8. System Header & Config Strip (Zone A)

Zone A is visible when:
- A system is selected in the left panel, OR
- A `SingleMatch` probe result has been accepted (explicitly or by Zone A appearing)

Zone A is hidden when no system is selected and probe state is None or Ambiguous.

### 8.1 Header content

- System name (`descriptor.name`), year, CPU summary, maker badge, type badge
- Config strip: horizontal row of controls derived from `HardwareTraits`

### 8.2 Config strip rendering

```
For each video_standard_configs entry   →  "Region" combo
For each memory_options entry           →  "Memory" combo
For each peripheral_options entry       →  Checkbox / toggle
For each custom_options entry           →  Combo with choices
```

Border colour: `kCfgBorderChanged` when value differs from default,
`kCfgBorderDefault` otherwise. Fields sourced from a high-confidence probe show an
"auto" indicator that clears on manual edit.

### 8.3 Action buttons

- `★` favourite toggle
- `⚙` settings (placeholder)
- `▶ Launch`: assembles final `SystemConfiguration`, calls `switch_system()` with
  selected file (if any) or bare boot (if none)

---

## 9. System List (Left Panel)

### 9.1 Population

```cpp
const auto& systems = SystemRegistry::instance().get_systems();
```

Sorted alphabetically by name by default.

### 9.2 Filtering

1. Type filter tabs: All / Home / Console / Arcade / Other → `descriptor.type`
2. Maker combo: populated from unique `descriptor.maker` values
3. Search input: case-insensitive substring across `name`, `short_name`,
   `description`, `maker`, `cpu_summary`, year string, type string

### 9.3 Row display

Each row: system name, year + maker badge (when metadata available), compact
indicator when system has no `supported_formats`.

### 9.4 Selection behaviour

- Single click: select system, populate Zone A, update file browser format filter,
  navigate file browser to system's data folder, clear any probe result
- Double click: select system and move focus to file browser

### 9.5 Context in title browser mode

When title browser is active, left panel shows filter facets instead of the system
list (see §6.2).

---

## 10. Multi-path Input (CLI, Drag-and-Drop)

All path sources use identical logic. All paths are VFS paths.

### 10.1 Input set semantics

```
paths[0]     →  primary: probe, determine system, launch
paths[1..N]  →  secondary: probe to confirm system match, then feed into
                ROM set search and flip list population for the running session
                Files that probe to a different system are silently skipped
```

### 10.2 CLI arguments

Same state machine as launcher file selection:
- One result, leaf path → launch immediately (no UI)
- One result, non-leaf → launch immediately (all content agrees)
- Multiple results → open launcher pre-navigated to the path, user chooses
- No results → open launcher at the path, Zone C shows NoMatch

`paths[1..N]` processed after session starts, confirming each against the launched
system before flip-list addition.

### 10.3 Drop onto the launcher

Navigates the file browser to the dropped path and triggers probe. Behaves
identically to the user navigating there manually.

### 10.4 Drop onto a running emulation instance

```
probe result matches running system  →  append paths[1..N] to flip list silently;
                                         paths[0] appended too if not already running
probe result is a different system   →  pause emulation, show confirm dialog:
                                         "Switch to [System]? Running session will
                                          be lost. [Switch] [Cancel]"
probe returns no match               →  pause emulation, open launcher at dropped path
```

---

## 11. Auto-hide Menu Bar and Emulation HUD

### 11.1 Auto-hide bar trigger

```
mouse not captured   →  F12 toggle  OR  cursor dwell at top N px (400–500 ms)
mouse captured       →  F12 only
```

Mouse capture state (paddle / lightgun / 1351 emulation) suppresses hover trigger.
The bar does not auto-hide while a submenu is open or while keyboard focus is inside
the bar, regardless of cursor position or idle timer.

Slide animation: 120–150 ms ease-out.

Auto-hide behaviour:
- Triggered by hover: auto-hides after ~3 seconds of idle
- Triggered by F12: stays visible until F12 again or explicit dismiss

### 11.2 Menu structure

```
File
    Switch System / Browse    →  opens launcher (pauses emulation)
    Recent                    →  submenu of recently launched titles
    ────────────────
    Quit

Library
    Scan roots…               →  opens scan roots manager
    Rescan all                →  full incremental rescan of all roots
    Rescan missing only       →  probes only file_missing entries
    Process orphans…          →  manual orphan cleanup
    ────────────────
    Switch to file browser
    Switch to title browser

Hardware
    [chip name]               →  opens/focuses floating chip panel
    Hide all panels

View
    Fullscreen        F11
    Menu bar          F12
    Performance graph         →  toggles graph overlay (extends HUD)
    HUD corner        ▾       →  submenu: Top-left / Top-right /
                                           Bottom-left / Bottom-right
```

### 11.3 Keyboard shortcuts in emulation

| Key | Action |
|---|---|
| F11 | Toggle fullscreen |
| F12 | Toggle auto-hide menu bar |
| Escape | Dismiss topmost UI element (chip panel → bar → nothing) |

Escape hierarchy: if a chip panel has focus, dismiss it. If the bar is visible and
no chip panel is focused, hide the bar. If nothing is open, Escape does nothing at
the host level (does not interfere with emulated systems since emulated Escape goes
through the keyboard matrix).

### 11.4 Emulation HUD

Persistent semi-transparent corner overlay showing: port status icons, active chip
names, CPU load percentage. Position is draggable to any of the four corners;
chosen corner is remembered in `SharedConfigStore` for the session.

The `[Graph ⬚]` toggle in the auto-hide bar extends the HUD with a performance
history graph overlay. The graph is visually connected to the HUD (same corner,
appears above or below) rather than being an independent element.

### 11.5 Chip panels

Floating `ImGui` windows, one per chip. Opened from the Hardware menu or via the
chip visualization menu entries. Each panel:
- Has its own close button
- Is draggable and resizable
- Can be closed via Escape when focused
- Does not capture F12 key events

A single "Hide all panels" menu entry under Hardware dismisses all open panels.

---

## 12. Scan Root Management

### 12.1 First-run / scan roots not configured

When the title browser is opened and no scan roots are configured, a non-blocking
panel appears:

```
Set up your ROM library
Cermu searches these folders for games. Select the ones that apply:

☑  /home/user/ROMs          (found, contains files)
☑  /home/user/roms           (found, contains files)
☐  /mnt/nas/retro            (found, empty)
☐  /home/user/.local/share/retroarch/roms  (found)

[Add folder…]   [Skip for now]   [Start scanning]
```

Pre-population sources (in priority order):
1. Parent directories of any path passed as CLI argument
2. Recently visited directories in the file browser
3. Known paths per host environment (see §12.2)

The panel is dismissible ("Skip for now") and does not block file browser use. It
reappears each time the title browser is opened with no scan roots defined.

### 12.2 Host environment pre-population

```
Linux    ~/ROMs  ~/roms  ~/Emulation/roms  $XDG_DATA_HOME/*/roms
         Common emulator data dirs (RetroArch, MAME, VICE)
         Flatpak sandbox paths

Windows  %USERPROFILE%\ROMs  %USERPROFILE%\Documents\ROMs
         Common emulator install paths (RetroArch, MAME, WinUAE)
         All drive roots if collection is on a separate volume
```

### 12.3 Scan roots persistence

Stored in a dedicated config file (TOML), separate from the catalog database and
`SharedConfigStore`. Written immediately on any change.

```toml
[scan_roots]
paths = [
    "/home/user/roms",
    "/mnt/nas/retro"
]
```

### 12.4 Path removal behaviour

When a root is removed from the list:
- Filesystem watcher stops watching that path immediately
- Background updates skip all entries whose source path falls under the removed root
- Catalog entries are NOT automatically removed — they are marked `source_root_removed`
  (distinct from `file_missing`)

`source_root_removed` is preserved to handle temporary unmounts. The user may have
de-listed a root because a drive was offline, not because the content is gone.

---

## 13. Cataloging Pipeline

### 13.1 Catalog entry states

```
present          file exists at known path, probe result current
stale            file mtime changed since last probe — re-probe queued
file_missing     root configured, file not found at last check
root_removed     source root was de-listed by user
orphaned         file_missing grace period expired without recovery
```

### 13.2 Pipeline phases

```
1. Discovery     walk all configured scan roots, enumerate files     fast, I/O-bound
2. Probing       probe_file() on each discovered file                slow, CPU-bound
3. Grouping      cluster probe results into titles                   fast, in-memory
4. Enrichment    match against TOSEC/No-Intro, fetch cover art       optional, network
```

Phases pipeline — grouping starts as soon as first probe results arrive. The title
browser displays partial results throughout. Probing is embarrassingly parallel;
a thread pool drains a work queue and posts results back to the main thread in
batches.

### 13.3 Catalog persistence

SQLite database, separate from `SharedConfigStore` and scan roots config. Keyed by
VFS path + mtime. On subsequent launches, only files whose mtime has changed since
the last scan are re-probed. Fingerprints (see §14.2) are stored alongside probe
results.

### 13.4 Grouping logic

A title is a cluster of files representing the same game.

**Variant level** (collapses into one card with version badges):
- Same game, different format (.d64 + .tap + .prg)
- Same game, different region (PAL + NTSC)
- Same game, different revision (v1.0, v1.1, crack)
- Same game on different systems (cross-system)

Grouping heuristics: filename similarity after stripping known suffixes (region
codes, revision markers, format tags); probe-confirmed system agreement within a
directory; hash-database matching for exact identification where available. Ungroupable
files appear as individual cards. Wrong groupings can be manually corrected.

### 13.5 Scan triggers

- First entry into title browser with no catalog: start immediately
- Explicit "Rescan all" / "Rescan missing only" from Library menu
- Lightweight staleness check on launcher open: compare directory mtime against
  last scan timestamp; trigger incremental rescan silently if stale
- Phase 6c: filesystem watch via `inotify` (Linux) / `ReadDirectoryChangesW`
  (Windows) for live updates on configured paths

### 13.6 Progress notification

When a background scan auto-updates paths for more than 15 entries simultaneously
(e.g. a bulk folder relocation was detected), a non-blocking notification is shown:

```
Library: 847 entries updated — /mnt/nas/roms relocated to /mnt/nas2/roms
```

Below 15 entries, silent update.

---

## 14. Orphan and Relocation Handling

### 14.1 Grace period

An entry transitions from `file_missing` to `orphaned` after:
**2 days OR 15 application launches, whichever comes first.**

Configurable:

```toml
[library]
orphan_grace_days    = 2
orphan_grace_launches = 15
```

Each background update retries fingerprint search (§14.2) for `file_missing`
entries before advancing the grace period clock.

### 14.2 Content fingerprint

Computed at probe time and stored in the catalog:

```
fingerprint = file_size
            + crc32(first fingerprint_head_kb KB)
            + crc32(last  fingerprint_tail_kb KB)
```

Fast, cheap, low false-positive rate for ROM files. Survives path changes.

```toml
[library]
fingerprint_head_kb   = 64
fingerprint_tail_kb   = 64
fuzzy_match_threshold = 0.80
```

### 14.3 Detection algorithm

Rename/relocation detection runs at two points:

**Background (silent, per missing file):**

```
file transitions to file_missing
    → search all scan roots for matching fingerprint
    → found any root   → path updated silently, entry → stale
    → not found        → stay file_missing, start grace period clock
```

**Folder-level optimisation:**

```
multiple files missing with a common path prefix
    → look for a directory in scan roots with the same structure
      (≥ 80% of missing fingerprints present)
    → found → folder relocation confirmed → bulk-update all affected entries
    → not found → fall back to per-file fingerprint search
```

**Orphan processing (user-requested, thorough):**

```
fingerprint search across all scan roots
folder structure matching
fuzzy name matching (threshold configurable, default 0.80)
    → match found     → present to user for confirmation → stale on accept
    → no match found  → user chooses Remove or ignore
```

### 14.4 Fuzzy name matching

Supplementary signal for files whose content changed (different dump, re-encoded
image). Always requires explicit user confirmation before path update.

```
missing: "Turrican II (EUR) [!].d64"
candidate: "Turrican II (Europe).d64"
stripped tokens: "turrican ii" == "turrican ii" → probable match, prompt user
```

### 14.5 Orphan processing UI

Accessible via Library → Process orphans…

```
Orphaned entries (47)                           [Remove all] [Search all]
─────────────────────────────────────────────────────────────────────────
✗ Turrican II.d64          last seen /mnt/nas/c64/    [Remove] [Search…]
✗ The Last Ninja.d64       last seen /home/user/roms/ [Remove] [Search…]
…
```

- **Remove**: deletes catalog entry. No per-entry confirmation; the list itself is
  the confirmation step. Bulk remove gets one confirm dialog.
- **Search…**: opens a scoped file browser to locate the file manually. On
  confirmation, catalog entry path is updated and entry returns to `stale`.
- **Search all**: runs fingerprint + fuzzy search automatically across all scan
  roots for every orphaned entry, presents matches for confirmation before
  updating paths. Handles bulk collection relocation.
- **Remove all**: confirm dialog then bulk-deletes all orphaned entries.

---

## 15. Keyboard Navigation

### 15.1 Launcher — global (no text input focused)

| Key | Action |
|---|---|
| `↑/↓` | Navigate system list (left panel focused) or file list (right panel focused) |
| `Tab` | Toggle focus between system list and file/title browser |
| `Enter` | System list: move focus to file browser. File browser, file selected, probe ready: launch. File browser, dir/container: navigate in. |
| `/` | Focus search input of active panel |
| `Esc` | File browser focused: return focus to system list, clear probe result. ProbeResultPopup open: close popup. |
| `Backspace` | File browser focused, no text input: navigate to parent |

### 15.2 Launcher — text inputs

| Key | Action |
|---|---|
| `Enter` or `↓` | Exit input, return focus to list |
| `Esc` | Clear input, exit input |

### 15.3 Launcher — probe states

| Key | Context | Action |
|---|---|---|
| `Enter` | File browser, SingleMatch probe ready | Launch with probe result |
| `Enter` | File browser, Ambiguous probe | Open ProbeResultPopup |
| `Esc` | Probe result shown | Clear probe result, deselect file |
| `Esc` | ProbeResultPopup open | Close popup |

### 15.4 Emulation

| Key | Action |
|---|---|
| `F11` | Toggle fullscreen |
| `F12` | Toggle auto-hide menu bar |
| `Esc` | Dismiss topmost UI element (chip panel → bar → nothing) |

### 15.5 Status bar hints

The status bar updates to show relevant hints for the current context:

| Context | Hints |
|---|---|
| System list focused | `↑↓` Navigate · `↵` Open files · `Tab` Switch panel · `/` Search |
| File browser focused, no probe | `↑↓` Navigate · `↵` Select & probe · `/` Search · `Backspace` Up |
| File browser focused, SingleMatch | `↵` Launch · `Esc` Clear · `Tab` Switch panel |
| File browser focused, Ambiguous | `↵` Choose… · `Esc` Clear |
| ProbeResultPopup open | `↵` Launch selected · `Esc` Close |
| Search input focused | `↵/↓` Jump to list · `Esc` Clear |

---

## 16. Rendering Approach

### 16.1 Window setup

```cpp
ImGui::SetNextWindowPos({0, 0});
ImGui::SetNextWindowSize(io.DisplaySize);
ImGui::Begin("##launcher", nullptr,
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus);
```

### 16.2 Panel split

```cpp
ImGui::BeginChild("##syspanel", {248, contentHeight}, false);
// system list or filter facets
ImGui::EndChild();
ImGui::SameLine();
ImGui::BeginChild("##contentpanel", {0, contentHeight}, false);
// Zone A (conditional) + Zone B + Zone C (conditional)
ImGui::EndChild();
```

### 16.3 Zone C rendering

```cpp
if (probe_state_ != ProbeState::None) {
    float bar_h = 44.0f;
    ImGui::SetCursorPosY(available_height - bar_h);
    render_probe_bar();
}
```

### 16.4 ProbeResultPopup

```cpp
if (show_probe_popup_) {
    ImGui::SetNextWindowPos(popup_anchor_, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({520, 360}, ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##probe_result")) {
        render_probe_popup();
        ImGui::EndPopup();
    }
}
```

### 16.5 Auto-hide bar

```cpp
float slide_target = bar_visible_ ? 0.0f : -bar_height_;
bar_offset_ = ImLerp(bar_offset_, slide_target, slide_speed_ * io.DeltaTime);
ImGui::SetNextWindowPos({0, bar_offset_});
ImGui::SetNextWindowSize({io.DisplaySize.x, bar_height_});
```

`slide_speed_` calibrated to give 120–150 ms effective slide duration.

---

## 17. Implementation Phases

### Phase 1 — Foundation ✅
*Commit: `0f9e3159`*
- Add `SystemType`, `maker`, `year`, `cpu_summary` to `SystemDescriptor`
- Populate new fields in all 62 existing system registrations (~35 files)
- Create `LauncherTheme` with all color tokens
- Create `SharedConfigStore` (in-session, no persistence)

### Phase 2 — System List Panel ✅
*Commit: `e8e72eda`*
- Create `LauncherPanel` with system list, filtering (type, maker, search)
- Config strip rendering from `HardwareTraits`
- `SharedConfigStore` integration
- Zone A system header with maker accent badges

### Phase 3 — File Browser ✅
*Commit: `c91342f9`*
- `FileBrowser` component: VFS-based directory listing, path bar with archive
  segment colouring, sortable columns, filename search
- Format filtering (system-scoped and global)
- Container navigation via VFS (transparent archive browsing)
- Path persistence (`last_file_path_`)
- Probe flow integrated directly — `ProbeResultPopup` merged into Zone C probe bar
  rather than a separate popup component

### Phase 4 — Integration ✅
*Commit: `23bfc7ba`*
- Keyboard navigation (arrow keys, Tab panel switching, Enter to launch/navigate)
- Status bar with context-sensitive key hints
- Drag-and-drop onto launcher (directory, archive, and file handling)
- Focus panel tracking between system list and file browser

*Adjustment: `SystemSelectionDialog` and `CermuFileDialog` are preserved alongside
the launcher rather than fully replaced — the old dialogs still function as fallback
paths. Full removal deferred to avoid regressions.*

### Phase 5 — Emulation UI ✅
*Commits: `a5343ed4`, `845471af`, `e51a3231`, `4e02b154`, `85bacb2d`, `f7aaede4`*
- Auto-hide menu bar in fullscreen: F12 toggle + cursor dwell trigger + slide
  animation via viewport position hack. Implemented directly in `session_gui.cpp`
  rather than a separate `AutoHideMenuBar` component.
- Emulation HUD: semi-transparent corner overlay with system name, speed %,
  FPS/frame time, port status icons. Corner selectable via View menu.
  Implemented as `render_emulation_hud()` in `session_gui.cpp` rather than a
  separate `EmulationHUD` component.
- Escape key hierarchy (chip panels → menu bar → nothing)
- Library menu placeholder with Scan roots…, Rescan all, Process orphans…
- Cursor visibility: visible over UI windows in fullscreen, hidden over emulated
  display after idle timeout
- "Hide all panels" in Hardware menu

*Additional display enhancements not in original plan:*
- Display zoom (0.25x–4.0x) and X/Y pan sliders in both Screen menu and Display
  Settings dialog
- Color temperature effect in CRT shader (blackbody tint normalized to 6500K D65)
- CRT toggle with proper panel swap (CRTPanel ↔ DirectPanel) — toggling off no
  longer causes black screen
- Display preset filtering by video signal compatibility (port type → accepted
  signals check)
- Conditional CRT-only sliders (curvature, scanline gap, dot pitch hidden for
  LCD/LED displays)
- Reset buttons positioned above sliders
- Screen window background cleared (no stale content on zoom-out)
- Launcher UI zoom: proportional left panel width scaling via `sqrt(ui_scale_)`,
  default zoom raised from 1.0x to 1.5x for better readability

### Phase 6a — Catalog Pipeline ✅
*Commits: `14710460`, `d3cbb77d`, `8c924347`*
- `ScanRootManager` (`src/gui/scan_root_manager.hpp`): TOML persistence
  (`~/.config/cermu/scan_roots.toml`), host environment discovery (Linux/Windows
  common paths), setup panel UI, manage dialog, wired into Library menu
- First-run scan root setup overlay rendered in launcher when no roots configured
- `CatalogStore` (`src/gui/catalog/catalog_store.hpp`): SQLite-backed persistence
  with entry states (present, stale, file_missing, root_removed, orphaned),
  fingerprint storage, title grouping queries
- `CatalogPipeline` (`src/gui/catalog/catalog_pipeline.hpp`): async discovery →
  probe → group pipeline with atomic progress counters, cancel support
- SQLite3 dependency added to CMakeLists.txt with pkg-config fallback and
  `CERMU_NO_SQLITE` graceful degradation

### Phase 6b — Title Browser ✅
*Commit: `8c924347`*
- `TitleBrowser` (`src/gui/catalog/title_browser.hpp`): card grid view with search,
  scan progress bar, empty/first-run state, variant picker
- File/Title view mode toggle in launcher top bar
- Right panel routes between file browser and title browser based on view mode
- `kCardBgDefault` color constant added to launcher theme

*Adjustment: The title browser shares the existing left panel (system list) rather
than replacing it with separate filter facets as originally planned. System selection
in the left panel acts as a filter for both file browser and title browser views.
`TitleBrowser::render()` takes `CatalogStore&` and `CatalogPipeline&` directly.*

### Phase 6c — Orphan and Relocation Handling ❌ Not started
- Content fingerprint computation and storage
- Background per-file fingerprint search on `file_missing` transition
- Folder-level relocation detection (common prefix, 80% fingerprint match)
- Grace period state machine (2 days / 15 launches, configurable)
- Orphan processing UI (Library → Process orphans…)
- Fuzzy name matching with user confirmation
- Bulk relocation notification (threshold: 15 entries)

*Note: The `CatalogStore` schema already includes fingerprint fields and entry
state tracking. The orphan state machine and relocation detection logic need to
be built on top of the existing infrastructure.*

### Phase 6d — Filesystem Watch ⚠️ Partial
*Commits: `db83db5c` (file watcher utility + file browser integration)*
- `FileWatcher` utility (`src/utils/file_watcher.hpp`): standalone inotify-based
  directory monitor (Linux), non-blocking poll, no-op stub on other platforms
- File browser auto-rescan: `FileBrowser` watches the current directory and
  rescans on change events with a 30-frame debounce

*Still missing:*
- Watching scan root directories to trigger incremental catalog rescans (the
  utility exists, but `ScanRootManager` / `CatalogPipeline` are not wired to it)
- `ReadDirectoryChangesW` (Windows) — currently returns `false` on non-Linux
- Stat-based fallback polling for platforms without native filesystem events

### Phase 6e — Enrichment (optional) ❌ Not started
- TOSEC / No-Intro database matching
- Cover art fetching and caching

---

## 18. Open Questions

1. **Fonts**: Bundle Sora + JetBrains Mono, or use system fallbacks /
   ImGui default? Affects Phase 1 scope and distribution packaging.

2. **Favourites**: Store in `SharedConfigStore` (in-session only) or a
   separate user prefs file (persistent)? Not blocking for initial phases.

3. **Cover art rights and sourcing**: Phase 6e enrichment needs a defined
   source for cover art. Fair-use thumbnail databases (ScreenScraper,
   TheGamesDB) have API terms that need review before integration.

4. **Multi-file launch (multi-disk)**: A ZIP containing disk1.d64 + disk2.d64
   navigates-in under the current rule (multiple files). Should files within
   that ZIP be selectable as a group for ordered multi-disk launch? Deferred,
   but the flip-list infrastructure from multi-path CLI input largely solves
   this already for the common case.

---

## 19. Implementation Audit (2026-03-25, updated 2026-03-28)

Full code audit of the implemented state against this plan. Serves as a reference
for resuming work on remaining phases.

### 19.1 Component status summary

| Component | File(s) | Status | Notes |
|-----------|---------|--------|-------|
| LauncherPanel | `launcher_panel.hpp` | ✅ Done | Keyboard nav, drag-drop, probe bar (Zone C), launch action, config recording |
| LauncherTheme | `launcher_theme.hpp` | ✅ Done | All color tokens + tag badge colors + manufacturer accents |
| SharedConfigStore | `shared_config_store.hpp` | ✅ Done | Region/peripherals/custom/memory storage, `build_config()`, `record_config()` |
| FileBrowser | `file_browser.hpp` | ✅ Done | 5-column layout (Name/Region/Year/Size/Type), search on parsed metadata, region filter pill, tag badges, filesystem watch, VFS archive navigation, strict-weak-ordering sort fix |
| TitleBrowser | `catalog/title_browser.hpp` | ⚠️ Partial | Card grid with accent bars + system badge pills + text wrapping; scan progress bar + empty state done; variant picker UI incomplete |
| CatalogStore | `catalog/catalog_store.hpp` | ✅ Done | SQLite integration, fingerprint storage, entry state tracking, grouping queries, mutex-protected, NULL-safe column reads, `display_title` column with COALESCE fallback |
| CatalogPipeline | `catalog/catalog_pipeline.hpp` | ✅ Done | Async discovery→probe→group pipeline with cancel support; atomic work-queue thread pool (N-1 workers capped at 7); computes `display_title` from `rom_filename::parse()` |
| ScanRootManager | `scan_root_manager.hpp` | ✅ Done | TOML persistence, host environment discovery, setup panel UI |
| FileWatcher | `utils/file_watcher.hpp` | ✅ Done | inotify (Linux) + no-op stub; integrated into FileBrowser |
| RomFilenameParser | `utils/rom_filename_parser.hpp` | ✅ Done | TOSEC/No-Intro/GoodTools parsing; title/region/year/tags/flags extraction |
| Auto-hide menu bar | `session_gui.cpp` | ✅ Done | F12 toggle + cursor dwell + slide animation + idle auto-hide |
| Emulation HUD | `session_gui.cpp` | ✅ Done | System name, speed %, FPS, port status; corner selectable via View menu |
| Escape hierarchy | `session_gui.cpp` | ✅ Done | Chip panels → menu bar → nothing |

### 19.2 Detailed findings — what works

**LauncherPanel** (`launcher_panel.hpp`):
- `handle_keyboard()` implements arrow keys (↑↓), Tab (panel toggle), Enter
  (focus switch / launch), Backspace (navigate up), `/` (focus search)
- `render_status_bar()` displays context-sensitive key hints based on `focus_panel_`
- `handle_drop()` routes directories/archives to navigation, files to probe
- `trigger_probe()` calls `SystemRegistry::instance().identify_system()`
- `launch_selected_system()` calls `config_store_.record_config()` then sets
  `selection_confirmed_ = true`; session_gui calls `switch_system()` on confirm
- Zone C probe bar renders ✓ (SingleMatch) / ⚠ (Ambiguous) / ✗ (NoMatch)

**FileBrowser** (`file_browser.hpp`):
- `scan_directory()` handles both VFS paths (via `VfsFileSystem::ScanDirectory()`)
  and real filesystem (via `std::filesystem::directory_iterator`)
- `parse_entry_metadata()` calls `rom_filename::parse()` to extract title/region/
  year/tags/flags from filenames
- `matches_search()` tests against `parsed_title`, `region`, `year`, and `name`
- Region filter pill cycles through detected regions on click
- Tag badges rendered as colored pills after title text using ImDrawList
- `dir_watcher_.poll_changed()` triggers `scan_directory()` + `apply_filter_and_sort()`
  with 30-frame debounce
- Sort comparator enforces strict weak ordering — all columns return early only
  on unequal primary keys, with a common name-based tie-breaker preventing UB
- `scan_directory()` wraps `std::filesystem::directory_iterator` in try/catch
  for filesystem error resilience
- Column widths: Region 50px, Year 40px, Size 70px, Type 60px, Name flexible

**CatalogStore** (`catalog/catalog_store.hpp`):
- `Fingerprint` struct: `file_size` + `head_crc32` + `tail_crc32`
- `EntryState` enum: Present, Stale, FileMissing, RootRemoved, Orphaned
- `find_by_fingerprint()` exists (schema ready for relocation detection)
- `get_title_groups()` with GROUP BY on `title_key`; uses
  `COALESCE(NULLIF(display_title,''), title_key)` for human-readable titles;
  `get_group_entries()` for per-group variant listing
- `display_title` column stores the parsed human-readable title from
  `rom_filename::parse()`, separate from the normalized `title_key`
- `read_entry()` uses a `col_text()` helper to safely handle NULL returns
  from `sqlite3_column_text()`, preventing `std::string(nullptr)` UB
- All public methods use `std::lock_guard<std::mutex>`

**CatalogPipeline** (`catalog/catalog_pipeline.hpp`):
- Three phases: Discovery (recursive dir walk), Probing (identify_system per file),
  Grouping (assign group_id by title_key)
- `std::atomic<bool> cancel_` checked at each phase boundary
- `PipelinePhase` enum + atomic phase counter for UI polling
- Thread pool with atomic `next_idx` work queue: `min(hardware_concurrency()-1, 7)`
  extra worker threads + main thread participates; all joined before grouping phase
- Computes `entry.display_title` from `rom_filename::parse()` during probing

### 19.3 Remaining work — prioritized

**High value (functional gaps):**

1. **Phase 6c — Orphan & relocation handling** (§14, entirely unbuilt)
   - Grace period state machine: `file_missing` → `orphaned` after 2 days / 15
     launches (configurable in TOML)
   - Background per-file fingerprint search on `file_missing` transition
   - Folder-level relocation detection (common path prefix, ≥80% fingerprint match)
   - Fuzzy name matching with user confirmation
   - Orphan processing UI (Library → Process orphans…): list view with per-entry
     Remove/Search and bulk Remove all/Search all actions
   - Bulk relocation notification (threshold: 15 entries)
   - *Prerequisite:* `CatalogStore` schema already has fingerprint fields and
     entry states. `find_by_fingerprint()` exists.

2. **Scan root watching** (Phase 6d remainder)
   - Wire `FileWatcher` instances to each configured scan root in `ScanRootManager`
   - On change events, trigger `CatalogPipeline` incremental rescan for the
     affected root
   - Currently only the file browser's current directory is watched

3. ~~**Pipeline thread pool** (§13.2)~~ — ✅ Done (2026-03-28). Atomic
   work-queue with N-1 workers (capped at 7) + main thread participation.

4. **Library menu wiring**
   - "Rescan all" / "Rescan missing only" should invoke `CatalogPipeline::start()`
     with appropriate mode
   - "Process orphans…" should open the orphan processing panel
   - Currently these are placeholder menu entries

5. **Title browser variant picker** (§6.3)
   - `expanded_group_` toggle exists but the detailed variant list UI
     (system/region/format/revision selector per card) is incomplete

**Medium value (quality-of-life):**

6. **Windows filesystem watch** — Add `ReadDirectoryChangesW` to `FileWatcher`,
   or implement stat-based fallback polling

7. **ProbeResultPopup detail view** (§7.5) — The plan describes a non-modal popup
   with ranked results, confidence bars, and per-row Launch buttons for the
   Ambiguous case. Currently probe results are shown only in the Zone C bar.

8. **Cover art / thumbnails** (Phase 6e) — Title cards are text-only. Even
   procedural CRT placeholder art (§6.1) would improve the experience.

9. **Font bundling** (open question §18.1) — Sora + JetBrains Mono would improve
   visual polish significantly over the ImGui default font.

10. **Favourites persistence** (open question §18.2) — `SharedConfigStore` is
    in-session only. A persistent user prefs file would enable favourites and
    preferences across sessions.

**Low priority / optional:**

11. **TOSEC / No-Intro database matching** (Phase 6e) — exact identification
    via hash databases, improving grouping accuracy

12. **First-run empty state verification** — §5.5 specifies a [Browse…] button
    opening a native OS folder picker. Verify this path works end-to-end.

13. **Multi-file launch / multi-disk** (open question §18.4) — flip-list
    infrastructure exists from CLI multi-path input; ZIP multi-disk selection
    UX is deferred

### 19.4 Recent visual polish commits (not in original plan)

| Commit | Description |
|--------|-------------|
| `64a6eb03` | Wire `cpu_summary` from chip traits `display_name` (29 system files) |
| `71b7209d` | Filename metadata columns (Name/Region/Year/Size/Type) + region filter |
| `609608e9` | Colored tag badges for ROM tags/flags in file browser |
| `db83db5c` | Auto-rescan file browser on directory changes (inotify) |
| `8bfdd87b` | Fix sort comparator UB causing crash on equal-size entries |
| `7207ff85` | Catalog: display_title, NULL safety, thread pool, enhanced title cards |

---

*End of implementation plan rev 3.*
