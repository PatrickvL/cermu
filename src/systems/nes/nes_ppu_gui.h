#pragma once

#include <stdbool.h>

// Forward declaration
namespace nes_system {
    class PPU;
}

// NES PPU (Ricoh 2C02) GUI rendering functions
void nes_ppu_render_debug_window(nes_system::PPU* ppu, bool* show_window);
void nes_ppu_render_settings_window(nes_system::PPU* ppu, bool* show_window);
