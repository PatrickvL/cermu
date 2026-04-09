#pragma once
/*
 * atari8_constants.hpp — Atari 400/800/XL/XE system constants
 *
 * Atari 800 (1979) / Atari 800XL (1983) / Atari 130XE (1985):
 *   CPU:    MOS 6502C @ 1.7897725 MHz (NTSC) / 1.773447 MHz (PAL)
 *   Video:  ANTIC (display list processor) + GTIA (color/sprite)
 *   Sound:  POKEY (4-channel PSG + keyboard/serial/timer I/O)
 *   I/O:    PIA 6520 (joystick ports, memory banking)
 *   Memory: 800: 48KB  |  XL: 64KB  |  130XE: 128KB (banked)
 *   Media:  ROM cartridge (8–16KB), cassette, floppy (SIO)
 *
 * Memory map ($0000–$FFFF):
 *   $0000–$3FFF : RAM (16KB low)
 *   $4000–$7FFF : RAM (may be banked on 130XE)
 *   $8000–$9FFF : RAM or cartridge ROM (left cart window)
 *   $A000–$BFFF : RAM or cartridge ROM / BASIC ROM
 *   $C000–$CFFF : RAM or OS ROM
 *   $D000–$D0FF : GTIA registers ($D000–$D01F, mirrored)
 *   $D200–$D2FF : POKEY registers ($D200–$D20F, mirrored)
 *   $D300–$D3FF : PIA registers ($D300–$D303, mirrored)
 *   $D400–$D4FF : ANTIC registers ($D400–$D40F, mirrored)
 *   $D800–$FFFF : OS ROM (10KB) / floating-point math pack
 */

#include <cstdint>

namespace atari8_constants {

    inline constexpr uint32_t CPU_FREQ_NTSC            = 1789772;
    inline constexpr uint32_t CPU_FREQ_PAL             = 1773447;

    inline constexpr uint32_t RAM_SIZE_800             = 49152;    // 48KB
    inline constexpr uint32_t RAM_SIZE_XL              = 65536;    // 64KB
    inline constexpr uint32_t RAM_SIZE_XE              = 131072;   // 128KB

    inline constexpr uint16_t GTIA_BASE                = 0xD000;
    inline constexpr uint16_t POKEY_BASE               = 0xD200;
    inline constexpr uint16_t PIA_BASE                 = 0xD300;
    inline constexpr uint16_t ANTIC_BASE               = 0xD400;

    inline constexpr int DISPLAY_WIDTH                 = 320;     // Wide playfield
    inline constexpr int DISPLAY_HEIGHT                = 192;

    // NTSC: 262 lines × 114 color clocks / 2 = 262 × 57 = 14934 CPU cycles/frame
    inline constexpr uint32_t CYCLES_PER_FRAME_NTSC    = 29868;
    // PAL: 312 lines × 114 color clocks / 2
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL     = 35568;

    inline constexpr int DEFAULT_SAMPLE_RATE           = 44100;

} // namespace atari8_constants
