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

    // Battlezone / Red Baron IN0 active-LOW mask.
    // bits 0(coin1), 1(coin2), 2-3(unused), 4(self-test), 5(diag step) are IP_ACTIVE_LOW.
    inline constexpr uint8_t BZ_IN0_ACTIVE_LOW_MASK = 0x3F;

    // ========================================================================
    // Battlezone-specific constants
    // ========================================================================
    //
    // Battlezone (1980): 6502 + AVG (AVG_BZONE variant), discrete audio.
    // Memory map (from MAME bzone.cpp):
    //   Work RAM:    $0000-$03FF (1 KB)
    //   I/O:         $0800-$1FFF (various decode — IN0, DSW, VGGO, VGRST)
    //   Vector RAM:  $2000-$2FFF (4 KB — shared CPU + AVG)
    //   Vector ROM:  $3000-$3FFF (4 KB — 2 × 2 KB)
    //   Program ROM: $5000-$7FFF (12 KB — 6 × 2 KB chips)

    inline constexpr uint16_t BZ_RAM_BASE          = 0x0000;
    inline constexpr uint16_t BZ_RAM_SIZE          = 0x0400;    // 1 KB (MAME: $0000-$03FF)

    inline constexpr uint16_t BZ_VECRAM_BASE       = 0x2000;
    inline constexpr uint16_t BZ_VECRAM_SIZE       = 0x1000;    // 4 KB (AVG: $2000-$2FFF)
    inline constexpr uint16_t BZ_VECROM_BASE       = 0x3000;
    inline constexpr uint16_t BZ_VECROM_SIZE       = 0x1000;    // 4 KB

    inline constexpr uint16_t BZ_PROGROM_BASE      = 0x4000;
    inline constexpr uint16_t BZ_PROGROM_SIZE      = 0x4000;    // 16 KB (manifest-aligned, actual ROM at $5000-$7FFF)
    inline constexpr uint16_t BZ_PROGROM_ACTUAL    = 0x3000;    // 12 KB actual ROM
    inline constexpr uint16_t BZ_PROGROM_OFFSET    = 0x1000;    // ROM data at offset $1000 within 16 KB slot

    // I/O decode — Battlezone uses $0800-$1FFF (MAME bzone.cpp)
    inline constexpr uint16_t BZ_IO_BASE            = 0x0800;

    inline constexpr uint16_t BZ_IN0_ADDR          = 0x0800;    // read: IN0 (direct byte)
    inline constexpr uint16_t BZ_DSW0_ADDR         = 0x0A00;    // read: DSW0
    inline constexpr uint16_t BZ_DSW1_ADDR         = 0x0C00;    // read: DSW1

    inline constexpr uint16_t BZ_COIN_CTR_ADDR     = 0x1000;    // write: coin counters
    inline constexpr uint16_t BZ_VGGO_ADDR         = 0x1200;    // write: trigger AVG (MAME bzone.cpp: avg_device::go_w)
    inline constexpr uint16_t BZ_WDCLR_ADDR        = 0x1400;    // write: watchdog clear (MAME bzone.cpp)
    inline constexpr uint16_t BZ_VGRST_ADDR        = 0x1600;    // write: reset AVG (MAME bzone.cpp: avg_device::reset_w)
    inline constexpr uint16_t BZ_SND_ADDR          = 0x1840;    // write: sound latch (MAME bzone_sounds_w)

    // DVG word offset: RAM at word 0x000, ROM at word 0x800 (4 KB gap, then 4 KB ROM)
    inline constexpr uint16_t BZ_VECROM_WORD_OFFSET = 0x800;

    // Battlezone IN0 bit definitions (MAME bzone.cpp BZONEIN0)
    // Bits 0-4: Coin1(active-LOW), Coin2(active-LOW), unused, unused, Self-test(active-LOW)
    // Bit 5: Diagnostic step (active-LOW)
    // Bit 6: VG HALT (IP_ACTIVE_HIGH = done_r)
    // Bit 7: 3 KHz clock (IP_ACTIVE_HIGH)
    inline constexpr uint8_t BZ_IN0_COIN1          = 0x01;  // bit 0: Coin 1 (active-LOW)
    inline constexpr uint8_t BZ_IN0_COIN2          = 0x02;  // bit 1: Coin 2 (active-LOW)
    inline constexpr uint8_t BZ_IN0_SELF_TEST      = 0x10;  // bit 4: Self-test (active-LOW)
    inline constexpr uint8_t BZ_IN0_DIAG_STEP      = 0x20;  // bit 5: Diagnostic step (active-LOW)
    inline constexpr uint8_t BZ_IN0_HALT           = 0x40;  // bit 6: VG HALT (IP_ACTIVE_HIGH)
    inline constexpr uint8_t BZ_IN0_CLOCK          = 0x80;  // bit 7: 3 KHz clock

    // Joystick ports at separate addresses
    inline constexpr uint16_t BZ_JSR_ADDR          = 0x0800;    // joystick register (overlaps IN0)
    // bit fields for joystick: forward/reverse left/right

    // ========================================================================
    // Red Baron-specific constants
    // ========================================================================
    //
    // Red Baron (1980): 6502 + AVG + POKEY, yoke controller.
    // Very similar memory map to Battlezone but uses POKEY for sound.
    // I/O addresses match BZ except POKEY at $1810-$181F.

    inline constexpr uint16_t RB_RAM_BASE          = 0x0000;
    inline constexpr uint16_t RB_RAM_SIZE          = 0x0400;    // 1 KB (MAME: $0000-$03FF)

    inline constexpr uint16_t RB_VECRAM_BASE       = 0x2000;
    inline constexpr uint16_t RB_VECRAM_SIZE       = 0x1000;    // 4 KB (AVG: $2000-$2FFF)
    inline constexpr uint16_t RB_VECROM_BASE       = 0x3000;
    inline constexpr uint16_t RB_VECROM_SIZE       = 0x1000;    // 4 KB

    inline constexpr uint16_t RB_PROGROM_BASE      = 0x4000;
    inline constexpr uint16_t RB_PROGROM_SIZE      = 0x4000;    // 16 KB (manifest-aligned, actual ROM at $5000-$7FFF)
    inline constexpr uint16_t RB_PROGROM_ACTUAL    = 0x3000;    // 12 KB actual ROM
    inline constexpr uint16_t RB_PROGROM_OFFSET    = 0x1000;    // ROM data at offset $1000 within 16 KB slot

    // Red Baron I/O same base as Battlezone
    inline constexpr uint16_t RB_POKEY_BASE        = 0x1810;    // POKEY at $1810-$181F

    inline constexpr uint16_t RB_VECROM_WORD_OFFSET = 0x800;

    // ========================================================================
    // AVG-based game memory map constants
    // ========================================================================
    //
    // The AVG games (Tempest, Gravitar, Space Duel, Black Widow, Major Havoc)
    // share a different board architecture from the DVG games:
    //
    //   Work RAM:    $0000-$03FF (1 KB)
    //   Vector RAM:  $2000-$2FFF (4 KB) — shared CPU + AVG
    //   Vector ROM:  $3000-$3FFF (4 KB) — DVG/AVG address space
    //   Program ROM: $4000-$7FFF (16 KB)
    //   POKEY:       $60xx
    //   I/O:         $0800-$0FFF or $6000–$6FFF (game-dependent)
    //   EAROM:       $0C00-$0CFF (some games)
    //
    // All AVG games use a 16-bit address space with A15=0 giving $0000-$7FFF.

    // Tempest (1980): 6502 + AVG + POKEY (x2), color vector, spinner input
    // Memory: $0000–$03FF RAM, $0800 I/O, $2000–$2FFF VRAM, $3000–$3FFF VROM,
    //         $4000–$5FFF math ROM?, $6000–$7FFF program ROM
    // Tempest uses a 32 KB address space: A15 unused → $0000-$7FFF mirrors.
    inline constexpr uint16_t TEMP_RAM_BASE        = 0x0000;
    inline constexpr uint16_t TEMP_RAM_SIZE        = 0x0400;    // 1 KB
    inline constexpr uint16_t TEMP_VECRAM_BASE     = 0x2000;
    inline constexpr uint16_t TEMP_VECRAM_SIZE     = 0x1000;    // 4 KB
    inline constexpr uint16_t TEMP_VECROM_BASE     = 0x3000;
    inline constexpr uint16_t TEMP_VECROM_SIZE     = 0x1000;    // 4 KB
    inline constexpr uint16_t TEMP_PROGROM_BASE    = 0x8000;
    inline constexpr uint16_t TEMP_PROGROM_SIZE    = 0x8000;    // 32 KB ($8000-$FFFF, ROM at $9000-$DFFF + mirror)
    inline constexpr uint16_t TEMP_POKEY1_BASE     = 0x60C0;    // POKEY 1
    inline constexpr uint16_t TEMP_POKEY2_BASE     = 0x60D0;    // POKEY 2 (secondary)
    inline constexpr uint16_t TEMP_EAROM_BASE      = 0x6000;    // MAME: $6000-$603F (write)
    inline constexpr uint16_t TEMP_EAROM_SIZE      = 0x0040;    // 64 bytes
    inline constexpr uint16_t TEMP_EAROM_READ_ADDR = 0x6050;    // MAME: earom_read
    inline constexpr uint16_t TEMP_EAROM_CTRL_ADDR = 0x6040;    // MAME: earom_control_w / mathbox_status_r
    inline constexpr uint16_t TEMP_VGGO_ADDR       = 0x4800;    // MAME tempest.cpp: avg_device::go_w
    inline constexpr uint16_t TEMP_VGRST_ADDR      = 0x5800;    // MAME tempest.cpp: avg_device::reset_w
    inline constexpr uint16_t TEMP_WDCLR_ADDR      = 0x5000;    // MAME tempest.cpp: wdclr_w
    inline constexpr uint16_t TEMP_IN0_ADDR        = 0x0C00;
    inline constexpr uint16_t TEMP_IN1_ADDR        = 0x0D00;
    inline constexpr uint16_t TEMP_DSW1_ADDR       = 0x0E00;
    inline constexpr uint16_t TEMP_DSW2_ADDR       = 0x0E00;

    // AVG word offset for Tempest-family games
    // Vector RAM at word 0x000-0x7FF (4 KB), Vector ROM at word 0x800-0xFFF (4 KB)
    inline constexpr uint16_t TEMP_VECROM_WORD_OFFSET = 0x800;

    // Gravitar (1982): Uses "bwidow" board (MAME bwidow.cpp).
    // I/O at $0800-$1FFF, program ROM at $4000-$7FFF.
    // MAME maps: POKEY1 $8800→$0800, VGGO $8840→$0840, VGRST $8880→$0880,
    //            WD $88C0→$08C0, POKEY2 $8A00→$0A00, IN0 $0C00, IN1 $0D00, DSW $0E00.
    inline constexpr uint16_t GRAV_RAM_BASE        = 0x0000;
    inline constexpr uint16_t GRAV_RAM_SIZE        = 0x0800;    // 2 KB (MAME: $0000-$07FF)
    inline constexpr uint16_t GRAV_VECRAM_BASE     = 0x2000;
    inline constexpr uint16_t GRAV_VECRAM_SIZE     = 0x0800;    // 2 KB (MAME bwidow.cpp: $2000-$27FF)
    inline constexpr uint16_t GRAV_VECROM_BASE     = 0x2800;    // MAME: $2800-$5FFF
    inline constexpr uint16_t GRAV_VECROM_SIZE     = 0x3800;    // 14 KB (MAME bwidow.cpp)
    inline constexpr uint16_t GRAV_POKEY1_BASE     = 0x6000;    // POKEY 1 (MAME bwidow.cpp: $6000)
    inline constexpr uint16_t GRAV_POKEY2_BASE     = 0x6800;    // POKEY 2 (MAME bwidow.cpp: $6800)
    inline constexpr uint16_t GRAV_IN0_ADDR        = 0x7800;    // IN0 (MAME: $7800)
    inline constexpr uint16_t GRAV_IN3_ADDR        = 0x8000;    // IN3 (MAME: $8000)
    inline constexpr uint16_t GRAV_IN4_ADDR        = 0x8800;    // IN4 (MAME: $8800)
    inline constexpr uint16_t GRAV_VGGO_ADDR       = 0x8840;    // write: trigger AVG
    inline constexpr uint16_t GRAV_VGRST_ADDR      = 0x8880;    // write: reset AVG
    inline constexpr uint16_t GRAV_IRQACK_ADDR     = 0x88C0;    // write: IRQ acknowledge
    inline constexpr uint16_t GRAV_WDCLR_ADDR      = 0x8980;    // write: watchdog clear (MAME: $8980)

    // Gravitar program ROM: MAME loads at $9000-$FFFF ($E000 reload for vectors).
    // No program ROM in $4000-$5FFF range (that's vector ROM on bwidow board).
    inline constexpr uint16_t GRAV_PROGROM_BASE    = 0x9000;    // MAME bwidow.cpp
    inline constexpr uint16_t GRAV_PROGROM_SIZE    = 0x7000;    // 28 KB ($9000-$FFFF)

    // Space Duel (1982): AVG + POKEY, color, two-player simultaneous
    // MAME spacduel_map: different board from bwidow!
    //   RAM $0000-$03FF (1KB), IN0 $0800, IN3 $0900,
    //   POKEY1 $1000, POKEY2 $1400, VGGO $0C80, VGRST $0D80,
    //   Vec RAM $2000-$27FF, Vec ROM $2800-$3FFF, Prog ROM $4000-$FFFF.
    inline constexpr uint16_t SD_RAM_BASE          = 0x0000;
    inline constexpr uint16_t SD_RAM_SIZE          = 0x0400;    // 1 KB (MAME: $0000-$03FF)
    inline constexpr uint16_t SD_VECRAM_BASE       = 0x2000;
    inline constexpr uint16_t SD_VECRAM_SIZE       = 0x0800;    // 2 KB (MAME: $2000-$27FF)
    inline constexpr uint16_t SD_VECROM_BASE       = 0x2800;    // MAME: $2800-$3FFF
    inline constexpr uint16_t SD_VECROM_SIZE       = 0x1800;    // 6 KB
    inline constexpr uint16_t SD_PROGROM_BASE      = 0x4000;
    inline constexpr uint16_t SD_PROGROM_SIZE      = 0x4000;

    // Space Duel high ROM: $8000-$FFFF (ROM at $8000 + mirrors for reset vector)
    inline constexpr uint16_t SD_PROGROM_HI_BASE   = 0x8000;
    inline constexpr uint16_t SD_PROGROM_HI_SIZE   = 0x8000;    // 32 KB ($8000-$FFFF)
    inline constexpr uint16_t SD_IN0_ADDR          = 0x0800;    // IN0 (MAME: $0800)
    inline constexpr uint16_t SD_POKEY1_BASE       = 0x1000;    // POKEY 1 (MAME: $1000)
    inline constexpr uint16_t SD_POKEY2_BASE       = 0x1400;    // POKEY 2 (MAME: $1400)
    inline constexpr uint16_t SD_VGGO_ADDR         = 0x0C80;    // write: trigger AVG
    inline constexpr uint16_t SD_VGRST_ADDR        = 0x0D80;    // write: reset AVG
    inline constexpr uint16_t SD_WDCLR_ADDR        = 0x0D00;    // write: watchdog clear (MAME: $0D00)
    inline constexpr uint16_t SD_IRQACK_ADDR       = 0x0E00;    // write: IRQ acknowledge (MAME: $0E00)

    // Black Widow (1982): AVG + POKEY, color, dual joystick
    // Same board as Gravitar (bwidow_map). Memory map identical to Gravitar.
    inline constexpr uint16_t BW_RAM_BASE          = 0x0000;
    inline constexpr uint16_t BW_RAM_SIZE          = 0x0800;
    inline constexpr uint16_t BW_VECRAM_BASE       = 0x2000;
    inline constexpr uint16_t BW_VECRAM_SIZE       = 0x0800;    // 2 KB (MAME bwidow.cpp: $2000-$27FF)
    inline constexpr uint16_t BW_VECROM_BASE       = 0x2800;    // MAME: $2800-$5FFF
    inline constexpr uint16_t BW_VECROM_SIZE       = 0x3800;    // 14 KB (MAME bwidow.cpp)
    // BW uses same POKEY, I/O addresses as Gravitar (shared bwidow_map)
    inline constexpr uint16_t BW_POKEY1_BASE       = 0x6000;    // POKEY 1 (MAME: $6000)
    inline constexpr uint16_t BW_POKEY2_BASE       = 0x6800;    // POKEY 2 (MAME: $6800)
    inline constexpr uint16_t BW_IN0_ADDR          = 0x7800;    // IN0 (MAME: $7800)
    inline constexpr uint16_t BW_VGGO_ADDR         = 0x8840;    // write: trigger AVG
    inline constexpr uint16_t BW_VGRST_ADDR        = 0x8880;    // write: reset AVG
    inline constexpr uint16_t BW_IRQACK_ADDR       = 0x88C0;    // write: IRQ acknowledge
    inline constexpr uint16_t BW_WDCLR_ADDR        = 0x8980;    // write: watchdog clear (MAME: $8980)

    // Black Widow program ROM: same layout as Gravitar (bwidow board).
    inline constexpr uint16_t BW_PROGROM_BASE      = 0x9000;    // MAME bwidow.cpp
    inline constexpr uint16_t BW_PROGROM_SIZE      = 0x7000;    // 28 KB ($9000-$FFFF)

    // Major Havoc (1983): AVG + POKEY + TMS5220, color, spinner
    // More complex memory map — has bank switching for extra program ROM
    // MAME mhavoc.cpp: VGGO $1400, VGRST $1600, IN0 $1200, POKEY1 $1200(?)
    inline constexpr uint16_t MH_RAM_BASE          = 0x0000;
    inline constexpr uint16_t MH_RAM_SIZE          = 0x0800;
    inline constexpr uint16_t MH_VECRAM_BASE       = 0x2000;
    inline constexpr uint16_t MH_VECRAM_SIZE       = 0x1000;
    inline constexpr uint16_t MH_VECROM_BASE       = 0x3000;
    inline constexpr uint16_t MH_VECROM_SIZE       = 0x1000;
    inline constexpr uint16_t MH_PROGROM_BASE      = 0x4000;
    inline constexpr uint16_t MH_PROGROM_SIZE      = 0x4000;
    inline constexpr uint16_t MH_POKEY1_BASE       = 0x1200;
    inline constexpr uint16_t MH_VGGO_ADDR         = 0x1400;    // write: trigger AVG
    inline constexpr uint16_t MH_VGRST_ADDR        = 0x1600;    // write: reset AVG
    inline constexpr uint16_t MH_WDCLR_ADDR        = 0x1800;    // write: watchdog clear

}  // namespace atari_vector_constants
