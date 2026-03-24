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
#include "core/core_chips.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/dvg/dvg.hpp"
#include "chip/video/avg/avg.hpp"
#include "chip/sound/pokey/c012294.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include <cstdint>
#include <memory>
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

    static constexpr uint16_t RAM_BASE        = atv::RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::AST_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::AST_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::AST_PROGROM_ACTUAL;
    static constexpr uint16_t PROGROM_OFFSET  = atv::AST_PROGROM_OFFSET;
    static constexpr uint16_t VECRAM_BASE     = atv::VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::AST_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x800;

    static constexpr bool HAS_POKEY     = false;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "DVG";

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

    static constexpr uint16_t RAM_BASE        = atv::RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::AD_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::AD_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::AD_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_OFFSET  = 0;
    static constexpr uint16_t VECRAM_BASE     = atv::VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::AD_VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::AD_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = true;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "blue";
    static constexpr const char* VIDEO_CHIP_NAME = "DVG";

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

    static constexpr uint16_t RAM_BASE        = atv::RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::LL_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::LL_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::LL_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_OFFSET  = 0;
    static constexpr uint16_t VECRAM_BASE     = atv::VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::LL_VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::LL_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;

    static constexpr bool HAS_POKEY     = false;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "white";
    static constexpr const char* VIDEO_CHIP_NAME = "DVG";

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

    static constexpr uint16_t RAM_BASE        = atv::BZ_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::BZ_RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::BZ_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::BZ_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::BZ_PROGROM_ACTUAL;
    static constexpr uint16_t PROGROM_OFFSET  = atv::BZ_PROGROM_OFFSET;
    static constexpr uint16_t VECRAM_BASE     = atv::BZ_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::BZ_VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::BZ_VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::BZ_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = atv::BZ_VECROM_WORD_OFFSET;

    static constexpr bool HAS_POKEY     = false;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

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

    static constexpr uint16_t RAM_BASE        = atv::RB_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::RB_RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::RB_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::RB_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::RB_PROGROM_ACTUAL;
    static constexpr uint16_t PROGROM_OFFSET  = atv::RB_PROGROM_OFFSET;
    static constexpr uint16_t VECRAM_BASE     = atv::RB_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::RB_VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::RB_VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::RB_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = atv::RB_VECROM_WORD_OFFSET;

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = true;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

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

    static constexpr uint16_t RAM_BASE        = atv::TEMP_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::TEMP_RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::TEMP_PROGROM_BASE;  // $8000
    static constexpr uint16_t PROGROM_SIZE    = atv::TEMP_PROGROM_SIZE;  // 32KB
    static constexpr uint16_t PROGROM_ACTUAL  = 0x5000;                  // 20KB actual ($9000-$DFFF)
    static constexpr uint16_t PROGROM_OFFSET  = 0x1000;                  // ROM starts at chip offset $1000
    static constexpr uint16_t VECRAM_BASE     = atv::TEMP_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::TEMP_VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::TEMP_VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::TEMP_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = atv::TEMP_VECROM_WORD_OFFSET;

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = true;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

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

    static constexpr uint16_t RAM_BASE        = atv::GRAV_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::GRAV_RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::GRAV_PROGROM_BASE;  // $9000
    static constexpr uint16_t PROGROM_SIZE    = atv::GRAV_PROGROM_SIZE;  // 28KB
    static constexpr uint16_t PROGROM_ACTUAL  = atv::GRAV_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_OFFSET  = 0;
    static constexpr uint16_t VECRAM_BASE     = atv::GRAV_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::GRAV_VECRAM_SIZE;  // 2KB
    static constexpr uint16_t VECROM_BASE        = atv::GRAV_VECROM_BASE;  // $2800
    static constexpr uint16_t VECROM_SIZE        = atv::GRAV_VECROM_SIZE;  // 14KB
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;  // RAM=2KB=1024 words, ROM at word 0x400

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "green";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

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

    static constexpr uint16_t RAM_BASE        = atv::SD_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::SD_RAM_SIZE;  // 1KB
    static constexpr uint16_t PROGROM_BASE    = atv::SD_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::SD_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::SD_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_OFFSET  = 0;
    static constexpr uint16_t VECRAM_BASE     = atv::SD_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::SD_VECRAM_SIZE;  // 2KB
    static constexpr uint16_t VECROM_BASE        = atv::SD_VECROM_BASE;  // $2800
    static constexpr uint16_t VECROM_SIZE        = atv::SD_VECROM_SIZE;  // 6KB
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;  // RAM=2KB=1024 words, ROM at word 0x400

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

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

    static constexpr uint16_t RAM_BASE        = atv::BW_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::BW_RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::BW_PROGROM_BASE;  // $9000
    static constexpr uint16_t PROGROM_SIZE    = atv::BW_PROGROM_SIZE;  // 28KB
    static constexpr uint16_t PROGROM_ACTUAL  = atv::BW_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_OFFSET  = 0;
    static constexpr uint16_t VECRAM_BASE     = atv::BW_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::BW_VECRAM_SIZE;  // 2KB
    static constexpr uint16_t VECROM_BASE        = atv::BW_VECROM_BASE;  // $2800
    static constexpr uint16_t VECROM_SIZE        = atv::BW_VECROM_SIZE;  // 14KB
    static constexpr uint16_t VECROM_WORD_OFFSET = 0x400;  // RAM=2KB=1024 words, ROM at word 0x400

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = false;
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

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

    static constexpr uint16_t RAM_BASE        = atv::MH_RAM_BASE;
    static constexpr uint16_t RAM_SIZE        = atv::MH_RAM_SIZE;
    static constexpr uint16_t PROGROM_BASE    = atv::MH_PROGROM_BASE;
    static constexpr uint16_t PROGROM_SIZE    = atv::MH_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_ACTUAL  = atv::MH_PROGROM_SIZE;
    static constexpr uint16_t PROGROM_OFFSET  = 0;
    static constexpr uint16_t VECRAM_BASE     = atv::MH_VECRAM_BASE;
    static constexpr uint16_t VECRAM_SIZE     = atv::MH_VECRAM_SIZE;
    static constexpr uint16_t VECROM_BASE        = atv::MH_VECROM_BASE;
    static constexpr uint16_t VECROM_SIZE        = atv::MH_VECROM_SIZE;
    static constexpr uint16_t VECROM_WORD_OFFSET = atv::TEMP_VECROM_WORD_OFFSET;

    static constexpr bool HAS_POKEY     = true;
    static constexpr bool HAS_EAROM     = false;
    static constexpr bool USES_15BIT_ADDR = true;  // TODO: MH needs 16-bit + bank switching, deferred
    static constexpr const char* PALETTE_ID = "color";
    static constexpr const char* VIDEO_CHIP_NAME = "AVG";

    static constexpr const char* ALIASES[] = { "MajorHavoc", "Major Havoc", "MAJORHAVOC" };
    static constexpr size_t PROBE_ROM_SIZES[] = { 16384, 32768 };
};


// ============================================================================
// Chip manifests — parameterized by variant
// ============================================================================

// ── DVG-based manifests (existing) ───────────────────────────────────────────

inline constexpr auto kAsteroidsChips = make_chip_manifest(
    Slot<RAMChip>{atv::RAM_BASE,     atv::RAM_SIZE,     0, "Work RAM"},
    Slot<RAMChip>{atv::VECRAM_BASE,  atv::VECRAM_SIZE,  0, "Vector RAM"},
    Slot<ROMChip>{atv::VECROM_BASE,  atv::AST_VECROM_SIZE, 0, "Vector ROM"},
    Slot<ROMChip>{atv::AST_PROGROM_BASE, atv::AST_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<dvg_t>{0, 0, 0, "DVG"}
);

inline constexpr auto kLunarLanderChips = make_chip_manifest(
    Slot<RAMChip>{atv::RAM_BASE,     atv::RAM_SIZE,     0, "Work RAM"},
    Slot<RAMChip>{atv::VECRAM_BASE,  atv::VECRAM_SIZE,  0, "Vector RAM"},
    Slot<ROMChip>{atv::LL_VECROM_BASE, atv::LL_VECROM_SIZE, 0, "Vector ROM"},
    Slot<ROMChip>{atv::LL_PROGROM_BASE, atv::LL_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<dvg_t>{0, 0, 0, "DVG"}
);

inline constexpr auto kAsteroidsDeluxeChips = make_chip_manifest(
    Slot<RAMChip>{atv::RAM_BASE,       atv::RAM_SIZE,       0, "Work RAM"},
    Slot<RAMChip>{atv::VECRAM_BASE,    atv::VECRAM_SIZE,    0, "Vector RAM"},
    Slot<ROMChip>{atv::AD_VECROM_BASE, atv::AD_VECROM_SIZE, 0, "Vector ROM"},
    Slot<ROMChip>{atv::AD_PROGROM_BASE, atv::AD_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<dvg_t>{0, 0, 0, "DVG"}
);

inline constexpr auto kBattlezoneChips = make_chip_manifest(
    Slot<RAMChip>{atv::BZ_RAM_BASE,     atv::BZ_RAM_SIZE,     0, "Work RAM"},
    Slot<RAMChip>{atv::BZ_VECRAM_BASE,  atv::BZ_VECRAM_SIZE,  0, "Vector RAM"},
    Slot<ROMChip>{atv::BZ_VECROM_BASE,  atv::BZ_VECROM_SIZE,  0, "Vector ROM"},
    Slot<ROMChip>{atv::BZ_PROGROM_BASE, atv::BZ_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

inline constexpr auto kRedBaronChips = make_chip_manifest(
    Slot<RAMChip>{atv::RB_RAM_BASE,     atv::RB_RAM_SIZE,     0, "Work RAM"},
    Slot<RAMChip>{atv::RB_VECRAM_BASE,  atv::RB_VECRAM_SIZE,  0, "Vector RAM"},
    Slot<ROMChip>{atv::RB_VECROM_BASE,  atv::RB_VECROM_SIZE,  0, "Vector ROM"},
    Slot<ROMChip>{atv::RB_PROGROM_BASE, atv::RB_PROGROM_SIZE, 0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

// ── AVG-based manifests ──────────────────────────────────────────────────────

inline constexpr auto kTempestChips = make_chip_manifest(
    Slot<RAMChip>{atv::TEMP_RAM_BASE,      atv::TEMP_RAM_SIZE,      0, "Work RAM"},
    Slot<RAMChip>{atv::TEMP_VECRAM_BASE,   atv::TEMP_VECRAM_SIZE,   0, "Vector RAM"},
    Slot<ROMChip>{atv::TEMP_VECROM_BASE,   atv::TEMP_VECROM_SIZE,   0, "Vector ROM"},
    Slot<ROMChip>{atv::TEMP_PROGROM_BASE,  atv::TEMP_PROGROM_SIZE,  0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

inline constexpr auto kGravitarChips = make_chip_manifest(
    Slot<RAMChip>{atv::GRAV_RAM_BASE,      atv::GRAV_RAM_SIZE,      0, "Work RAM"},
    Slot<RAMChip>{atv::GRAV_VECRAM_BASE,   atv::GRAV_VECRAM_SIZE,   0, "Vector RAM"},
    Slot<ROMChip>{atv::GRAV_VECROM_BASE,   0x4000,   0, "Vector ROM", 0, 0, 0, atv::GRAV_VECROM_SIZE},
    Slot<ROMChip>{atv::GRAV_PROGROM_BASE,  0x8000,   0, "Program ROM", 0, 0, 0, atv::GRAV_PROGROM_SIZE},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

inline constexpr auto kSpaceDuelChips = make_chip_manifest(
    Slot<RAMChip>{atv::SD_RAM_BASE,      atv::SD_RAM_SIZE,      0, "Work RAM"},
    Slot<RAMChip>{atv::SD_VECRAM_BASE,   atv::SD_VECRAM_SIZE,   0, "Vector RAM"},
    Slot<ROMChip>{atv::SD_VECROM_BASE,   0x2000,   0, "Vector ROM", 0, 0, 0, atv::SD_VECROM_SIZE},
    Slot<ROMChip>{atv::SD_PROGROM_BASE,  atv::SD_PROGROM_SIZE,  0, "Program ROM Low"},
    Slot<ROMChip>{atv::SD_PROGROM_HI_BASE, atv::SD_PROGROM_HI_SIZE, 0, "Program ROM High"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

inline constexpr auto kBlackWidowChips = make_chip_manifest(
    Slot<RAMChip>{atv::BW_RAM_BASE,      atv::BW_RAM_SIZE,      0, "Work RAM"},
    Slot<RAMChip>{atv::BW_VECRAM_BASE,   atv::BW_VECRAM_SIZE,   0, "Vector RAM"},
    Slot<ROMChip>{atv::BW_VECROM_BASE,   0x4000,   0, "Vector ROM", 0, 0, 0, atv::BW_VECROM_SIZE},
    Slot<ROMChip>{atv::BW_PROGROM_BASE,  0x8000,   0, "Program ROM", 0, 0, 0, atv::BW_PROGROM_SIZE},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

inline constexpr auto kMajorHavocChips = make_chip_manifest(
    Slot<RAMChip>{atv::MH_RAM_BASE,      atv::MH_RAM_SIZE,      0, "Work RAM"},
    Slot<RAMChip>{atv::MH_VECRAM_BASE,   atv::MH_VECRAM_SIZE,   0, "Vector RAM"},
    Slot<ROMChip>{atv::MH_VECROM_BASE,   atv::MH_VECROM_SIZE,   0, "Vector ROM"},
    Slot<ROMChip>{atv::MH_PROGROM_BASE,  atv::MH_PROGROM_SIZE,  0, "Program ROM"},
    Slot<MOS6502>{0, 0, 0, "MOS 6502"},
    Slot<avg_t>{0, 0, 0, "AVG"}
);

// ============================================================================
// Manifest selector
// ============================================================================

template<AtariVectorVariant V>
constexpr const auto& select_manifest() {
    if constexpr (V == AtariVectorVariant::ASTEROIDS)         return kAsteroidsChips;
    else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) return kAsteroidsDeluxeChips;
    else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) return kLunarLanderChips;
    else if constexpr (V == AtariVectorVariant::BATTLEZONE)   return kBattlezoneChips;
    else if constexpr (V == AtariVectorVariant::RED_BARON)    return kRedBaronChips;
    else if constexpr (V == AtariVectorVariant::TEMPEST)      return kTempestChips;
    else if constexpr (V == AtariVectorVariant::GRAVITAR)     return kGravitarChips;
    else if constexpr (V == AtariVectorVariant::SPACE_DUEL)   return kSpaceDuelChips;
    else if constexpr (V == AtariVectorVariant::BLACK_WIDOW)  return kBlackWidowChips;
    else                                                       return kMajorHavocChips;
}

// ============================================================================
// BusSpec for each variant
// ============================================================================

using AsteroidsBusSpec       = ManifestBusSpec<kAsteroidsChips, 16, 8>;
using AsteroidsDeluxeBusSpec = ManifestBusSpec<kAsteroidsDeluxeChips, 16, 8>;
using LunarLanderBusSpec     = ManifestBusSpec<kLunarLanderChips, 16, 8>;
using BattlezoneBusSpec      = ManifestBusSpec<kBattlezoneChips, 16, 8>;
using RedBaronBusSpec        = ManifestBusSpec<kRedBaronChips, 16, 8>;
using TempestBusSpec         = ManifestBusSpec<kTempestChips, 16, 8>;
using GravitarBusSpec        = ManifestBusSpec<kGravitarChips, 16, 8>;
using SpaceDuelBusSpec       = ManifestBusSpec<kSpaceDuelChips, 16, 8>;
using BlackWidowBusSpec      = ManifestBusSpec<kBlackWidowChips, 16, 8>;
using MajorHavocBusSpec      = ManifestBusSpec<kMajorHavocChips, 16, 8>;

template<AtariVectorVariant V>
struct VectorBusSpecSelect;

template<> struct VectorBusSpecSelect<AtariVectorVariant::ASTEROIDS>       { using type = AsteroidsBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::ASTEROIDS_DELUXE>{ using type = AsteroidsDeluxeBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::LUNAR_LANDER>    { using type = LunarLanderBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::BATTLEZONE>      { using type = BattlezoneBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::RED_BARON>       { using type = RedBaronBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::TEMPEST>         { using type = TempestBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::GRAVITAR>        { using type = GravitarBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::SPACE_DUEL>      { using type = SpaceDuelBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::BLACK_WIDOW>     { using type = BlackWidowBusSpec; };
template<> struct VectorBusSpecSelect<AtariVectorVariant::MAJOR_HAVOC>     { using type = MajorHavocBusSpec; };

template<AtariVectorVariant V>
using VectorBusSpec = typename VectorBusSpecSelect<V>::type;


// ============================================================================
// AtariVectorSystem — unified system for all Atari 6502 vector games
// ============================================================================

template<AtariVectorVariant V>
class AtariVectorSystem : public System {
    using Traits    = AtariVectorTraits<V>;
    using VideoChip = typename Traits::VideoChip;
    using Spec      = VectorBusSpec<V>;
    using Bus       = MemoryBus<Spec>;
    using ChipSet   = CoreChips<MOS6502, VideoChip>;
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

    pokey::C012294 pokey_;           // POKEY sound chip (games with HAS_POKEY)

    // Memory chips (non-owning; owned by board_)
    RAMChip*    vec_ram_     = nullptr;
    ROMChip*    vec_rom_     = nullptr;
    ROMChip*    prog_rom_    = nullptr;
    ROMChip*    prog_rom_hi_ = nullptr;  // 16-bit games: upper ROM ($8000-$FFFF)

    // ── Memory bus ───────────────────────────────────────────────────────
    Bus bus_;
    MainBoard board_{select_manifest<V>()};

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
    uint8_t dsw1_      = 0x84;
    uint8_t dsw2_      = 0x00;
    uint8_t thrust_    = 0x00;      // Lunar Lander thrust ADC

    // ── NMI gating ──────────────────────────────────────────────────────
    bool nmi_enabled_ = false;

    // ── Sound output latches ────────────────────────────────────────────
    uint8_t snd_latch_ = 0x00;

    // ── EAROM (some games — ER2055 64×4-bit) ────────────────────────────
    uint8_t earom_[64] = {};
    uint8_t earom_ctrl_ = 0x00;

    // ── Internal helpers ────────────────────────────────────────────────
    void tick_cpu();
    bus_state_t io_read(uint16_t addr, bus_state_t pins);
    bus_state_t io_write(uint16_t addr, uint8_t data, bus_state_t pins);



    // Convenience accessors for the video chip
    VideoChip& vg() { return board_.video(); }
    const VideoChip& vg() const { return board_.video(); }
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
