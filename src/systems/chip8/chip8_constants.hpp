#pragma once

#include <cstdint>

// ============================================================================
// CHIP-8 SYSTEM CONSTANTS
// ============================================================================

namespace chip8_constants {
    // Display dimensions (hi-res)
    inline constexpr uint32_t HIRES_WIDTH            = 128;
    inline constexpr uint32_t HIRES_HEIGHT           = 64;

    // Audio
    inline constexpr uint32_t AUDIO_SAMPLE_RATE      = 4000;

    // Program start address
    inline constexpr uint16_t PROGRAM_START          = 0x200;

    // Maximum ROM size (standard: 4KB - program start)
    inline constexpr uint32_t MAX_ROM_STANDARD       = 4096 - PROGRAM_START;   // 3584

    // Timer rate
    inline constexpr uint32_t TIMER_HZ               = 60;
}
