# Chip Visualization & Debug UI Proposal

**Date:** 2026-02-20
**Goal:** Make chip debug/visualization available to ALL emulated systems, with a clean, uniform UI

---

## Executive Summary

Currently, only the C64's SID chip has a working debug window (with a chip visualization on the left and register state on the right). This window is toggled from a C64-specific "Chip Debug Windows" submenu — a UI pattern that doesn't exist for VIC-20, C16, NES, or any other system.

This proposal standardizes chip debug as a **first-class cross-system feature** using a single consistent UI pattern: a **"Hardware" menu** plus **per-chip debug windows** that every system can provide.

---

## Design Principles

1. **Consistent across all systems** — same menu structure, same window layout
2. **Unobtrusive** — one top-level menu, no clutter; chip windows are opt-in
3. **Graduated detail** — overview first, drill into individual chips on demand
4. **Follow established patterns** — mirror how connector ports work (clean menu bar items + popups)
5. **Chip implementations own their rendering** — each chip provides its own debug window (already the pattern with `render_debug_window`)

---

## Current State

| System | Chip Debug? | How? |
|--------|-------------|------|
| C64 | SID only | `gui_state_t.show_chip_debug[16]`, toggled from "Chip Debug Windows" submenu inside System menu |
| VIC-20 | None | `render_system_menu_items()` has only "Reset VIC-20" |
| C16/C116/Plus4 | None | `render_system_menu_items()` has only "Reset {name}" |
| NES/Famicom | None | `render_system_menu_items()` has "Reset" + "Eject Cartridge" |
| CHIP-8 | None | — |
| Apple 1 | None | — |

The SID debug window already demonstrates the right rendering pattern:
- Left panel: chip visualization (DIP package with pin states)
- Right panel: register values, voice state, filter settings
- Collapsible sections for detail levels

---

## Proposed Architecture

### A. System-Level Chip Enumeration

Each system exposes its chips through a simple virtual interface on `EmulatedSystem`:

```cpp
struct ChipInfo {
    const char* name;           // "VIC-II (MOS 6569)"
    const char* short_name;     // "VIC-II"
    const char* category;       // "Video" | "Audio" | "CPU" | "I/O" | "Memory" | "Bus"
    void* chip;                 // Opaque pointer to chip instance
    uint16_t base_address;      // I/O base address (e.g., $D400 for SID)
    bool has_debug_window;      // Whether render_chip_debug_window() does anything
    bool has_settings_window;   // Whether render_chip_settings_window() does anything
};

// In EmulatedSystem:
virtual std::vector<ChipInfo> get_chip_info() const { return {}; }
virtual void render_chip_debug_window(int chip_index, bool* show) {}
virtual void render_chip_settings_window(int chip_index, bool* show) {}
```

Each system overrides `get_chip_info()` to list its chips. Systems that haven't implemented debug windows yet return `has_debug_window = false` — the menu item appears grayed out, signaling future work without blocking the UI.

### B. The "Hardware" Menu

A new top-level menu between "System" and "View":

```
File  System  Hardware  View  Screen  Settings  Help
                 │
                 ├─ System Overview...        → opens the board overview window
                 ├─ ─────────────────
                 ├─ CPU
                 │   └─ MOS 6510              → toggles debug window
                 ├─ Video
                 │   └─ VIC-II (MOS 6569)     → toggles debug window
                 ├─ Audio
                 │   └─ SID (MOS 6581)        → toggles debug window
                 ├─ I/O
                 │   ├─ CIA 1 (MOS 6526)      → toggles debug window
                 │   └─ CIA 2 (MOS 6526)      → toggles debug window
                 ├─ Memory
                 │   ├─ RAM (64KB)
                 │   ├─ BASIC ROM
                 │   ├─ KERNAL ROM
                 │   └─ Character ROM
                 └─ Bus
                     └─ PLA / Address Bus     → toggles debug window
```

The menu is auto-generated from `get_chip_info()`, grouped by `category`. Chips without a `render_chip_debug_window` implementation show as disabled (grayed text). This makes it immediately visible which chips have debug support and which are TODO.

**For the NES, the same menu structure:**

```
Hardware
  ├─ System Overview...
  ├─ ─────────────────
  ├─ CPU
  │   └─ Ricoh 2A03
  ├─ Video
  │   └─ PPU (Ricoh 2C02)
  └─ Audio
      └─ APU (built-in)
```

### C. Chip Debug Window (per-chip)

Each chip's debug window follows the established SID pattern:

```
┌─ VIC-II (MOS 6569) Debug ──────────────────────────────────┐
│ ┌─ Chip ─────────────┐ ┌─ State ─────────────────────────┐ │
│ │                     │ │                                  │ │
│ │   ┌──── ────────┐   │ │ ▼ Registers                     │ │
│ │   │ MOS6569     │   │ │   $D000 X Scroll: $03           │ │
│ │   │ VIC-II      │   │ │   $D001 Y Scroll: $1B           │ │
│ │   │             │   │ │   $D002 Raster Compare: $00      │ │
│ │   │  (DIP-40    │   │ │   ...                            │ │
│ │   │   with pin  │   │ │                                  │ │
│ │   │   states)   │   │ │ ▼ Raster                         │ │
│ │   │             │   │ │   Current Line: 263 / 312        │ │
│ │   └─────────────┘   │ │   Bad Lines: 16                  │ │
│ │                     │ │   Frame: 1,204,532               │ │
│ │  Pin states:        │ │                                  │ │
│ │  BA: HIGH           │ │ ▼ Sprites                        │ │
│ │  IRQ: LOW           │ │   #0: enabled, x=100 y=150      │ │
│ │  AEC: HIGH          │ │   #1: disabled                   │ │
│ │                     │ │   ...                            │ │
│ └─────────────────────┘ └──────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

- **Left panel**: chip package visualization (already exists as `ChipVisualization` renderer with `ChipLayout` data). Shows live pin states with color coding (green=HIGH, dim=LOW, yellow=floating).
- **Right panel**: chip-specific debug info in collapsible sections. Each chip implementation provides this — the framework just calls `render_chip_debug_window(index, &show)`.

The framework provides the window chrome (title bar, close button, two-column layout). Each chip only needs to fill in its content.

### D. System Overview Window (optional, future)

A single window showing all chips in a "block diagram" view:

```
┌─ C64 System Overview ──────────────────────────────────────┐
│                                                             │
│   ┌────────┐    ┌────────┐    ┌────────┐                   │
│   │ 6510   │◄──►│  PLA   │◄──►│  RAM   │                   │
│   │  CPU   │    │ Bank   │    │ 64KB   │                   │
│   └───┬────┘    └───┬────┘    └────────┘                   │
│       │             │                                       │
│   ════╪═════════════╪═══════ Address/Data Bus ═══════      │
│       │             │                                       │
│   ┌───┴────┐  ┌─────┴──┐  ┌────────┐  ┌────────┐         │
│   │ VIC-II │  │  SID   │  │ CIA 1  │  │ CIA 2  │         │
│   │ Video  │  │ Audio  │  │  I/O   │  │  I/O   │         │
│   └────────┘  └────────┘  └────────┘  └────────┘         │
│                                                             │
│   Click any chip to open its debug window                  │
└─────────────────────────────────────────────────────────────┘
```

This is the "educational" view — clickable chips that open their debug windows. Data-driven from the existing `HARDWARE_VISUALIZATION_ARCHITECTURE.md` TOML layout system (already designed, not yet connected). This is a nice-to-have, not required for the core debug functionality.

### E. Debug State Management

Replace the C64-specific `gui_state_t.show_chip_debug[16]` with a generic container:

```cpp
// In EmulatedSystem (or a ChipDebugState helper):
struct ChipDebugState {
    std::vector<bool> show_debug;    // indexed by chip_index
    std::vector<bool> show_settings; // indexed by chip_index

    void ensure_size(size_t n) {
        if (show_debug.size() < n) show_debug.resize(n, false);
        if (show_settings.size() < n) show_settings.resize(n, false);
    }
};
```

This lives in `EmulatedSystem` as a protected member. `SystemGUI` builds the Hardware menu from `get_chip_info()` and toggles `chip_debug_state_.show_debug[i]`. In `render_debug_windows()`, the base class iterates and calls `render_chip_debug_window(i, &show)` for each enabled chip.

---

## Menu Integration

### Before (current)

```
System
  ├─ Reset
  ├─ Pause/Resume
  ├─ Single Step
  ├─ Speed: [====]
  ├─ ─────────────
  ├─ [system-specific items]     ← C64 puts "Chip Debug Windows" here
  │   ├─ Load Test Binary...     ← C64-only
  │   ├─ Chip Debug Windows ►    ← C64-only, submenu per chip
  │   └─ Chip Settings ►         ← C64-only
  └─ ─────────────
```

Problems:
- "Chip Debug" buried in system-specific submenu
- Other systems have no equivalent
- Mixes system control (Reset/Pause) with hardware inspection

### After (proposed)

```
File  System  Hardware  View  Screen  Settings  Help
        │        │
        │        ├─ System Overview...
        │        ├─ ────────────
        │        ├─ CPU ►
        │        ├─ Video ►
        │        ├─ Audio ►
        │        ├─ I/O ►
        │        └─ Memory ►
        │
        ├─ Reset
        ├─ Pause/Resume
        ├─ Single Step
        ├─ Speed: [====]
        ├─ ─────────────
        └─ [system-specific items]   ← Reset variant, Eject Cart, etc.
```

Benefits:
- Chip debug has its own top-level home — discoverable for all systems
- System menu stays clean (controls only)
- Uniform across C64, VIC-20, NES, etc.
- Grayed-out items show what's possible even before debug windows are implemented

### Connector Icons (unchanged)

The right-aligned connector port icons in the menu bar remain as-is. They're a different concern (peripheral attachment) and their pattern works well. The Hardware menu complements them — connectors show *what's plugged in*, Hardware shows *what's on the board*.

---

## Implementation Plan

### Step 1: Define the interface (EmulatedSystem)

Add `ChipInfo`, `get_chip_info()`, `render_chip_debug_window()`, `render_chip_settings_window()`, and `ChipDebugState` to `EmulatedSystem`.

### Step 2: Build the Hardware menu (SystemGUI)

Add a "Hardware" menu to the menu bar that auto-generates from `get_chip_info()`. Group by category, toggle `chip_debug_state_` flags. Call `render_chip_debug_window()` from the render loop.

### Step 3: Implement for C64

Override `get_chip_info()` in `C64System` to return all 12+ chips. Move the existing SID `render_debug_window` callback to the new `render_chip_debug_window(index, show)` method. The SID window works immediately. Other chips show as grayed-out until their debug windows are implemented.

### Step 4: Implement for other systems

Each system adds its `get_chip_info()` override — even if all chips show as grayed-out initially. This gives users a "hardware map" of every emulated system from day one.

### Step 5: Gradually add chip debug windows

Each chip gets a debug window implementation at its own pace. Priority:
1. **VIC-II** — raster state, sprite positions, register dump (most useful for C64 debugging)
2. **CIA 1/2** — timer state, port values, interrupt state
3. **TED 7360** — for C16/Plus4 (register dump, raster, sound)
4. **6502/6510 CPU** — register file, flags, PC, SP, cycle count
5. **NES PPU** — nametable, sprite OAM, scroll state
6. **NES APU** — channel state, duty cycle, volume

Each chip debug window follows the SID pattern: chip visualization left, registers/state right.

### Step 6: Add System Overview window (optional)

Block-diagram view with clickable chips. Data-driven from TOML board layouts (already designed in `HARDWARE_VISUALIZATION_ARCHITECTURE.md`). This is the "educational" long-term goal — not blocking on anything else.

---

## What About `system_8bit_t`?

The legacy `system_8bit_t` chip registry was the C64's mechanism to iterate chips for debug windows. With the new `get_chip_info()` virtual method, this is no longer needed:

- `get_chip_info()` replaces `system.chips[]` iteration
- `render_chip_debug_window(i)` replaces `desc->render_debug_window(chip, &show)` callback
- `ChipDebugState` replaces `gui_state_t.show_chip_debug[16]`

The `system_8bit_t` struct and `chip_entry_t` array can be removed during the C64 consolidation (Phase 3 of the consolidation plan).

The `ChipDescriptor` struct (`chip.h`) with its `create`/`destroy`/`render_debug_window` function pointers is the C-style precursor to this design. The `ChipBase` abstract class (also in `chip.h`) is the C++ successor. Both can coexist during migration — chips that already use `ChipBase` get debug windows through the virtual method; legacy C chips use wrapper lambdas:

```cpp
void C64System::render_chip_debug_window(int chip_index, bool* show) {
    switch (chip_index) {
        case CHIP_SID:
            mos6581_render_debug_window(sid_, show);  // existing function, works as-is
            break;
        case CHIP_VICII:
            // TODO: implement VIC-II debug window
            break;
    }
}
```

---

## Summary

| Aspect | Current | Proposed |
|--------|---------|----------|
| Menu location | Buried in System > system-specific | Top-level "Hardware" menu |
| Systems covered | C64 only | All systems |
| Chip enumeration | `system_8bit_t.chips[]` (C64-only) | `get_chip_info()` virtual (all systems) |
| Debug state | `gui_state_t.show_chip_debug[16]` | `ChipDebugState` in base class |
| Window rendering | `desc->render_debug_window()` C callback | `render_chip_debug_window(i)` virtual |
| Discovery | Hidden — users must know to look | Visible — grayed items show what exists |
| Extensibility | Add to C chip registry | Override virtual method |

**Confidence: 0.9**

The design follows established patterns (connector UI, SID debug window) and requires no new rendering infrastructure. The main uncertainty is whether the "Hardware" menu name is the best choice — alternatives include "Chips", "Debug", or "Inspect". "Hardware" was chosen because it aligns with the existing `HARDWARE_VISUALIZATION_ARCHITECTURE.md` and encompasses the System Overview window.

**Key uncertainties:**
- Menu name preference (Hardware vs. Chips vs. Debug)
- Whether grayed-out chip items are annoying or helpful for users
- Priority ordering of which chip debug windows to implement first

**How to improve:** User feedback on menu structure mockup before implementing.
