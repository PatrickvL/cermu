/*
 * non_cpu_chip_layouts.h - Pin layout definitions for non-CPU chips
 * 
 * This header defines pin layouts for video, audio, and I/O chips
 * used in vintage computer systems, providing hardware-accurate
 * visualization with proper pin assignments and package information.
 */

#ifndef NON_CPU_CHIP_LAYOUTS_H
#define NON_CPU_CHIP_LAYOUTS_H

#include "chip_layout.h"
#include "../gui/chip_visualization.h"
#include "system_lines.h"

// ============================================================================
// VIDEO CHIP LAYOUTS
// ============================================================================

// MOS 6567 VIC-II (NTSC) - 40-pin DIP
ChipLayout create_mos6567_layout();

// MOS 6569 VIC-II (PAL) - 40-pin DIP  
ChipLayout create_mos6569_layout();

// ============================================================================
// AUDIO CHIP LAYOUTS
// ============================================================================

// MOS 6581 SID - 28-pin DIP
ChipLayout create_mos6581_layout();

// ============================================================================
// I/O CHIP LAYOUTS
// ============================================================================

// MOS 6526 CIA - 40-pin DIP
ChipLayout create_mos6526_layout();

// ============================================================================
// MEMORY CHIP LAYOUTS
// ============================================================================

// MOS 2114 SRAM - 18-pin DIP
ChipLayout create_mos2114_layout();

// ============================================================================
// LOGIC CHIP LAYOUTS
// ============================================================================

// 74LS139 Dual 2-to-4 Decoder - 16-pin DIP
ChipLayout create_74ls139_layout();

// Custom C64 PLA - 28-pin DIP
ChipLayout create_c64_pla_layout();

// ============================================================================
// CHIP PIN STATE EXTRACTION FOR NON-CPU CHIPS
// ============================================================================

// Generic pin state extraction for video chips
std::vector<PinState> get_video_chip_pin_states(void* chip, const ChipLayout* layout, bus_state_t bus_state);

// Generic pin state extraction for audio chips  
std::vector<PinState> get_audio_chip_pin_states(void* chip, const ChipLayout* layout, bus_state_t bus_state);

// Generic pin state extraction for I/O chips
std::vector<PinState> get_io_chip_pin_states(void* chip, const ChipLayout* layout, bus_state_t bus_state);

// Generic pin state extraction for memory chips
std::vector<PinState> get_memory_chip_pin_states(void* chip, const ChipLayout* layout, bus_state_t bus_state);

// Generic pin state extraction for logic chips
std::vector<PinState> get_logic_chip_pin_states(void* chip, const ChipLayout* layout, bus_state_t bus_state);

#endif // NON_CPU_CHIP_LAYOUTS_H