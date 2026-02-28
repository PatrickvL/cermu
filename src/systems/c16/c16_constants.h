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
    inline constexpr uint32_t DISPLAY_WIDTH          = 320;
    inline constexpr uint32_t DISPLAY_HEIGHT         = 200;

    // Memory sizes
    inline constexpr uint32_t RAM_SIZE_C16           = 16384;     // 16 KB (C16/C116)
    inline constexpr uint32_t RAM_SIZE_PLUS4         = 65536;     // 64 KB (Plus/4)
    inline constexpr uint32_t ROM_SIZE               = 32768;     // 32 KB combined ROM
    inline constexpr uint32_t ROM_HALF_SIZE          = 16384;     // 16 KB per ROM chip (BASIC / KERNAL)

    // BASIC start address
    inline constexpr uint16_t BASIC_START            = 0x1001;
}
