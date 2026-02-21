#pragma once

#include <stdbool.h>

// Forward declaration  
struct nes6502_t;

// NES APU (Ricoh 2A03 built-in) GUI rendering functions
// The APU is part of the 2A03 CPU package; we pass the CPU handle
// so we can extract the APU instance via nes6502_get_apu().
void nes_apu_render_debug_window(nes6502_t* cpu, bool* show_window);
void nes_apu_render_settings_window(nes6502_t* cpu, bool* show_window);
void nes_apu_render_layout_window(nes6502_t* cpu, bool* show_window);
