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
    // DVG I/O Write Map (active A13=1, active write)
    // ========================================================================
    //
    // Both DVG games decode writes in the $3000-$3FFF range:
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
    // Asteroids IN0 bit definitions (active-HIGH unless noted)
    // ========================================================================
    //
    // Matches MAME asteroid.cpp INPUT_PORTS.  All inputs are active-HIGH
    // (pressed = 1) except DVG HALT which is active-LOW (running = 1).
    // Multiplexed: reading $200X returns bit X at D7.

    inline constexpr uint8_t AST_IN0_CLOCK         = 0x02;  // bit 1: 3 KHz clock
    inline constexpr uint8_t AST_IN0_HALT          = 0x04;  // bit 2: DVG HALT (active-LOW: halted=0, running=1)
    inline constexpr uint8_t AST_IN0_HYPERSPACE    = 0x08;  // bit 3: Hyperspace
    inline constexpr uint8_t AST_IN0_FIRE          = 0x10;  // bit 4: Fire

    // ========================================================================
    // Asteroids IN1 bit definitions (active-HIGH, pressed = 1)
    // ========================================================================
    //
    // Matches MAME asteroid.cpp INPUT_PORTS.
    // Multiplexed: reading $240X returns bit X at D7.

    inline constexpr uint8_t AST_IN1_COIN1         = 0x01;  // bit 0: Coin Left
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
    // Asteroids Deluxe I/O
    // ========================================================================

    // POKEY sound chip at $2600-$260F
    inline constexpr uint16_t AD_POKEY_BASE        = 0x2600;

    // EAROM: ER2055 at $2C00-$2C0F (64×4-bit nonvolatile memory)
    inline constexpr uint16_t AD_EAROM_BASE        = 0x2C00;
    inline constexpr uint16_t AD_EAROM_SIZE        = 0x40;    // 64 bytes address space

    // ========================================================================
    // Lunar Lander IN0 bit definitions (active-HIGH unless noted)
    // ========================================================================
    //
    // Matches MAME asteroid.cpp llander INPUT_PORTS.
    // Lunar Lander IN0 is a direct (non-multiplexed) full-byte read at $2000.

    inline constexpr uint8_t LL_IN0_HALT           = 0x01;  // bit 0: DVG HALT (active-HIGH: done_r)
    inline constexpr uint8_t LL_IN0_CLOCK          = 0x40;  // bit 6: 3 KHz clock

    // Polarity masks for IP_ACTIVE_LOW bits.
    // Internal state uses active-HIGH convention (1=pressed). These masks are
    // XORed in io_read to produce the hardware-expected active-LOW signals
    // (idle=1, active=0) for the bits that the real hardware active-pulls.
    inline constexpr uint8_t LL_IN0_ACTIVE_LOW_MASK = 0xBE;  // bits 1,2,3,4,5,7
    inline constexpr uint8_t LL_IN1_ACTIVE_LOW_MASK = 0xCE;  // bits 1,2,3,6,7

    // Battlezone / Red Baron IN0 active-LOW mask.
    // bits 0(coin1), 1(coin2), 2-3(unused), 4(self-test), 5(diag step) are IP_ACTIVE_LOW.
    inline constexpr uint8_t BZ_IN0_ACTIVE_LOW_MASK = 0x3F;

    // ========================================================================
    // Battlezone I/O (MAME bzone.cpp)
    // ========================================================================
    //
    // Battlezone (1980): 6502 + AVG (AVG_BZONE variant), discrete audio.
    // Memory map:
    //   Work RAM:    $0000-$03FF (1 KB)
    //   I/O:         $0800-$1FFF (various decode — IN0, DSW, VGGO, VGRST)
    //   Vector RAM:  $2000-$2FFF (4 KB — shared CPU + AVG)
    //   Vector ROM:  $3000-$3FFF (4 KB — 2 × 2 KB)
    //   Program ROM: $5000-$7FFF (12 KB — 6 × 2 KB chips)

    inline constexpr uint16_t BZ_IN0_ADDR          = 0x0800;    // read: IN0 (direct byte)
    inline constexpr uint16_t BZ_DSW0_ADDR         = 0x0A00;    // read: DSW0
    inline constexpr uint16_t BZ_DSW1_ADDR         = 0x0C00;    // read: DSW1

    inline constexpr uint16_t BZ_COIN_CTR_ADDR     = 0x1000;    // write: coin counters
    inline constexpr uint16_t BZ_VGGO_ADDR         = 0x1200;    // write: trigger AVG (MAME bzone.cpp: avg_device::go_w)
    inline constexpr uint16_t BZ_WDCLR_ADDR        = 0x1400;    // write: watchdog clear (MAME bzone.cpp)
    inline constexpr uint16_t BZ_VGRST_ADDR        = 0x1600;    // write: reset AVG (MAME bzone.cpp: avg_device::reset_w)
    inline constexpr uint16_t BZ_SND_ADDR          = 0x1840;    // write: sound latch (MAME bzone_sounds_w)

    // Battlezone IN0 bit definitions (MAME bzone.cpp BZONEIN0)
    // Bits 0-4: Coin1(active-LOW), Coin2(active-LOW), unused, unused, Self-test(active-LOW)
    // Bit 5: Diagnostic step (active-LOW)
    // Bit 6: VG HALT (IP_ACTIVE_HIGH = done_r)
    // Bit 7: 3 KHz clock (IP_ACTIVE_HIGH)
    inline constexpr uint8_t BZ_IN0_COIN1          = 0x01;  // bit 0: Coin 1 (active-LOW)
    inline constexpr uint8_t BZ_IN0_HALT           = 0x40;  // bit 6: VG HALT (IP_ACTIVE_HIGH)
    inline constexpr uint8_t BZ_IN0_CLOCK          = 0x80;  // bit 7: 3 KHz clock

    // ========================================================================
    // Red Baron I/O (MAME bzone.cpp)
    // ========================================================================
    //
    // Red Baron (1980): 6502 + AVG + POKEY, yoke controller.
    // I/O addresses match Battlezone except POKEY at $1810-$181F.

    inline constexpr uint16_t RB_POKEY_BASE        = 0x1810;    // POKEY at $1810-$181F

    // ========================================================================
    // Tempest I/O (MAME tempest.cpp)
    // ========================================================================
    //
    // Tempest (1980): 6502 + AVG + POKEY (x2), color vector, spinner input
    // Memory map:
    //   $0000-$07FF: RAM (2 KB)
    //   $2000-$2FFF: Vector RAM (4 KB)
    //   $3000-$3FFF: Vector ROM (4 KB)
    //   $4800: VGGO, $5000: WD CLR, $5800: VGRST
    //   $6000-$603F: EAROM, $6040: EAROM ctrl, $6050: EAROM data
    //   $60C0-$60CF: POKEY 1, $60D0-$60DF: POKEY 2
    //   $9000-$DFFF: Program ROM (mirrored to $E000-$FFFF)

    inline constexpr uint16_t TEMP_POKEY1_BASE     = 0x60C0;    // POKEY 1
    inline constexpr uint16_t TEMP_POKEY2_BASE     = 0x60D0;    // POKEY 2 (secondary)
    inline constexpr uint16_t TEMP_EAROM_BASE      = 0x6000;    // MAME: $6000-$603F (write)
    inline constexpr uint16_t TEMP_EAROM_SIZE      = 0x0040;    // 64 bytes
    inline constexpr uint16_t TEMP_EAROM_READ_ADDR = 0x6050;    // MAME: earom_read
    inline constexpr uint16_t TEMP_EAROM_CTRL_ADDR = 0x6040;    // MAME: earom_control_w / mathbox_status_r
    inline constexpr uint16_t TEMP_VGGO_ADDR       = 0x4800;    // MAME tempest.cpp: avg_device::go_w
    inline constexpr uint16_t TEMP_VGRST_ADDR      = 0x5800;    // MAME tempest.cpp: avg_device::reset_w
    inline constexpr uint16_t TEMP_WDCLR_ADDR      = 0x5000;    // MAME tempest.cpp: wdclr_w

    // ========================================================================
    // Gravitar / Black Widow I/O (MAME bwidow.cpp — shared board)
    // ========================================================================
    //
    // Gravitar (1982) and Black Widow (1982) share the "bwidow" board.
    // Memory map:
    //   $0000-$07FF: RAM (2 KB)
    //   $2000-$27FF: Vector RAM (2 KB)
    //   $2800-$5FFF: Vector ROM (14 KB)
    //   $6000: POKEY 1, $6800: POKEY 2
    //   $7800: IN0, $8000: IN3, $8800: IN4
    //   $8840: VGGO, $8880: VGRST, $88C0: IRQACK, $8980: WDCLR
    //   $9000-$FFFF: Program ROM (28 KB)

    inline constexpr uint16_t GRAV_POKEY1_BASE     = 0x6000;    // POKEY 1 (MAME bwidow.cpp: $6000)
    inline constexpr uint16_t GRAV_POKEY2_BASE     = 0x6800;    // POKEY 2 (MAME bwidow.cpp: $6800)
    inline constexpr uint16_t GRAV_IN0_ADDR        = 0x7800;    // IN0 (MAME: $7800)
    inline constexpr uint16_t GRAV_IN3_ADDR        = 0x8000;    // IN3 (MAME: $8000)
    inline constexpr uint16_t GRAV_IN4_ADDR        = 0x8800;    // IN4 (MAME: $8800)
    inline constexpr uint16_t GRAV_VGGO_ADDR       = 0x8840;    // write: trigger AVG
    inline constexpr uint16_t GRAV_VGRST_ADDR      = 0x8880;    // write: reset AVG
    inline constexpr uint16_t GRAV_IRQACK_ADDR     = 0x88C0;    // write: IRQ acknowledge
    inline constexpr uint16_t GRAV_WDCLR_ADDR      = 0x8980;    // write: watchdog clear (MAME: $8980)

    // ========================================================================
    // Space Duel I/O (MAME spacduel_map)
    // ========================================================================
    //
    // Space Duel (1982): AVG + POKEY, color, two-player simultaneous.
    // Different board from bwidow — separate I/O decode.
    //   RAM $0000-$03FF (1KB), Vec RAM $2000-$27FF, Vec ROM $2800-$3FFF,
    //   Prog ROM $4000-$FFFF (two regions), POKEY1 $1000, POKEY2 $1400.

    inline constexpr uint16_t SD_IN0_ADDR          = 0x0800;    // IN0 (MAME: $0800)
    inline constexpr uint16_t SD_POKEY1_BASE       = 0x1000;    // POKEY 1 (MAME: $1000)
    inline constexpr uint16_t SD_POKEY2_BASE       = 0x1400;    // POKEY 2 (MAME: $1400)
    inline constexpr uint16_t SD_VGGO_ADDR         = 0x0C80;    // write: trigger AVG
    inline constexpr uint16_t SD_VGRST_ADDR        = 0x0D80;    // write: reset AVG
    inline constexpr uint16_t SD_WDCLR_ADDR        = 0x0D00;    // write: watchdog clear (MAME: $0D00)
    inline constexpr uint16_t SD_IRQACK_ADDR       = 0x0E00;    // write: IRQ acknowledge (MAME: $0E00)

    // ========================================================================
    // Major Havoc I/O (MAME mhavoc.cpp)
    // ========================================================================
    //
    // Major Havoc (1983): AVG + POKEY + TMS5220, color, spinner.
    // More complex memory map — has bank switching for extra program ROM.

    inline constexpr uint16_t MH_POKEY1_BASE       = 0x1200;
    inline constexpr uint16_t MH_VGGO_ADDR         = 0x1400;    // write: trigger AVG
    inline constexpr uint16_t MH_VGRST_ADDR        = 0x1600;    // write: reset AVG
    inline constexpr uint16_t MH_WDCLR_ADDR        = 0x1800;    // write: watchdog clear

}  // namespace atari_vector_constants
