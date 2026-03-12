#pragma once
/*
 * a2600_mapper_factory.h — Atari 2600 mapper auto-detection and creation
 *
 * Atari 2600 cartridges have no header — the banking scheme must be
 * detected from ROM size and content analysis. The factory uses:
 *
 *   1. ROM size as primary discriminator (unambiguous for most sizes)
 *   2. Hotspot/opcode signature analysis for ambiguous sizes (e.g. 8KB
 *      can be F8, E0, or FE)
 *
 * Size → default mapper:
 *   ≤2KB  → 2K       8KB  → F8 (check E0/FE)     32KB   → F4 (check 3F)
 *   ≤4KB  → 4K      12KB  → FA                    >32KB  → 3F
 *    -               16KB  → F6
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/mappers/a2600_mapper_2k.hpp"
#include "systems/atari2600/mappers/a2600_mapper_4k.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f8.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f6.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f4.hpp"
#include "systems/atari2600/mappers/a2600_mapper_e0.hpp"
#include "systems/atari2600/mappers/a2600_mapper_3f.hpp"
#include "systems/atari2600/mappers/a2600_mapper_fe.hpp"
#include "systems/atari2600/mappers/a2600_mapper_fa.hpp"
#include <memory>
#include <cstdio>

namespace a2600_mapper_factory {

// ========================================================================
// SIGNATURE DETECTION
// ========================================================================

// Detect E0 (Parker Brothers) signature in 8KB ROM.
// E0 carts use segment-switching hotspots at $1FE0–$1FF7. We look for
// absolute-addressing opcodes that reference these addresses.
inline bool detect_e0(const uint8_t* rom, uint32_t size) {
    if (size != 8192) return false;

    int hotspot_refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        // Absolute addressing opcodes (3-byte): LDA, STA, JMP, JSR, LDX, LDY, STX, STY
        if (opcode == 0xAD || opcode == 0x8D || opcode == 0x4C || opcode == 0x20 ||
            opcode == 0xAE || opcode == 0xAC || opcode == 0x8E || opcode == 0x8C) {
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if (addr >= 0x1FE0 && addr <= 0x1FF7)
                hotspot_refs++;
        }
    }
    // E0 carts typically have ≥4 references to segment-switching hotspots
    return hotspot_refs >= 4;
}

// Detect FE (Activision) signature in 8KB ROM.
// FE carts rely on JSR/RTS to addresses ending in $FE for bank switching.
// Activision used this in Decathlon and Robot Tank.
inline bool detect_fe(const uint8_t* rom, uint32_t size) {
    if (size != 8192) return false;

    int fe_refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        if (rom[i] == 0x20) {  // JSR abs
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if ((addr & 0x00FF) == 0xFE) fe_refs++;
        }
    }
    // Weak heuristic — FE is rare and hard to auto-detect
    return fe_refs >= 6;
}

// Detect 3F (Tigervision) signature for ROMs >8KB.
// 3F carts write to $003F (or mirrored $xx3F) for bank selection.
inline bool detect_3f(const uint8_t* rom, uint32_t size) {
    if (size <= 4096) return false;

    int refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        if (opcode == 0x85 && rom[i + 1] == 0x3F) {
            // STA zeropage $3F
            refs++;
        } else if (opcode == 0x8D) {
            // STA absolute — check if target has low 6 bits == 0x3F
            // and is NOT in cartridge space (A12=0)
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if ((addr & 0x003F) == 0x003F && !(addr & 0x1000))
                refs++;
        }
    }
    return refs >= 2;
}

// ========================================================================
// PUBLIC API
// ========================================================================

// Create a mapper for the given ROM data and size.
// Auto-detects the banking scheme based on ROM size and content analysis.
inline std::unique_ptr<A2600Mapper> create(const uint8_t* rom, uint32_t size) {
    std::unique_ptr<A2600Mapper> mapper;

    if (size <= 2048) {
        mapper = std::make_unique<A2600Mapper2K>();
        printf("Atari2600: Mapper = 2K (no bank switching)\n");
    }
    else if (size <= 4096) {
        mapper = std::make_unique<A2600Mapper4K>();
        printf("Atari2600: Mapper = 4K (no bank switching)\n");
    }
    else if (size == 8192) {
        // 8KB: most likely F8, but could be E0 (Parker Bros) or FE (Activision)
        if (detect_e0(rom, size)) {
            mapper = std::make_unique<A2600MapperE0>();
            printf("Atari2600: Mapper = E0 (Parker Brothers)\n");
        } else if (detect_fe(rom, size)) {
            mapper = std::make_unique<A2600MapperFE>();
            printf("Atari2600: Mapper = FE (Activision)\n");
        } else {
            mapper = std::make_unique<A2600MapperF8>();
            printf("Atari2600: Mapper = F8 (standard 8KB)\n");
        }
    }
    else if (size == 12288) {
        mapper = std::make_unique<A2600MapperFA>();
        printf("Atari2600: Mapper = FA (CBS RAM Plus 12KB)\n");
    }
    else if (size == 16384) {
        mapper = std::make_unique<A2600MapperF6>();
        printf("Atari2600: Mapper = F6 (standard 16KB)\n");
    }
    else if (size == 32768) {
        if (detect_3f(rom, size)) {
            mapper = std::make_unique<A2600Mapper3F>();
            printf("Atari2600: Mapper = 3F (Tigervision 32KB)\n");
        } else {
            mapper = std::make_unique<A2600MapperF4>();
            printf("Atari2600: Mapper = F4 (standard 32KB)\n");
        }
    }
    else if (size > 32768) {
        // Large ROMs: likely 3F (Tigervision) — supports up to 512KB
        mapper = std::make_unique<A2600Mapper3F>();
        printf("Atari2600: Mapper = 3F (Tigervision %uKB)\n", size / 1024);
    }
    else {
        // Unusual size — fall back to 4K with mirroring
        mapper = std::make_unique<A2600Mapper4K>();
        printf("Atari2600: Mapper = 4K (fallback for %u bytes)\n", size);
    }

    mapper->set_rom(rom, size);
    mapper->reset();
    return mapper;
}

} // namespace a2600_mapper_factory
