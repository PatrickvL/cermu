# Chip GUI Implementation Guide

## Overview

This document describes the implementation of GUI debug and settings windows for all emulated chips in the C64 emulator. The GUI system uses Dear ImGui via cimgui bindings to provide real-time debugging and configuration interfaces for each chip.

## Architecture

### 1. Chip Descriptor Extension

The `chip_descriptor_t` structure in `core/chip.h` has been extended with two optional GUI callback functions:

```c
struct chip_descriptor_s {
    // ... existing fields ...
    void (*render_debug_window)(void* chip, bool* show_window);     // Optional GUI debug window callback
    void (*render_settings_window)(void* chip, bool* show_window);  // Optional GUI settings window callback
};
```

### 2. GUI File Structure

Each chip now has a corresponding `*_gui.c` file that implements:
- Debug window: Real-time display of chip state, registers, and internal status
- Settings window: Configuration options and controls for chip behavior

## Implemented GUI Files

### CPU
- **File**: `chip/cpu/mos6510/mos6510_gui.c`
- **Features**: 
  - CPU state display (PC, A, X, Y, SP, flags)
  - I/O port visualization ($0000-$0001)
  - Control lines status (IRQ, NMI, RDY)
  - Single-step execution controls
  - CPU configuration settings

### I/O Chips
- **File**: `chip/io/mos6526_gui.c`
- **Features**:
  - CIA port status (PRA, PRB, DDRA, DDRB)
  - Timer states and controls
  - Time-of-day clock display
  - Interrupt status
  - Port configuration

### Sound Chip
- **File**: `chip/sound/mos6581_gui.c`
- **Features**:
  - Voice status for all 3 SID voices
  - Filter and volume controls
  - Sound enable/disable options
  - Master volume control

### Video Chips
- **Files**: 
  - `chip/video/mos6567_gui.c` (NTSC VIC-II)
  - `chip/video/mos6569_gui.c` (PAL VIC-II)
- **Features**:
  - Display control registers
  - Sprite status and positioning
  - Color palette settings
  - Memory pointer configuration
  - Display mode selection

### Memory
- **File**: `chip/memory/ram_gui.c`
- **Features**:
  - Memory viewer with hex display
  - Address input for navigation
  - Memory manipulation tools
  - Pattern fill options

### Logic
- **File**: `chip/logic/pla_gui.c`
- **Features**:
  - Memory banking visualization
  - ROM/RAM mapping display
  - Banking control switches
  - Current memory map view

## Integration Status

### ✅ Completed
1. Extended chip descriptor structure
2. Created GUI files for all major chips:
   - MOS6510 CPU
   - MOS6526 CIA
   - MOS6581 SID
   - MOS6567/6569 VIC-II
   - RAM
   - PLA
3. Implemented MOS6510 GUI integration (callbacks added to descriptor)

### 🔄 In Progress / Needs Completion

#### 1. Fix cimgui Integration Issues
The GUI files currently have compilation issues with cimgui includes and function signatures. Need to:
- Verify correct cimgui include paths
- Fix `ImVec2` type definitions
- Correct function parameter lists for ImGui functions

#### 2. Complete Chip Descriptor Updates
Add GUI callbacks to remaining chip descriptors:
```c
// In each chip's .c file, update the descriptor:
chip_descriptor_t chipname_descriptor = {
    // ... existing fields ...
    .render_debug_window = chipname_render_debug_window,
    .render_settings_window = chipname_render_settings_window
};
```

#### 3. Add Function Declarations
Add GUI function declarations to each chip's header file:
```c
// In each chip's .h file:
void chipname_render_debug_window(void* chip, bool* show_window);
void chipname_render_settings_window(void* chip, bool* show_window);
```

#### 4. Include GUI Files in Build
Add `#include "chipname_gui.c"` to each chip's main .c file (following the pattern used in MOS6510).

#### 5. Connect to Main GUI System
Update the main GUI interface to:
- Add menu items for each chip's debug/settings windows
- Maintain window state in `gui_state_t`
- Call chip GUI functions when windows are open

## Usage Example

Once completed, the system will work as follows:

```c
// In main GUI rendering loop:
if (gui_state->show_cpu_debug) {
    mos6510_descriptor.render_debug_window(c64->cpu, &gui_state->show_cpu_debug);
}

if (gui_state->show_cpu_settings) {
    mos6510_descriptor.render_settings_window(c64->cpu, &gui_state->show_cpu_settings);
}
```

## GUI Window Features

### Debug Windows
- **Real-time State Display**: Live view of registers, flags, and internal state
- **Memory Visualization**: Hex displays, bit patterns, color-coded status
- **Control Flow**: Execution control, breakpoints, single-stepping
- **Status Indicators**: Visual indicators for active states (interrupts, timers, etc.)

### Settings Windows
- **Configuration Options**: Enable/disable features, timing adjustments
- **Behavioral Controls**: Modify chip behavior for testing
- **Reset Functions**: Individual chip reset capabilities
- **Debug Options**: Logging, tracing, breakpoint configuration

## Benefits

1. **Real-time Debugging**: See chip state changes as they happen
2. **Educational Value**: Learn how C64 chips work by watching them operate
3. **Development Aid**: Easier debugging of emulation accuracy issues
4. **Interactive Testing**: Modify chip behavior to test edge cases
5. **Visual Feedback**: Immediate visual confirmation of chip interactions

## Next Steps

1. Fix cimgui compilation issues
2. Complete integration for all chips
3. Add window state management to main GUI
4. Implement actual chip state reading (replace placeholder values)
5. Add interactive controls for chip manipulation
6. Test and refine the interface

This GUI system will transform the C64 emulator from a black-box emulator into a transparent, educational, and highly debuggable system.