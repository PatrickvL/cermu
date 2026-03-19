#pragma once
/*
 * atari_vector_constants.hpp — Shared constants for Atari vector arcade systems
 *
 * Defines hardware constants shared between Asteroids (1979) and
 * Lunar Lander (1979).  Both use the same general board architecture:
 *   - MOS 6502 CPU @ 1.512 MHz
 *   - DVG (Digital Vector Generator) for XY vector display
 *   - Discrete sound circuits (no sound chip)
 *   - DIP switches, coin inputs, player controls
 *
 * Memory maps differ between games:
 *   Asteroids:     2 KB RAM ($0000-$03FF), vector RAM ($4000-$47FF),
 *                  vector ROM ($5000-$57FF), program ROM ($6800-$7FFF).
 *   Lunar Lander:  1 KB RAM ($0000-$03FF), vector RAM ($4000-$47FF),
 *                  vector ROM ($5000-$57FF), program ROM ($6000-$7FFF).
 *
 * Both share the same I/O decode region ($2000-$3FFF):
 *   Reads:   coin/start switches, player inputs, DIP switches
 *   Writes:  VGGO, VGRST, sound triggers, coin counters, LEDs
 */

#include <cstdint>

// ============================================================================
// Game variant enum — selects between Asteroids and Lunar Lander
// ============================================================================

enum class AtariVectorVariant : uint8_t {
    ASTEROIDS,
    LUNAR_LANDER,
};

namespace atari_vector_constants {

    // ========================================================================
    // Timing
    // ========================================================================

    // Master clock: 12.096 MHz crystal, CPU gets /8 = 1.512 MHz
    inline constexpr uint32_t MASTER_CLOCK_HZ     = 12096000;
    inline constexpr uint32_t CPU_FREQ_HZ          = MASTER_CLOCK_HZ / 8;  // 1,512,000 Hz

    // Frame rate: 60 Hz (driven by 3 KHz NMI timer, NMI every 4 ms = 250 Hz,
    // but display refresh set by software at ~60 Hz)
    inline constexpr uint32_t TARGET_FPS           = 60;
    inline constexpr uint32_t CYCLES_PER_FRAME     = CPU_FREQ_HZ / TARGET_FPS;  // 25,200

    // NMI timer: 3 KHz clock, fires every ~4 ms (250 Hz)
    // In Asteroids, NMI fires every 4 ms; in Lunar Lander every 4.1 ms,
    // but both are close enough to model as 4 ms.
    inline constexpr uint32_t NMI_PERIOD_CYCLES    = CPU_FREQ_HZ / 250;  // ~6048 cycles

    // ========================================================================
    // Display (vector rasterization target)
    // ========================================================================

    inline constexpr int DISPLAY_WIDTH             = 1024;
    inline constexpr int DISPLAY_HEIGHT            = 1024;

    // ========================================================================
    // Audio
    // ========================================================================

    inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

    // ========================================================================
    // Common memory map regions (shared between both games)
    // ========================================================================

    // Zero-page / work RAM
    inline constexpr uint16_t RAM_BASE             = 0x0000;
    inline constexpr uint16_t RAM_SIZE             = 0x0400;    // 1 KB (both games)

    // I/O read addresses (active when A15=0, A13=1)
    inline constexpr uint16_t IO_READ_BASE         = 0x2000;

    // I/O write addresses (active when A15=0, A13=1, active write cycle)
    inline constexpr uint16_t IO_WRITE_BASE        = 0x3000;

    // Vector RAM (read/write by both CPU and DVG)
    inline constexpr uint16_t VECRAM_BASE          = 0x4000;
    inline constexpr uint16_t VECRAM_SIZE          = 0x0800;    // 2 KB

    // Vector ROM (read-only, shared DVG + CPU)
    inline constexpr uint16_t VECROM_BASE          = 0x5000;
    inline constexpr uint16_t VECROM_SIZE          = 0x0800;    // 2 KB

    // ========================================================================
    // I/O Read Map (active A13=1, active read)
    // ========================================================================
    //
    // Both games decode reads in the $2000-$2FFF range.  Specific addresses:
    //
    // Asteroids:
    //   $2000 — IN0: coins, self-test, slam, diagnostic step
    //   $2001 — IN1: P1 controls (left, right, fire, thrust, hyperspace)
    //   $2002 — IN2: P2 controls (unused in 1-player Asteroids)
    //   $2003 — — (unused)
    //   $2800 — DSW1: DIP switches (lives, language, bonus)
    //   $2801 — DSW2: DIP switches (coinage)
    //
    // Lunar Lander:
    //   $2000 — IN0: coins, self-test, slam
    //   $2001 — IN1: abort, game select, start buttons
    //   $2400 — Thrust lever (4-bit ADC value)
    //   $2800 — DSW1: DIP switches (fuel, coinage)
    //   $2801 — DSW2: DIP switches (language, bonus)
    //   $2C00 — DVG status (bit 7 = HALT flag)

    inline constexpr uint16_t IN0_ADDR             = 0x2000;
    inline constexpr uint16_t IN1_ADDR             = 0x2001;
    inline constexpr uint16_t IN2_ADDR             = 0x2002;  // Asteroids only
    inline constexpr uint16_t THRUST_ADC_ADDR      = 0x2400;  // Lunar Lander only
    inline constexpr uint16_t DSW1_ADDR            = 0x2800;
    inline constexpr uint16_t DSW2_ADDR            = 0x2801;
    inline constexpr uint16_t DVG_STATUS_ADDR      = 0x2C00;  // Bit 7 = HALT

    // ========================================================================
    // I/O Write Map (active A13=1, active write)
    // ========================================================================
    //
    // Both games decode writes in the $3000-$3FFF range:
    //
    //   $3000 — VGGO (trigger DVG)
    //   $3200 — VGRST (reset DVG)
    //   $3400 — WD CLR (watchdog clear)
    //   $3600 — Explosion sound trigger (Asteroids)
    //           Thrust sound / lamp control (Lunar Lander)
    //   $3800-$3BFF — Sound / output latches (game-specific)
    //   $3C00 — Coin counter
    //   $3E00 — NMI acknowledge (clears NMI flip-flop)

    inline constexpr uint16_t VGGO_ADDR            = 0x3000;
    inline constexpr uint16_t VGRST_ADDR           = 0x3200;
    inline constexpr uint16_t WDCLR_ADDR           = 0x3400;
    inline constexpr uint16_t SND_BASE_ADDR        = 0x3600;
    inline constexpr uint16_t COIN_CTR_ADDR        = 0x3C00;
    inline constexpr uint16_t NMI_ACK_ADDR         = 0x3E00;

    // ========================================================================
    // Asteroids-specific program ROM
    // ========================================================================

    // Asteroids program ROM: $6800-$7FFF (6 KB actual).
    // Manifest requires power-of-2 sizes; allocate 8 KB from $6000 and
    // leave the first 2 KB ($6000-$67FF) as open bus / unused.
    inline constexpr uint16_t AST_PROGROM_BASE     = 0x6000;
    inline constexpr uint16_t AST_PROGROM_SIZE     = 0x2000;  // 8 KB (manifest-aligned)
    inline constexpr uint16_t AST_PROGROM_ACTUAL   = 0x1800;  // 6 KB actual ROM
    inline constexpr uint16_t AST_PROGROM_OFFSET   = 0x0800;  // ROM data starts at byte $800 within the 8 KB

    // ========================================================================
    // Lunar Lander-specific program ROM
    // ========================================================================

    // Lunar Lander program ROM: $6000-$7FFF (8 KB)
    inline constexpr uint16_t LL_PROGROM_BASE      = 0x6000;
    inline constexpr uint16_t LL_PROGROM_SIZE      = 0x2000;  // 8 KB

    // ========================================================================
    // Asteroids IN0 bit definitions (active low)
    // ========================================================================

    inline constexpr uint8_t AST_IN0_CLOCK         = 0x01;  // 3 KHz clock (bit 0)
    inline constexpr uint8_t AST_IN0_SELF_TEST     = 0x04;  // Self-test switch
    inline constexpr uint8_t AST_IN0_DIAG_STEP     = 0x08;  // Diagnostic step
    inline constexpr uint8_t AST_IN0_SLAM          = 0x10;  // Slam switch
    inline constexpr uint8_t AST_IN0_HALT          = 0x20;  // DVG HALT status
    inline constexpr uint8_t AST_IN0_COIN_R        = 0x40;  // Right coin
    inline constexpr uint8_t AST_IN0_COIN_C        = 0x80;  // Center coin

    // ========================================================================
    // Asteroids IN1 bit definitions (active low)
    // ========================================================================

    inline constexpr uint8_t AST_IN1_FIRE          = 0x04;  // Fire button (active low)
    inline constexpr uint8_t AST_IN1_THRUST        = 0x08;  // Thrust (active low)
    inline constexpr uint8_t AST_IN1_ROT_RIGHT     = 0x10;  // Rotate right (active low)
    inline constexpr uint8_t AST_IN1_ROT_LEFT      = 0x20;  // Rotate left (active low)
    inline constexpr uint8_t AST_IN1_HYPERSPACE    = 0x40;  // Hyperspace (active low)
    inline constexpr uint8_t AST_IN1_2P_START      = 0x80;  // 2-player start

    // ========================================================================
    // Lunar Lander IN0 bit definitions
    // ========================================================================

    inline constexpr uint8_t LL_IN0_COIN           = 0x01;  // Coin deposit
    inline constexpr uint8_t LL_IN0_SELF_TEST      = 0x04;  // Self-test
    inline constexpr uint8_t LL_IN0_SLAM           = 0x10;  // Slam switch
    inline constexpr uint8_t LL_IN0_HALT           = 0x20;  // DVG HALT status
    inline constexpr uint8_t LL_IN0_START          = 0x40;  // Start button
    inline constexpr uint8_t LL_IN0_SELECT         = 0x80;  // Select button

}  // namespace atari_vector_constants
