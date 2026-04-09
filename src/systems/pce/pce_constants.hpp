#pragma once
/*
 * pce_constants.hpp — PC Engine / TurboGrafx-16 system constants
 *
 * The PC Engine (1987) uses a Hudson HuC6280 CPU (modified 65C02) with
 * integrated PSG, HuC6270 VDC for background/sprite rendering, and
 * HuC6260 VCE for color output.
 *
 * Memory map (21-bit, 2MB via 8-page MPR banking):
 *   Page $F8: RAM ($1F0000–$1F1FFF, 8KB)
 *   Page $FF: I/O ($1FE000–$1FFFFF, hardware registers)
 *   Pages $00–$7F: HuCard ROM (up to 1MB)
 *   Pages $80–$87: CD-ROM System Card RAM (optional)
 *
 * I/O page ($1FE000, 8KB window):
 *   $0000–$0003: VDC (HuC6270)
 *   $0400–$0407: VCE (HuC6260)
 *   $0800–$0809: PSG (HuC6280 built-in)
 *   $0C00–$0C01: Timer
 *   $1000:       I/O port (joypad)
 *   $1400–$1403: IRQ control
 */

#include <cstdint>

namespace pce_constants {

    // ── Clock ────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ          = 7159090;   // 7.16 MHz master
    inline constexpr uint32_t CPU_FREQ_SLOW_HZ     = 1789772;   // 1.79 MHz (slow mode)

    // ── Display ──────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH             = 256;       // Default (variable)
    inline constexpr int DISPLAY_HEIGHT            = 240;
    inline constexpr int TARGET_FPS                = 60;

    // ── Timing ───────────────────────────────────────────────────────────
    inline constexpr uint32_t DOTS_PER_LINE        = 1365;      // VDC dot counter
    inline constexpr uint32_t LINES_PER_FRAME      = 263;       // NTSC
    inline constexpr uint32_t CYCLES_PER_FRAME     = CPU_FREQ_HZ / TARGET_FPS;

    // ── Memory sizes ─────────────────────────────────────────────────────
    inline constexpr uint32_t RAM_SIZE             = 8192;       // 8KB work RAM
    inline constexpr uint32_t MAX_ROM_SIZE         = 1048576;    // 1MB HuCard max
    inline constexpr uint32_t ROM_PAGE_SIZE        = 8192;       // 8KB page

    // ── I/O addresses (within I/O page) ──────────────────────────────────
    inline constexpr uint16_t IO_VDC               = 0x0000;    // $0000–$0003
    inline constexpr uint16_t IO_VCE               = 0x0400;    // $0400–$0407
    inline constexpr uint16_t IO_PSG               = 0x0800;    // $0800–$0809
    inline constexpr uint16_t IO_TIMER             = 0x0C00;    // $0C00–$0C01
    inline constexpr uint16_t IO_JOYPAD            = 0x1000;    // $1000
    inline constexpr uint16_t IO_IRQ               = 0x1400;    // $1400–$1403

    // ── Audio ────────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

}  // namespace pce_constants
