#pragma once

#include <cstdint>

// ============================================================================
// C64 SYSTEM CONSTANTS
// ============================================================================

namespace c64_constants {
    // CPU / timing
    inline constexpr uint32_t CPU_FREQ_PAL        = 985248;     // ~0.985 MHz
    inline constexpr uint32_t CPU_FREQ_NTSC       = 1022727;    // ~1.023 MHz
    inline constexpr uint32_t AUDIO_SAMPLE_RATE   = 44100;      // Hz
    inline constexpr uint32_t TARGET_FPS_PAL      = 50;

    // Display (VIC-II PAL visible area)
    inline constexpr uint32_t DISPLAY_WIDTH_PAL   = 403;
    inline constexpr uint32_t DISPLAY_HEIGHT_PAL  = 284;

    // Memory sizes
    inline constexpr uint32_t BASIC_ROM_SIZE      = 8192;        // 8 KB
    inline constexpr uint32_t KERNAL_ROM_SIZE     = 8192;        // 8 KB
    inline constexpr uint32_t CHAR_ROM_SIZE       = 4096;        // 4 KB

    // Memory map base addresses
    inline constexpr uint16_t BASIC_START         = 0x0801;      // BASIC program start
    inline constexpr uint16_t BASIC_ROM_BASE      = 0xA000;      // BASIC ROM
    inline constexpr uint16_t KERNAL_BASE         = 0xE000;      // KERNAL ROM
    inline constexpr uint16_t CHAR_ROM_BASE       = 0xD000;      // Character ROM
    inline constexpr uint16_t ROML_BASE           = 0x8000;      // Cartridge ROML
    inline constexpr uint16_t SCREEN_RAM_BASE     = 0x0400;      // Default screen RAM
    inline constexpr uint16_t KBD_BUFFER_BASE     = 0x0277;      // Keyboard buffer
    inline constexpr uint8_t  KBD_BUFFER_COUNT    = 0xC6;        // Keyboard buffer count (ZP)

    // CIA I/O registers used in SID player / system setup
    inline constexpr uint16_t CIA1_ICR            = 0xDC0D;
    inline constexpr uint16_t CIA2_ICR            = 0xDD0D;
}
