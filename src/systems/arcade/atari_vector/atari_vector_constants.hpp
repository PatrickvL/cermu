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
    ASTEROIDS_DELUXE,
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
    // Asteroids: 1 × 2 KB chip at $5000
    // Lunar Lander: 2 × 2 KB chips at $4800-$57FF (same layout as AD)
    inline constexpr uint16_t VECROM_BASE          = 0x5000;
    inline constexpr uint16_t LL_VECROM_BASE       = 0x4800;    // LL ROM starts at $4800
    inline constexpr uint16_t AST_VECROM_SIZE      = 0x0800;    // 2 KB (Asteroids)
    inline constexpr uint16_t LL_VECROM_SIZE       = 0x1000;    // 4 KB (Lunar Lander)

    // ========================================================================
    // I/O Read Map (active A13=1, active read)
    // ========================================================================
    //
    // Both games decode reads in the $2000-$2FFF range.  Input reads
    // use MULTIPLEXED access via 74LS244 buffers: reading address
    // $200X returns bit X of the port value, placed at D7.  The
    // BIT instruction can then test specific inputs via BMI/BPL.
    //
    // Asteroids:
    //   $2000-$2007 — IN0 (multiplexed): DVG halt, 3KHz clock,
    //                 hyperspace, fire, diag step, tilt, self-test
    //   $2400-$2407 — IN1 (multiplexed): coins, start, thrust,
    //                 rotate left/right
    //   $2800-$2803 — DSW1 (multiplexed): DIP switches
    //
    // Lunar Lander:
    //   $2000       — IN0 (direct, full byte): DVG halt, self-test,
    //                 tilt, 3KHz clock, diagnostic step
    //   $2400-$2407 — IN1 (multiplexed): start, coins, select,
    //                 abort, rotate left/right
    //   $2800-$2803 — DSW1 (multiplexed): DIP switches
    //   $2C00       — Thrust lever ADC (direct, 8-bit value)

    inline constexpr uint16_t IN0_BASE             = 0x2000;
    inline constexpr uint16_t IN1_BASE             = 0x2400;
    inline constexpr uint16_t DSW1_BASE            = 0x2800;
    inline constexpr uint16_t THRUST_ADC_ADDR      = 0x2C00;  // Lunar Lander only

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
    // Asteroids Deluxe-specific constants
    // ========================================================================

    // AD program ROM: $6000-$7FFF (8 KB — 4 × 2 KB chips)
    inline constexpr uint16_t AD_PROGROM_BASE      = 0x6000;
    inline constexpr uint16_t AD_PROGROM_SIZE      = 0x2000;  // 8 KB

    // AD vector ROM: $4800-$57FF (4 KB — 2 × 2 KB chips)
    // In the DVG address space: word 0x400-0xBFF (no gap after RAM)
    inline constexpr uint16_t AD_VECROM_BASE       = 0x4800;
    inline constexpr uint16_t AD_VECROM_SIZE       = 0x1000;  // 4 KB

    // EAROM: ER2055 at $2C00-$2C0F (64×4-bit nonvolatile memory)
    inline constexpr uint16_t AD_EAROM_BASE        = 0x2C00;
    inline constexpr uint16_t AD_EAROM_SIZE        = 0x40;    // 64 bytes address space

    // POKEY sound chip at $2600-$260F
    inline constexpr uint16_t AD_POKEY_BASE        = 0x2600;

    // EAROM control register
    inline constexpr uint16_t AD_EAROM_CTRL_ADDR   = 0x3800;

    // ========================================================================
    // Lunar Lander-specific program ROM
    // ========================================================================

    // Lunar Lander program ROM: $6000-$7FFF (8 KB)
    inline constexpr uint16_t LL_PROGROM_BASE      = 0x6000;
    inline constexpr uint16_t LL_PROGROM_SIZE      = 0x2000;  // 8 KB

    // ========================================================================
    // Asteroids IN0 bit definitions (active-HIGH unless noted)
    // ========================================================================
    //
    // Matches MAME asteroid.cpp INPUT_PORTS.  All inputs are active-HIGH
    // (pressed = 1) except DVG HALT which is active-LOW (running = 1).
    // Multiplexed: reading $200X returns bit X at D7.

    inline constexpr uint8_t AST_IN0_UNUSED0       = 0x01;  // bit 0: unused
    inline constexpr uint8_t AST_IN0_CLOCK         = 0x02;  // bit 1: 3 KHz clock
    inline constexpr uint8_t AST_IN0_HALT          = 0x04;  // bit 2: DVG HALT (active-LOW: halted=0, running=1)
    inline constexpr uint8_t AST_IN0_HYPERSPACE    = 0x08;  // bit 3: Hyperspace
    inline constexpr uint8_t AST_IN0_FIRE          = 0x10;  // bit 4: Fire
    inline constexpr uint8_t AST_IN0_DIAG_STEP     = 0x20;  // bit 5: Diagnostic step
    inline constexpr uint8_t AST_IN0_TILT          = 0x40;  // bit 6: Tilt
    inline constexpr uint8_t AST_IN0_SELF_TEST     = 0x80;  // bit 7: Self-test (PORT_SERVICE)

    // ========================================================================
    // Asteroids IN1 bit definitions (active-HIGH, pressed = 1)
    // ========================================================================
    //
    // Matches MAME asteroid.cpp INPUT_PORTS.
    // Multiplexed: reading $240X returns bit X at D7.

    inline constexpr uint8_t AST_IN1_COIN1         = 0x01;  // bit 0: Coin Left
    inline constexpr uint8_t AST_IN1_COIN2         = 0x02;  // bit 1: Coin Center
    inline constexpr uint8_t AST_IN1_COIN3         = 0x04;  // bit 2: Coin Right
    inline constexpr uint8_t AST_IN1_1P_START      = 0x08;  // bit 3: 1-player start
    inline constexpr uint8_t AST_IN1_2P_START      = 0x10;  // bit 4: 2-player start
    inline constexpr uint8_t AST_IN1_THRUST        = 0x20;  // bit 5: Thrust
    inline constexpr uint8_t AST_IN1_ROT_RIGHT     = 0x40;  // bit 6: Rotate right
    inline constexpr uint8_t AST_IN1_ROT_LEFT      = 0x80;  // bit 7: Rotate left

    // ========================================================================
    // Asteroids Deluxe IN0/IN1 bit definitions
    // ========================================================================
    //
    // Matches MAME asteroid.cpp astdelux INPUT_PORTS.
    // IN0 is the same as Asteroids except bit 3 = shields (was hyperspace).
    // IN1 rearranges controls:
    //   bits 0-4: same as Asteroids (coins, starts)
    //   bit 5: rotate left  (joystick 2-way left)
    //   bit 6: rotate right (joystick 2-way right)
    //   bit 7: fire (button 1)

    inline constexpr uint8_t AD_IN0_SHIELDS        = 0x08;  // bit 3: Shields (replaces hyperspace)
    inline constexpr uint8_t AD_IN0_THRUST         = 0x10;  // bit 4: Thrust
    inline constexpr uint8_t AD_IN1_ROT_LEFT       = 0x20;  // bit 5: Rotate left
    inline constexpr uint8_t AD_IN1_ROT_RIGHT      = 0x40;  // bit 6: Rotate right
    inline constexpr uint8_t AD_IN1_FIRE           = 0x80;  // bit 7: Fire

    // ========================================================================
    // Lunar Lander IN0 bit definitions (active-HIGH unless noted)
    // ========================================================================
    //
    // Matches MAME asteroid.cpp llander INPUT_PORTS.
    // Lunar Lander IN0 is a direct (non-multiplexed) full-byte read at $2000.

    inline constexpr uint8_t LL_IN0_HALT           = 0x01;  // bit 0: DVG HALT (active-HIGH: done_r)
    inline constexpr uint8_t LL_IN0_SELF_TEST      = 0x02;  // bit 1: Self-test (active-LOW)
    inline constexpr uint8_t LL_IN0_TILT           = 0x04;  // bit 2: Tilt (active-LOW)
    // bits 3-5: unknown (active-LOW)
    inline constexpr uint8_t LL_IN0_CLOCK          = 0x40;  // bit 6: 3 KHz clock
    inline constexpr uint8_t LL_IN0_DIAG_STEP      = 0x80;  // bit 7: Diagnostic step (active-LOW)

    // Polarity masks for IP_ACTIVE_LOW bits.
    // Internal state uses active-HIGH convention (1=pressed). These masks are
    // XORed in io_read to produce the hardware-expected active-LOW signals
    // (idle=1, active=0) for the bits that the real hardware active-pulls.
    inline constexpr uint8_t LL_IN0_ACTIVE_LOW_MASK = 0xBE;  // bits 1,2,3,4,5,7
    inline constexpr uint8_t LL_IN1_ACTIVE_LOW_MASK = 0xCE;  // bits 1,2,3,6,7

}  // namespace atari_vector_constants
