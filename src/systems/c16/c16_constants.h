#pragma once

#include <cstdint>

// ============================================================================
// C16 / PLUS/4 (TED) SYSTEM CONSTANTS
// ============================================================================

namespace c16_constants {
    // CPU / timing — PAL
    inline constexpr uint32_t CPU_FREQ_PAL           = 886724;    // PAL TED clock ÷ 2
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL   = 17734;     // 886724 / 50

    // CPU / timing — NTSC
    inline constexpr uint32_t CPU_FREQ_NTSC          = 894886;    // NTSC TED clock ÷ 2


    // Audio
    inline constexpr uint32_t AUDIO_SAMPLE_RATE      = 22050;     // Default output sample rate (Hz)

    // Display
    inline constexpr uint32_t DISPLAY_WIDTH          = 384;    // TED_VISIBLE_WIDTH (320 + borders)
    inline constexpr uint32_t DISPLAY_HEIGHT         = 288;    // TED_VISIBLE_HEIGHT_PAL (normal borders)

    // Memory sizes
    inline constexpr uint32_t RAM_SIZE_C16           = 16384;     // 16 KB (C16/C116)
    inline constexpr uint32_t RAM_SIZE_PLUS4         = 65536;     // 64 KB (Plus/4)
    inline constexpr uint32_t ROM_SIZE               = 32768;     // 32 KB combined ROM
    inline constexpr uint32_t ROM_HALF_SIZE          = 16384;     // 16 KB per ROM chip (BASIC / KERNAL)

    // BASIC start address
    inline constexpr uint16_t BASIC_START            = 0x1001;

    // Keyboard buffer — C16/Plus4 uses different addresses than C64/VIC-20
    inline constexpr uint16_t KBD_BUFFER_BASE        = 0x0527;      // Keyboard buffer ($0527-$052E)
    inline constexpr uint16_t KBD_BUFFER_COUNT       = 0x00EF;      // Keyboard buffer count (ZP)
    inline constexpr uint8_t  KBD_BUFFER_SIZE        = 8;           // Buffer capacity (8 bytes)

    // BASIC 3.5 warm-start vector (written to $0302/$0303 during cold-start)
    inline constexpr uint8_t  BASIC_WARMSTART_LO     = 0x12;        // Low byte of $8712
    inline constexpr uint8_t  BASIC_WARMSTART_HI     = 0x87;        // High byte of $8712
}
