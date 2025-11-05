/*
 * chip_layouts.h - Hardware-accurate pin layouts for all chip types
 * 
 * This header provides chip layout functions for all chips in the system,
 * ensuring hardware-accurate visualization with proper pin assignments.
 */

#ifndef CHIP_LAYOUTS_H
#define CHIP_LAYOUTS_H

#include "../core/chip_layout.h"
#include "../gui/chip_visualization.h"
#include "../core/system_lines.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VIDEO CHIP LAYOUTS (VIC-II Family)
// ============================================================================

// MOS 6567 VIC-II (NTSC) - 40-pin DIP
PinLayout create_mos6567_vic_layout(void);

// MOS 6569 VIC-II (PAL) - 40-pin DIP
PinLayout create_mos6569_vic_layout(void);

// ============================================================================
// AUDIO CHIP LAYOUTS (SID Family)
// ============================================================================

// MOS 6581 SID - 28-pin DIP
PinLayout create_mos6581_sid_layout(void);

// ============================================================================
// I/O CHIP LAYOUTS (CIA Family)
// ============================================================================

// MOS 6526 CIA - 40-pin DIP
PinLayout create_mos6526_cia_layout(void);

// ============================================================================
// MEMORY CHIP LAYOUTS
// ============================================================================

// Generic RAM chip - 18-pin DIP (MOS 2114 style)
PinLayout create_generic_ram_layout(void);

// MOS 2114 SRAM (1K x 4) - 18-pin DIP
PinLayout create_mos2114_layout(void);

// Generic ROM chip - various packages
PinLayout create_generic_rom_layout(void);

// ============================================================================
// LOGIC CHIP LAYOUTS (PLA and others)
// ============================================================================

// Generic PLA - 28-pin DIP
PinLayout create_generic_pla_layout(void);

// C64 PLA (906114-01) - 28-pin DIP
PinLayout create_c64_pla_layout(void);

// ============================================================================
// CHIP VISUALIZATION HELPERS
// ============================================================================

// Create chip visualization with layout
typedef struct {
    PinLayout layout;
    ChipVisualization* visualization;
    bool initialized;
} ChipGUIState;

// Initialize chip GUI state with layout
void init_chip_gui_state(ChipGUIState* state, PinLayout layout);

// Cleanup chip GUI state
void cleanup_chip_gui_state(ChipGUIState* state);

// Render chip visualization in GUI
void render_chip_layout_gui(ChipGUIState* state, void* chip, bus_state_t bus_state, const char* chip_name);

// Get pin states for generic chip (basic implementation)
void get_generic_chip_pin_states(void* chip, const PinLayout* layout, bus_state_t bus_state, PinState* out_states);

#ifdef __cplusplus
}
#endif

#endif // CHIP_LAYOUTS_H