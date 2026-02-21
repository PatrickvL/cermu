#pragma once

#include <stdbool.h>

// Forward declaration
namespace nes_system {
    class PPU;
}

// NES PPU (Ricoh 2C02) GUI rendering functions
void nes_ppu_render_debug_content(nes_system::PPU* ppu);
void nes_ppu_render_settings_content(nes_system::PPU* ppu);
void nes_ppu_render_layout_content(nes_system::PPU* ppu);
