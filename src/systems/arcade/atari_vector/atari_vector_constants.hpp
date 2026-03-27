#pragma once
/*
 * atari_vector_constants.hpp — Shared constants for Atari vector arcade systems
 *
 * Defines hardware constants for all Atari 6502-based vector arcade games:
 *
 * DVG-based (Digital Vector Generator):
 *   Lunar Lander (1979), Asteroids (1979), Asteroids Deluxe (1980)
 *
 * AVG-based (Analog Vector Generator):
 *   Battlezone (1980), Red Baron (1980), Tempest (1980),
 *   Gravitar (1982), Space Duel (1982),
 *   Black Widow (1982), Major Havoc (1983)
 *
 * All share the same general architecture:
 *   - MOS 6502 CPU @ 1.512 MHz (12.096 MHz / 8)
 *   - DVG or AVG vector display processor
 *   - POKEY sound chip (later games; early games use discrete)
 *   - DIP switches, coin inputs, player controls
 *   - I/O decode at $2000-$3FFF (reads=inputs, writes=outputs)
 *   - Vector RAM at $2000+ (AVG) or $4000+ (DVG)
 *   - Program ROM at $4000+ (AVG) or $6000+ (DVG)
 */

#include <cstdint>

// ============================================================================
// Game variant enum
// ============================================================================

enum class AtariVectorVariant : uint8_t {
    // DVG-based
    ASTEROIDS,
    ASTEROIDS_DELUXE,
    LUNAR_LANDER,
    BATTLEZONE,
    RED_BARON,

    // AVG-based
    TEMPEST,
    GRAVITAR,
    SPACE_DUEL,
    BLACK_WIDOW,
    MAJOR_HAVOC,
};

namespace atari_vector_constants {

    // ========================================================================
    // Timing
    // ========================================================================

    // Master clock: 12.096 MHz crystal, CPU gets /8 = 1.512 MHz
    inline constexpr uint32_t CPU_FREQ_HZ          = 12096000 / 8;  // 1,512,000 Hz

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
    // Input bit definitions
    // ========================================================================
    //
    // Per-game input port bit masks.  These define which bit positions in
    // the hardware input registers correspond to specific buttons, coins,
    // and status signals.  Used in game-specific io_read() formatting logic.
    //
    // I/O ADDRESS constants (VGGO, VGRST, WDCLR, POKEY base, EAROM base,
    // etc.) are declared in AtariVectorTraits<V> and wired through the
    // ChipManifest — they do not appear here.

    // ── Asteroids IN0 (active-HIGH unless noted) ────────────────────────
    // Multiplexed: reading $200X returns bit X at D7.

    inline constexpr uint8_t AST_IN0_CLOCK         = 0x02;  // bit 1: 3 KHz clock
    inline constexpr uint8_t AST_IN0_HALT          = 0x04;  // bit 2: DVG HALT (active-LOW: halted=0, running=1)
    inline constexpr uint8_t AST_IN0_HYPERSPACE    = 0x08;  // bit 3: Hyperspace
    inline constexpr uint8_t AST_IN0_FIRE          = 0x10;  // bit 4: Fire

    // ── Asteroids IN1 (active-HIGH, pressed = 1) ───────────────────────
    // Multiplexed: reading $240X returns bit X at D7.

    inline constexpr uint8_t AST_IN1_COIN1         = 0x01;  // bit 0: Coin Left
    inline constexpr uint8_t AST_IN1_1P_START      = 0x08;  // bit 3: 1-player start
    inline constexpr uint8_t AST_IN1_2P_START      = 0x10;  // bit 4: 2-player start
    inline constexpr uint8_t AST_IN1_THRUST        = 0x20;  // bit 5: Thrust
    inline constexpr uint8_t AST_IN1_ROT_RIGHT     = 0x40;  // bit 6: Rotate right
    inline constexpr uint8_t AST_IN1_ROT_LEFT      = 0x80;  // bit 7: Rotate left

    // ── Asteroids Deluxe IN0/IN1 ────────────────────────────────────────
    // IN0 same as Asteroids except bit 3 = shields.
    // IN1 rearranges controls: bit 5=rot left, bit 6=rot right, bit 7=fire.

    inline constexpr uint8_t AD_IN0_SHIELDS        = 0x08;  // bit 3: Shields (replaces hyperspace)
    inline constexpr uint8_t AD_IN0_THRUST         = 0x10;  // bit 4: Thrust
    inline constexpr uint8_t AD_IN1_ROT_LEFT       = 0x20;  // bit 5: Rotate left
    inline constexpr uint8_t AD_IN1_ROT_RIGHT      = 0x40;  // bit 6: Rotate right
    inline constexpr uint8_t AD_IN1_FIRE           = 0x80;  // bit 7: Fire

    // ── Lunar Lander IN0 (active-HIGH unless noted) ─────────────────────
    // Direct (non-multiplexed) full-byte read at $2000.

    inline constexpr uint8_t LL_IN0_HALT           = 0x01;  // bit 0: DVG HALT (active-HIGH: done_r)
    inline constexpr uint8_t LL_IN0_CLOCK          = 0x40;  // bit 6: 3 KHz clock

    // Polarity masks for IP_ACTIVE_LOW bits.
    // Internal state uses active-HIGH convention (1=pressed). These masks are
    // XORed in io_read to produce the hardware-expected active-LOW signals
    // (idle=1, active=0) for the bits that the real hardware active-pulls.
    inline constexpr uint8_t LL_IN0_ACTIVE_LOW_MASK = 0xBE;  // bits 1,2,3,4,5,7
    inline constexpr uint8_t LL_IN1_ACTIVE_LOW_MASK = 0xCE;  // bits 1,2,3,6,7

    // ── Battlezone / Red Baron IN0 ──────────────────────────────────────
    // bits 0(coin1), 1(coin2), 2-3(unused), 4(self-test), 5(diag step) are IP_ACTIVE_LOW.

    inline constexpr uint8_t BZ_IN0_ACTIVE_LOW_MASK = 0x3F;
    inline constexpr uint8_t BZ_IN0_COIN1          = 0x01;  // bit 0: Coin 1 (active-LOW)
    inline constexpr uint8_t BZ_IN0_HALT           = 0x40;  // bit 6: VG HALT (IP_ACTIVE_HIGH)
    inline constexpr uint8_t BZ_IN0_CLOCK          = 0x80;  // bit 7: 3 KHz clock

}  // namespace atari_vector_constants
