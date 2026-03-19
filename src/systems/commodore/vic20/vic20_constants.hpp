#pragma once

#include <cstdint>

// ============================================================================
// VIC-20 SYSTEM CONSTANTS
// ============================================================================

namespace vic20_constants {
    // CPU / timing — PAL
    inline constexpr uint32_t CPU_FREQ_PAL           = 1108405;   // PAL crystal: 4.433619 MHz / 4
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL   = 22168;     // 1108405 / 50

    // CPU / timing — NTSC
    inline constexpr uint32_t CPU_FREQ_NTSC          = 1022727;   // NTSC crystal: 14.31818 MHz / 14


    // Audio
    inline constexpr uint32_t AUDIO_SAMPLE_RATE      = 22050;     // Default output sample rate (Hz)

    // Display (per-dot-clock visible area, excluding HSync cycle)
    inline constexpr uint32_t DISPLAY_WIDTH          = 248;        // (63 - 1) cycles × 4 pixels
    inline constexpr uint32_t DISPLAY_HEIGHT         = 284;        // Visible raster lines (PAL)

    // BASIC start addresses (vary by expansion config)
    inline constexpr uint16_t BASIC_START_UNEXPANDED = 0x1001;     // Default (unexpanded)
    inline constexpr uint16_t BASIC_START_3K         = 0x0401;     // 3 KB expansion
    inline constexpr uint16_t BASIC_START_8K         = 0x1201;     // 8 KB+ expansion

    // BASIC warm-start vector (written to $0302/$0303 after boot)
    inline constexpr uint8_t  BASIC_WARMSTART_LO     = 0x74;       // Low byte of $C474
    inline constexpr uint8_t  BASIC_WARMSTART_HI     = 0xC4;       // High byte of $C474

    // KERNAL keyboard buffer
    inline constexpr uint16_t KBD_BUFFER_BASE        = 0x0277;     // 10-byte keyboard buffer
    inline constexpr uint8_t  KBD_BUFFER_COUNT       = 0xC6;       // Keyboard buffer count (ZP)

    // VIC-20 memory block boundaries
    inline constexpr uint16_t BLK0_START             = 0x0400;     // Block 0: 3 KB expansion RAM
    inline constexpr uint16_t BLK0_END               = 0x1000;     // End of block 0
    inline constexpr uint16_t BLK1_START             = 0x2000;     // Block 1: 8 KB expansion
    inline constexpr uint16_t BLK1_END               = 0x4000;     // End of block 1
    inline constexpr uint16_t BLK2_START             = 0x4000;     // Block 2: 8 KB expansion
    inline constexpr uint16_t BLK2_END               = 0x6000;     // End of block 2
    inline constexpr uint16_t BLK3_START             = 0x6000;     // Block 3: 8 KB expansion
    inline constexpr uint16_t BLK3_END               = 0x8000;     // End of block 3
    inline constexpr uint16_t BLK5_START             = 0xA000;     // Block 5: cartridge ROM
}
