#pragma once

#include <cstdint>

// ============================================================================
// APPLE 1 SYSTEM CONSTANTS
// ============================================================================

namespace apple1_constants {
    // CPU / timing
    inline constexpr uint32_t CPU_FREQ               = 1000000;   // 1 MHz
    inline constexpr uint32_t CYCLES_PER_FRAME        = 16667;     // 1000000 / 60

    // Display (terminal: 40 columns × 24 rows × 8 pixels)
    inline constexpr uint32_t DISPLAY_WIDTH           = 320;       // 40 × 8
    inline constexpr uint32_t DISPLAY_HEIGHT          = 192;       // 24 × 8

    // Memory sizes
    inline constexpr uint32_t RAM_8K                  = 8192;      // Default
    inline constexpr uint32_t RAM_64K                 = 65536;

    // Memory map
    inline constexpr uint16_t MONITOR_BASE            = 0xFF00;    // Woz Monitor ROM
    inline constexpr uint16_t PIA_BASE                = 0xD010;    // MC6820 PIA
}
