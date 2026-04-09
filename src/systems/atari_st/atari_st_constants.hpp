#pragma once
/*
 * atari_st_constants.hpp — Atari ST system constants
 *
 * The Atari ST (1985) uses a Motorola 68000 CPU with a custom chipset:
 *   - Shifter: video DAC / shift register (palette + framebuffer DMA)
 *   - GLUE: address decoding, bus arbitration, interrupt routing
 *   - MK68901 MFP: timers, USART, interrupts, GPIO
 *   - YM2149 PSG: sound + floppy control + parallel port
 *   - WD1772 FDC: floppy disk controller
 *   - Two MC6850 ACIAs: keyboard + MIDI
 *
 * Memory map (24-bit, active low 22 lines):
 *   $000000–$3FFFFF : RAM (512KB–4MB)
 *   $FA0000–$FBFFFF : Cartridge ROM (128KB)
 *   $FC0000–$FEFFFF : TOS ROM (192KB, or 256KB on later models)
 *   $FF8000–$FF8FFF : Hardware I/O
 *   $FFFA00–$FFFA3F : MFP (MK68901)
 *   $FFFC00–$FFFC06 : ACIA (keyboard + MIDI)
 */

#include <cstdint>

namespace atari_st_constants {

    // ── Clock ────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ          = 8000000;   // 8 MHz
    inline constexpr uint32_t MFP_FREQ_HZ          = 2457600;   // 2.4576 MHz MFP clock
    inline constexpr uint32_t PSG_FREQ_HZ          = 2000000;   // 2 MHz YM2149

    // ── Display ──────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH_LOW         = 320;
    inline constexpr int DISPLAY_HEIGHT_LOW        = 200;
    inline constexpr int DISPLAY_WIDTH_MED         = 640;
    inline constexpr int DISPLAY_HEIGHT_MED        = 200;
    inline constexpr int DISPLAY_WIDTH_HI          = 640;
    inline constexpr int DISPLAY_HEIGHT_HI         = 400;
    inline constexpr int TARGET_FPS_50HZ           = 50;
    inline constexpr int TARGET_FPS_60HZ           = 60;

    // ── Timing ───────────────────────────────────────────────────────────
    inline constexpr uint32_t CYCLES_PER_FRAME_50  = CPU_FREQ_HZ / TARGET_FPS_50HZ;
    inline constexpr uint32_t CYCLES_PER_FRAME_60  = CPU_FREQ_HZ / TARGET_FPS_60HZ;

    // ── Memory sizes ─────────────────────────────────────────────────────
    inline constexpr uint32_t RAM_SIZE_512K        = 524288;
    inline constexpr uint32_t RAM_SIZE_1M          = 1048576;
    inline constexpr uint32_t RAM_SIZE_4M          = 4194304;
    inline constexpr uint32_t TOS_ROM_SIZE         = 196608;     // 192KB
    inline constexpr uint32_t CART_ROM_SIZE        = 131072;     // 128KB

    // ── I/O addresses ────────────────────────────────────────────────────
    // Hardware register space: $FF8000–$FF8FFF
    inline constexpr uint32_t IO_BASE              = 0xFF8000;
    inline constexpr uint32_t SHIFTER_BASE         = 0xFF8200;  // $FF8200–$FF8260
    inline constexpr uint32_t PALETTE_BASE         = 0xFF8240;  // $FF8240–$FF825F
    inline constexpr uint32_t PSG_BASE             = 0xFF8800;  // $FF8800–$FF8803
    inline constexpr uint32_t FDC_BASE             = 0xFF8604;  // $FF8604–$FF860F
    inline constexpr uint32_t DMA_BASE             = 0xFF8600;  // $FF8600–$FF860F
    inline constexpr uint32_t MFP_BASE             = 0xFFFA00;  // $FFFA01–$FFFA2F
    inline constexpr uint32_t ACIA_KBD_BASE        = 0xFFFC00;  // $FFFC00–$FFFC02
    inline constexpr uint32_t ACIA_MIDI_BASE       = 0xFFFC04;  // $FFFC04–$FFFC06

    // ROM addresses
    inline constexpr uint32_t TOS_ROM_BASE         = 0xFC0000;
    inline constexpr uint32_t CART_ROM_BASE        = 0xFA0000;

    // ── Audio ────────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

}  // namespace atari_st_constants
