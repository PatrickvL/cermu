#pragma once
/*
 * atari_vector_system.hpp — Atari vector arcade systems (1979–1983)
 *
 * Unified system class for all Atari 6502-based vector arcade games.
 * Both DVG-based (Asteroids, Lunar Lander, Asteroids Deluxe) and
 * AVG-based (Battlezone, Red Baron, Tempest, Gravitar, Space Duel, Black Widow, Major Havoc)
 * share the same architecture:
 *
 *   - MOS 6502 CPU @ 1.512 MHz (12.096 MHz master / 8)
 *   - DVG or AVG vector display processor
 *   - Optional POKEY sound chip(s)
 *   - Work RAM, vector RAM, vector ROM, program ROM
 *   - I/O region with game-specific decode
 *   - NMI driven by 250 Hz timer
 *
 * Template parameter V (AtariVectorVariant) selects the game.
 * AtariVectorTraits<V> provides all compile-time differences:
 *   - Memory map (ROM/RAM base addresses, sizes)
 *   - Video chip type (dvg_t or avg_t)
 *   - I/O read/write decode addresses
 *   - System descriptor metadata (name, aliases, ROM sizes)
 *   - POKEY presence and base address
 *
 * The system template uses CoreChips<MOS6502, VideoChip> where
 * VideoChip is either dvg_t or avg_t, selected by traits.
 */

#include "systems/arcade/atari_vector/atari_vector_constants.hpp"
#include "core/system.hpp"
#include "core/board.hpp"
#include "core/dip_switch.hpp"
#include "core/core_chips.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/dvg/dvg.hpp"
#include "chip/video/avg/avg.hpp"
#include "chip/sound/pokey/c012294.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/logic/ls259.hpp"
#include "chip/memory/er2055.hpp"
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace atv = atari_vector_constants;

// ============================================================================
// AVG color palette — maps AVG STAT[2:0] color index to RGB.
// Kept here (not in vector_shader.hpp) to avoid GUI dependency.
// ============================================================================

namespace atari_vector_palette {
    inline constexpr float COLOR_PALETTE[8 * 3] = {
        1.0f, 1.0f, 1.0f,   // 0: white
        0.15f, 0.15f, 1.0f, // 1: blue
        0.15f, 1.0f, 0.15f, // 2: green
        0.15f, 1.0f, 1.0f,  // 3: cyan
        1.0f, 0.15f, 0.15f, // 4: red
        1.0f, 0.15f, 1.0f,  // 5: magenta
        1.0f, 1.0f, 0.15f,  // 6: yellow
        1.0f, 1.0f, 1.0f,   // 7: white
    };
}

// ============================================================================
// Variant traits — compile-time differences between game variants.
// Drives ChipManifest, BusSpec, Board, SystemDescriptor, and ROM loading.
// ============================================================================

template<AtariVectorVariant V>
struct AtariVectorTraits;

// ── DVG-based games ──────────────────────────────────────────────────────────

template<>
struct AtariVectorTraits<AtariVectorVariant::ASTEROIDS> {
    using VideoChip = dvg_t;
    static constexpr const char* NAME         = "Asteroids";
    static constexpr const char* SHORT_NAME   = "Asteroids";
    static constexpr const char* DESCRIPTION  = "Atari Asteroids (1979) — 6502 CPU, DVG vector display";
    static constexpr const char* DATA_FOLDER  = "asteroids";
    static constexpr int         YEAR         = 1979;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0400;   // 1 KB
    static constexpr uint16_t PROGROM_BASE       = 0x6000;
    static constexpr uint16_t PROGROM_SIZE       = 0x2000;   // 8 KB (manifest-aligned)
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;        // 0 = same as PROGROM_SIZE
    static constexpr uint16_t PROGROM_ACTUAL     = 0x1800;   // 6 KB actual ROM
    static constexpr uint16_t PROGROM_OFFSET     = 0x0800;   // ROM data starts at byte $800
    static constexpr uint16_t VECRAM_BASE        = 0x4000;
    static constexpr uint16_t VECRAM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_BASE        = 0x5000;
    static constexpr uint16_t VECROM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;        // 0 = same as VECROM_SIZE
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x800;

    static constexpr bool HAS_POKEY       = false;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "DVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x3000;
    static constexpr uint16_t VGRST_ADDR       = 0x3200;
    static constexpr uint16_t WDCLR_ADDR       = 0x3400;
    static constexpr uint16_t POKEY1_BASE      = 0;
    static constexpr uint16_t POKEY2_BASE      = 0;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "Asteroids", "ASTEROIDS" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 6144, 8192 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::ASTEROIDS_DELUXE> {
    using VideoChip = dvg_t;
    static constexpr const char* NAME         = "Asteroids Deluxe";
    static constexpr const char* SHORT_NAME   = "AsteroidsDeluxe";
    static constexpr const char* DESCRIPTION  = "Atari Asteroids Deluxe (1980) — 6502 CPU, DVG, POKEY";
    static constexpr const char* DATA_FOLDER  = "asteroids_deluxe";
    static constexpr int         YEAR         = 1980;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0400;   // 1 KB
    static constexpr uint16_t PROGROM_BASE       = 0x6000;
    static constexpr uint16_t PROGROM_SIZE       = 0x2000;   // 8 KB
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x2000;
    static constexpr uint16_t PROGROM_OFFSET     = 0;
    static constexpr uint16_t VECRAM_BASE        = 0x4000;
    static constexpr uint16_t VECRAM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_BASE        = 0x4800;
    static constexpr uint16_t VECROM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = true;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "blue";
    static constexpr const char* VIDEO_CHIP_NAME = "DVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x3000;
    static constexpr uint16_t VGRST_ADDR       = 0x3200;
    static constexpr uint16_t WDCLR_ADDR       = 0x3400;
    static constexpr uint16_t POKEY1_BASE      = 0x2600;
    static constexpr uint16_t POKEY1_SIZE      = 0x10;
    static constexpr uint16_t POKEY2_BASE      = 0;
    static constexpr uint16_t POKEY2_SIZE      = 0;
    static constexpr uint16_t EAROM_BASE       = 0x2C00;
    static constexpr uint16_t EAROM_SIZE       = 0x40;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;        // handled via DVG switch ($3800/$3A00)
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "AsteroidsDeluxe", "Asteroids Deluxe", "ASTEROIDSDELUXE" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 12288 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::LUNAR_LANDER> {
    using VideoChip = dvg_t;
    static constexpr const char* NAME         = "Lunar Lander";
    static constexpr const char* SHORT_NAME   = "LunarLander";
    static constexpr const char* DESCRIPTION  = "Atari Lunar Lander (1979) — 6502 CPU, DVG vector display";
    static constexpr const char* DATA_FOLDER  = "lunar_lander";
    static constexpr int         YEAR         = 1979;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0400;   // 1 KB
    static constexpr uint16_t PROGROM_BASE       = 0x6000;
    static constexpr uint16_t PROGROM_SIZE       = 0x2000;   // 8 KB
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x2000;
    static constexpr uint16_t PROGROM_OFFSET     = 0;
    static constexpr uint16_t VECRAM_BASE        = 0x4000;
    static constexpr uint16_t VECRAM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_BASE        = 0x4800;
    static constexpr uint16_t VECROM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;

    static constexpr bool HAS_POKEY       = false;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "white";
    static constexpr const char* VIDEO_CHIP_NAME = "DVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x3000;
    static constexpr uint16_t VGRST_ADDR       = 0x3200;
    static constexpr uint16_t WDCLR_ADDR       = 0x3400;
    static constexpr uint16_t POKEY1_BASE      = 0;
    static constexpr uint16_t POKEY2_BASE      = 0;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "LunarLander", "Lunar Lander", "LUNARLANDER" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 8192, 10240 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::BATTLEZONE> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Battlezone";
    static constexpr const char* SHORT_NAME   = "Battlezone";
    static constexpr const char* DESCRIPTION  = "Atari Battlezone (1980) — 6502 CPU, AVG vector display";
    static constexpr const char* DATA_FOLDER  = "battlezone";
    static constexpr int         YEAR         = 1980;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0400;   // 1 KB
    static constexpr uint16_t PROGROM_BASE       = 0x4000;
    static constexpr uint16_t PROGROM_SIZE       = 0x4000;   // 16 KB manifest-aligned
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x3000;   // 12 KB actual ($5000-$7FFF)
    static constexpr uint16_t PROGROM_OFFSET     = 0x1000;
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_BASE        = 0x3000;
    static constexpr uint16_t VECROM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x800;

    static constexpr bool HAS_POKEY       = false;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x1200;
    static constexpr uint16_t VGRST_ADDR       = 0x1600;
    static constexpr uint16_t WDCLR_ADDR       = 0x1400;
    static constexpr uint16_t POKEY1_BASE      = 0;
    static constexpr uint16_t POKEY2_BASE      = 0;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "Battlezone", "BATTLEZONE", "BZone" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 12288, 16384 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::RED_BARON> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Red Baron";
    static constexpr const char* SHORT_NAME   = "RedBaron";
    static constexpr const char* DESCRIPTION  = "Atari Red Baron (1980) — 6502 CPU, AVG, POKEY";
    static constexpr const char* DATA_FOLDER  = "red_baron";
    static constexpr int         YEAR         = 1980;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0400;   // 1 KB
    static constexpr uint16_t PROGROM_BASE       = 0x4000;
    static constexpr uint16_t PROGROM_SIZE       = 0x4000;   // 16 KB (manifest-aligned)
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x3000;   // 12 KB actual
    static constexpr uint16_t PROGROM_OFFSET     = 0x1000;
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_BASE        = 0x3000;
    static constexpr uint16_t VECROM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x800;

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x1200;
    static constexpr uint16_t VGRST_ADDR       = 0x1600;
    static constexpr uint16_t WDCLR_ADDR       = 0x1400;
    static constexpr uint16_t POKEY1_BASE      = 0x1810;
    static constexpr uint16_t POKEY1_SIZE      = 0x10;
    static constexpr uint16_t POKEY2_BASE      = 0;
    static constexpr uint16_t POKEY2_SIZE      = 0;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "RedBaron", "Red Baron", "REDBARON" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 12288, 16384 };
};

// ── AVG-based games ──────────────────────────────────────────────────────────

template<>
struct AtariVectorTraits<AtariVectorVariant::TEMPEST> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Tempest";
    static constexpr const char* SHORT_NAME   = "Tempest";
    static constexpr const char* DESCRIPTION  = "Atari Tempest (1980) — 6502 CPU, AVG color vector, POKEY";
    static constexpr const char* DATA_FOLDER  = "tempest";
    static constexpr int         YEAR         = 1980;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0800;   // 2 KB
    static constexpr uint16_t PROGROM_BASE       = 0x8000;
    static constexpr uint16_t PROGROM_SIZE       = 0x8000;   // 32 KB
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x5000;   // 20 KB actual ($9000-$DFFF)
    static constexpr uint16_t PROGROM_OFFSET     = 0x1000;   // ROM starts at chip offset $1000
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_BASE        = 0x3000;
    static constexpr uint16_t VECROM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x800;

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = true;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x4800;
    static constexpr uint16_t VGRST_ADDR       = 0x5800;
    static constexpr uint16_t WDCLR_ADDR       = 0x5000;
    static constexpr uint16_t POKEY1_BASE      = 0x60C0;
    static constexpr uint16_t POKEY1_SIZE      = 0x10;
    static constexpr uint16_t POKEY2_BASE      = 0x60D0;
    static constexpr uint16_t POKEY2_SIZE      = 0x10;
    static constexpr uint16_t EAROM_BASE       = 0x6000;
    static constexpr uint16_t EAROM_SIZE       = 0x40;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0x6040;
    static constexpr uint16_t EAROM_READ_ADDR  = 0x6050;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "Tempest", "TEMPEST" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 16384, 24576 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::GRAVITAR> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Gravitar";
    static constexpr const char* SHORT_NAME   = "Gravitar";
    static constexpr const char* DESCRIPTION  = "Atari Gravitar (1982) — 6502 CPU, AVG vector, POKEY";
    static constexpr const char* DATA_FOLDER  = "gravitar";
    static constexpr int         YEAR         = 1982;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0800;   // 2 KB
    static constexpr uint16_t PROGROM_BASE       = 0x9000;
    static constexpr uint16_t PROGROM_SIZE       = 0x7000;   // 28 KB ($9000-$FFFF)
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0x8000;   // pow2 slot for bank mapping
    static constexpr uint16_t PROGROM_ACTUAL     = 0x7000;
    static constexpr uint16_t PROGROM_OFFSET     = 0;
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_BASE        = 0x2800;
    static constexpr uint16_t VECROM_SIZE        = 0x3800;   // 14 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0x4000;   // pow2 slot for bank mapping
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;    // RAM=2KB=1024 words, ROM at word 0x400

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x8840;
    static constexpr uint16_t VGRST_ADDR       = 0x8880;
    static constexpr uint16_t WDCLR_ADDR       = 0x8980;
    static constexpr uint16_t POKEY1_BASE      = 0x6000;
    static constexpr uint16_t POKEY1_SIZE      = 0x20;
    static constexpr uint16_t POKEY2_BASE      = 0x6800;
    static constexpr uint16_t POKEY2_SIZE      = 0x20;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0x88C0;

    static constexpr const char* ALIASES[] = { "Gravitar", "GRAVITAR" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 16384, 24576 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::SPACE_DUEL> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Space Duel";
    static constexpr const char* SHORT_NAME   = "SpaceDuel";
    static constexpr const char* DESCRIPTION  = "Atari Space Duel (1982) — 6502 CPU, AVG color vector, POKEY";
    static constexpr const char* DATA_FOLDER  = "space_duel";
    static constexpr int         YEAR         = 1982;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0400;   // 1 KB
    static constexpr uint16_t PROGROM_BASE       = 0x4000;
    static constexpr uint16_t PROGROM_SIZE       = 0x4000;   // 16 KB
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x4000;
    static constexpr uint16_t PROGROM_OFFSET     = 0;
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_BASE        = 0x2800;
    static constexpr uint16_t VECROM_SIZE        = 0x1800;   // 6 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0x2000;   // pow2 slot for bank mapping
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;    // RAM=2KB=1024 words, ROM at word 0x400

    // Space Duel has a second program ROM region at $8000-$FFFF
    static constexpr bool     HAS_EXTRA_PROGROM  = true;
    static constexpr uint16_t PROGROM_HI_BASE    = 0x8000;
    static constexpr uint16_t PROGROM_HI_SIZE    = 0x8000;   // 32 KB

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x0C80;
    static constexpr uint16_t VGRST_ADDR       = 0x0D80;
    static constexpr uint16_t WDCLR_ADDR       = 0x0D00;
    static constexpr uint16_t POKEY1_BASE      = 0x1000;
    static constexpr uint16_t POKEY1_SIZE      = 0x0400;   // $1000-$13FF mirrored
    static constexpr uint16_t POKEY2_BASE      = 0x1400;
    static constexpr uint16_t POKEY2_SIZE      = 0x0400;   // $1400-$17FF mirrored
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0x0E00;

    static constexpr const char* ALIASES[] = { "SpaceDuel", "Space Duel", "SPACEDUEL" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 16384, 24576 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::BLACK_WIDOW> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Black Widow";
    static constexpr const char* SHORT_NAME   = "BlackWidow";
    static constexpr const char* DESCRIPTION  = "Atari Black Widow (1982) — 6502 CPU, AVG color vector, POKEY";
    static constexpr const char* DATA_FOLDER  = "black_widow";
    static constexpr int         YEAR         = 1982;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0800;   // 2 KB
    static constexpr uint16_t PROGROM_BASE       = 0x9000;
    static constexpr uint16_t PROGROM_SIZE       = 0x7000;   // 28 KB ($9000-$FFFF)
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0x8000;   // pow2 slot for bank mapping
    static constexpr uint16_t PROGROM_ACTUAL     = 0x7000;
    static constexpr uint16_t PROGROM_OFFSET     = 0;
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x0800;   // 2 KB
    static constexpr uint16_t VECROM_BASE        = 0x2800;
    static constexpr uint16_t VECROM_SIZE        = 0x3800;   // 14 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0x4000;   // pow2 slot for bank mapping
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;    // RAM=2KB=1024 words, ROM at word 0x400

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x8840;
    static constexpr uint16_t VGRST_ADDR       = 0x8880;
    static constexpr uint16_t WDCLR_ADDR       = 0x8980;
    static constexpr uint16_t POKEY1_BASE      = 0x6000;
    static constexpr uint16_t POKEY1_SIZE      = 0x20;
    static constexpr uint16_t POKEY2_BASE      = 0x6800;
    static constexpr uint16_t POKEY2_SIZE      = 0x20;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0x88C0;

    static constexpr const char* ALIASES[] = { "BlackWidow", "Black Widow", "BLACKWIDOW" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 16384, 24576 };
};

template<>
struct AtariVectorTraits<AtariVectorVariant::MAJOR_HAVOC> {
    using VideoChip = avg_t;
    static constexpr const char* NAME         = "Major Havoc";
    static constexpr const char* SHORT_NAME   = "MajorHavoc";
    static constexpr const char* DESCRIPTION  = "Atari Major Havoc (1983) — 6502 CPU, AVG color vector, POKEY";
    static constexpr const char* DATA_FOLDER  = "major_havoc";
    static constexpr int         YEAR         = 1983;

    static constexpr uint16_t RAM_BASE           = 0x0000;
    static constexpr uint16_t RAM_SIZE           = 0x0800;   // 2 KB
    static constexpr uint16_t PROGROM_BASE       = 0x4000;
    static constexpr uint16_t PROGROM_SIZE       = 0x4000;   // 16 KB
    static constexpr uint16_t PROGROM_SLOT_SIZE  = 0;
    static constexpr uint16_t PROGROM_ACTUAL     = 0x4000;
    static constexpr uint16_t PROGROM_OFFSET     = 0;
    static constexpr uint16_t VECRAM_BASE        = 0x2000;
    static constexpr uint16_t VECRAM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_BASE        = 0x3000;
    static constexpr uint16_t VECROM_SIZE        = 0x1000;   // 4 KB
    static constexpr uint16_t VECROM_SLOT_SIZE   = 0;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x800;

    static constexpr bool HAS_POKEY       = true;
    static constexpr bool HAS_EAROM       = false;
    static constexpr bool USES_15BIT_ADDR = true;  // TODO: MH needs 16-bit + bank switching, deferred
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    // I/O addresses (0 = not present on this board)
    static constexpr uint16_t VGGO_ADDR        = 0x1400;
    static constexpr uint16_t VGRST_ADDR       = 0x1600;
    static constexpr uint16_t WDCLR_ADDR       = 0x1800;
    static constexpr uint16_t POKEY1_BASE      = 0x1200;
    static constexpr uint16_t POKEY1_SIZE      = 0x10;
    static constexpr uint16_t POKEY2_BASE      = 0;
    static constexpr uint16_t POKEY2_SIZE      = 0;
    static constexpr uint16_t EAROM_BASE       = 0;
    static constexpr uint16_t EAROM_SIZE       = 0;
    static constexpr uint16_t EAROM_CTRL_ADDR  = 0;
    static constexpr uint16_t EAROM_READ_ADDR  = 0;
    static constexpr uint16_t IRQACK_ADDR      = 0;

    static constexpr const char* ALIASES[] = { "MajorHavoc", "Major Havoc", "MAJORHAVOC" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 16384, 32768 };
};


// ============================================================================
// Chip manifest — parameterized by variant
// ============================================================================

template<AtariVectorVariant V>
constexpr auto make_atv_manifest() {
    using T  = AtariVectorTraits<V>;
    using VG = typename T::VideoChip;

    // Non-pow2 ROMs need a larger slot (pow2) with effective_size set to actual.
    constexpr uint16_t vrom_slot = T::VECROM_SLOT_SIZE ? T::VECROM_SLOT_SIZE : T::VECROM_SIZE;
    constexpr size_t   vrom_eff  = T::VECROM_SLOT_SIZE ? T::VECROM_SIZE : 0;
    constexpr uint16_t prom_slot = T::PROGROM_SLOT_SIZE ? T::PROGROM_SLOT_SIZE : T::PROGROM_SIZE;
    constexpr size_t   prom_eff  = T::PROGROM_SLOT_SIZE ? T::PROGROM_SIZE : 0;

    // Core slots present in every variant
    auto core = std::make_tuple(
        Slot<RAMChip>{T::RAM_BASE,     T::RAM_SIZE,     0, "Work RAM"},
        Slot<RAMChip>{T::VECRAM_BASE,  T::VECRAM_SIZE,  0, "Vector RAM"},
        Slot<ROMChip>{T::VECROM_BASE,  vrom_slot,       0, "Vector ROM",  0, 0, 0, vrom_eff},
        Slot<ROMChip>{T::PROGROM_BASE, prom_slot,       0,
                      V == AtariVectorVariant::SPACE_DUEL ? "Program ROM Low" : "Program ROM",
                      0, 0, 0, prom_eff},
        Slot<MOS6502>{0, 0, 0, "MOS 6502"},
        Slot<VG>{0, 0, 0, T::VIDEO_CHIP_NAME},
        Slot<LS259>{0, 0, 0, "74LS259"}
    );

    // Variant-specific optional slots (extra ROM, POKEY, EAROM)
    auto extra = []() {
        if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            return std::make_tuple(
                Slot<ROMChip>{T::PROGROM_HI_BASE, T::PROGROM_HI_SIZE, 0, "Program ROM High"},
                Slot<pokey::C012294>{T::POKEY1_BASE, 0, 0x0F, "POKEY 1"}
            );
        } else if constexpr (T::HAS_POKEY && T::HAS_EAROM) {
            return std::make_tuple(
                Slot<pokey::C012294>{T::POKEY1_BASE, 0, 0x0F, "POKEY 1"},
                Slot<ER2055>{T::EAROM_BASE, 0, 0x3F, "ER2055 EAROM"}
            );
        } else if constexpr (T::HAS_POKEY) {
            return std::make_tuple(
                Slot<pokey::C012294>{T::POKEY1_BASE, 0, 0x0F, "POKEY 1"}
            );
        } else {
            return std::tuple<>();
        }
    }();

    return std::apply([](auto&&... slots) {
        return make_chip_manifest(std::forward<decltype(slots)>(slots)...);
    }, std::tuple_cat(core, extra));
}

template<AtariVectorVariant V>
inline constexpr auto kVectorChips = make_atv_manifest<V>();

// ============================================================================
// BusSpec — single template alias replaces 10 named aliases + selector
// ============================================================================

template<AtariVectorVariant V>
using VectorBusSpec = ManifestBusSpec<kVectorChips<V>, 16, 8>;


// ============================================================================
// AtariVectorSystem — unified system for all Atari 6502 vector games
// ============================================================================

template<AtariVectorVariant V>
class AtariVectorSystem : public System {
    using Traits    = AtariVectorTraits<V>;
    using VideoChip = typename Traits::VideoChip;
    using SoundChip = std::conditional_t<Traits::HAS_POKEY, pokey::C012294, NoChip>;
    using Spec      = VectorBusSpec<V>;
    using Bus       = MemoryBus<Spec>;
    using ChipSet   = CoreChips<MOS6502, VideoChip, SoundChip>;
    using MainBoard = Board<Spec, ChipSet>;

public:
    AtariVectorSystem();
    ~AtariVectorSystem() override;

    // ── System interface ─────────────────────────────────────────
    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;
    void get_display_dimensions(int* width, int* height) const override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    VideoSignalType get_video_signal_type() const override { return VideoSignalType::Vector; }
    VectorDisplayConfig get_vector_display_config() const override {
        // Color AVG games use white phosphor + color palette.
        // Monochrome games use tinted phosphor (green/blue/white) with no palette.
        if constexpr (Traits::PALETTE_ID[0] == 'c') {  // "color"
            return { 1.0f, 1.0f, 1.0f, atari_vector_palette::COLOR_PALETTE };
        } else if constexpr (Traits::PALETTE_ID[0] == 'b') {  // "blue"
            return { 0.3f, 0.3f, 1.0f, nullptr };
        } else if constexpr (Traits::PALETTE_ID[0] == 'w') {  // "white"
            return { 1.0f, 1.0f, 1.0f, nullptr };
        } else {  // "green" (default)
            return { 0.2f, 1.0f, 0.2f, nullptr };
        }
    }
    void set_speed_multiplier(float multiplier) override;

    // ROM set loading
    std::vector<const RomSetDescriptor*> get_rom_set_descriptors() const override;
    bool load_rom_set(const RomSetMatch& match) override;

    bool is_system_ready() const override { return system_ready_; }

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Chips ────────────────────────────────────────────────────────────

    // Memory chips (non-owning; owned by board_)
    RAMChip*    vec_ram_     = nullptr;
    ROMChip*    vec_rom_     = nullptr;
    ROMChip*    prog_rom_    = nullptr;
    ROMChip*    prog_rom_hi_ = nullptr;  // 16-bit games: upper ROM ($8000-$FFFF)

    // ── Memory bus ───────────────────────────────────────────────────────
    Bus bus_;
    MainBoard board_{kVectorChips<V>};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<VectorVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    std::unique_ptr<AudioPort> audio_port_;
    int audio_sample_rate_ = atv::DEFAULT_SAMPLE_RATE;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_ = MOS6502::default_bus_state();
    bool system_ready_    = false;
    uint32_t nmi_counter_ = 0;

    // ── Inputs ──────────────────────────────────────────────────────────
    uint8_t in0_       = 0x00;
    uint8_t in1_       = 0x00;
    uint8_t thrust_    = 0x00;      // Lunar Lander thrust ADC

    // ── DIP switches ────────────────────────────────────────────────────
    // Per-game DIP switch banks (descriptors set in initialize()).
    // dip_bank_[0] = primary (DSW0/DSW1/IN4/IN2/IN3 depending on game)
    // dip_bank_[1] = secondary (DSW1 for BZ/RB only)
    DipSwitchBank dip_bank_[2];

    // ── 74LS259 addressable latch (coin counters, LEDs, NMI enable) ────
    LS259 latch_259_;
    bool irq_asserted_ = false;     // Level-sensitive IRQ (Tempest, Gravitar, BW, SD, AD fallback)

    // ── Sound output latches ────────────────────────────────────────────
    uint8_t snd_latch_ = 0x00;

    // ── EAROM (ER2055 512-bit Electrically Alterable ROM) ────────────────
    // Used on: Asteroids Deluxe, Tempest, Gravitar, Black Widow, Space Duel,
    // Red Baron, Centipede, Millipede, Dig Dug, Liberator, and others.
    // Atari part number: 137161-001
    ER2055 earom_;

    // ── Internal helpers ────────────────────────────────────────────────
    void tick_cpu();
    bus_state_t io_read(uint16_t addr, bus_state_t pins);
    bus_state_t io_write(uint16_t addr, uint8_t data, bus_state_t pins);



    // Convenience accessors for the video chip
    VideoChip& vg() { return board_.video; }
    const VideoChip& vg() const { return board_.video; }
};

// ============================================================================
// Concrete type aliases for system registration
// ============================================================================

// DVG-based
using AsteroidsSystem       = AtariVectorSystem<AtariVectorVariant::ASTEROIDS>;
using AsteroidsDeluxeSystem = AtariVectorSystem<AtariVectorVariant::ASTEROIDS_DELUXE>;
using LunarLanderSystem     = AtariVectorSystem<AtariVectorVariant::LUNAR_LANDER>;

// AVG-based
using BattlezoneSystem      = AtariVectorSystem<AtariVectorVariant::BATTLEZONE>;
using RedBaronSystem        = AtariVectorSystem<AtariVectorVariant::RED_BARON>;
using TempestSystem         = AtariVectorSystem<AtariVectorVariant::TEMPEST>;
using GravitarSystem        = AtariVectorSystem<AtariVectorVariant::GRAVITAR>;
using SpaceDuelSystem       = AtariVectorSystem<AtariVectorVariant::SPACE_DUEL>;
using BlackWidowSystem      = AtariVectorSystem<AtariVectorVariant::BLACK_WIDOW>;
using MajorHavocSystem      = AtariVectorSystem<AtariVectorVariant::MAJOR_HAVOC>;
