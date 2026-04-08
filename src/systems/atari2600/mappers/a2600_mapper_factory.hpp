#pragma once
/*
 * a2600_mapper_factory.h — Atari 2600 mapper auto-detection and creation
 *
 * Atari 2600 cartridges have no header — the banking scheme must be
 * detected from ROM size and content analysis. The factory uses:
 *
 *   1. ROM size as primary discriminator (unambiguous for most sizes)
 *   2. Hotspot/opcode signature analysis for ambiguous sizes
 *
 * Size → default mapper:
 *   ≤2KB  → 2K (or CV)   8KB   → F8/F8SC (check E0/FE/UA)   64KB  → EF/EFSC/F0
 *   ≤4KB  → 4K           12KB  → FA                          >64KB → 3F/3E
 *    -                    16KB  → F6/F6SC (check E7)
 *    -                    32KB  → F4/F4SC (check 3F/3E)
 */

#include "core/cermu.hpp"
#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/mappers/a2600_mapper_2k.hpp"
#include "systems/atari2600/mappers/a2600_mapper_4k.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f8.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f8sc.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f6.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f6sc.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f4.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f4sc.hpp"
#include "systems/atari2600/mappers/a2600_mapper_e0.hpp"
#include "systems/atari2600/mappers/a2600_mapper_e7.hpp"
#include "systems/atari2600/mappers/a2600_mapper_ef.hpp"
#include "systems/atari2600/mappers/a2600_mapper_efsc.hpp"
#include "systems/atari2600/mappers/a2600_mapper_f0.hpp"
#include "systems/atari2600/mappers/a2600_mapper_3f.hpp"
#include "systems/atari2600/mappers/a2600_mapper_3e.hpp"
#include "systems/atari2600/mappers/a2600_mapper_fe.hpp"
#include "systems/atari2600/mappers/a2600_mapper_fa.hpp"
#include "systems/atari2600/mappers/a2600_mapper_ua.hpp"
#include "systems/atari2600/mappers/a2600_mapper_cv.hpp"
#include "systems/atari2600/mappers/a2600_mapper_ar.hpp"
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

// Detect UA (UA Limited) signature in 8KB ROM.
// UA carts switch banks by accessing $0220/$0240 (RIOT space).
// We look for code referencing these addresses (or their mirrors).
inline bool detect_ua(const uint8_t* rom, uint32_t size) {
    if (size != 8192) return false;

    int refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        // Absolute addressing opcodes (LDA, STA, LDX, LDY, STX, STY)
        if (opcode == 0xAD || opcode == 0x8D || opcode == 0xAE || opcode == 0xAC ||
            opcode == 0x8E || opcode == 0x8C) {
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if ((addr & 0x1260) == 0x0220 || (addr & 0x1260) == 0x0240)
                refs++;
        }
    }
    return refs >= 2;
}

// Detect Superchip (128 bytes extra RAM) in ROM.
// SC carts have RAM at cart offsets $000-$07F (write) and $080-$0FF (read).
// We look for code that accesses these split-port areas in cart space.
inline bool detect_superchip(const uint8_t* rom, uint32_t size) {
    int read_refs = 0, write_refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
        if (!(addr & 0x1000)) continue;  // Must be cart space (A12=1)
        uint16_t off = addr & 0x0FFF;

        // Load from read port ($x080-$x0FF)
        if ((opcode == 0xAD || opcode == 0xAE || opcode == 0xAC) &&
            off >= 0x0080 && off <= 0x00FF)
            read_refs++;
        // Store to write port ($x000-$x07F)
        if ((opcode == 0x8D || opcode == 0x8E || opcode == 0x8C) &&
            off <= 0x007F)
            write_refs++;
    }
    // Need both read and write references to confirm SC
    return read_refs >= 2 && write_refs >= 2;
}

// Detect 3F (Tigervision) signature.
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

// Detect 3E (Tigervision + RAM) signature.
// Like 3F but also writes to $003E for RAM bank selection.
inline bool detect_3e(const uint8_t* rom, uint32_t size) {
    if (size <= 4096) return false;

    int refs_3f = 0, refs_3e = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        if (opcode == 0x85) {
            if (rom[i + 1] == 0x3F) refs_3f++;
            if (rom[i + 1] == 0x3E) refs_3e++;
        } else if (opcode == 0x8D) {
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if (!(addr & 0x1000)) {
                if ((addr & 0x003F) == 0x003F) refs_3f++;
                if ((addr & 0x003F) == 0x003E) refs_3e++;
            }
        }
    }
    // Must have both $3F and $3E references
    return refs_3f >= 1 && refs_3e >= 1;
}

// Detect E7 (M-Network) signature in 16KB ROM.
// E7 carts use hotspots $1FE0-$1FEB — the range overlaps E0/EF but
// the distinguishing feature is $1FE7 (RAM mode select) and $1FE8-$1FEB
// (RAM bank select).
inline bool detect_e7(const uint8_t* rom, uint32_t size) {
    if (size != 16384) return false;

    int ram_mode_refs = 0, ram_bank_refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        if (opcode == 0xAD || opcode == 0x8D || opcode == 0x4C || opcode == 0x20 ||
            opcode == 0xAE || opcode == 0xAC || opcode == 0x8E || opcode == 0x8C) {
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if (addr == 0x1FE7) ram_mode_refs++;
            if (addr >= 0x1FE8 && addr <= 0x1FEB) ram_bank_refs++;
        }
    }
    // E7 is distinctive: uses $1FE7 for RAM mode and $1FE8-$1FEB for RAM banks
    return ram_mode_refs >= 1 || ram_bank_refs >= 2;
}

// Detect EF signature in 64KB ROM.
// EF carts use hotspots $1FE0-$1FEF (16 banks).
inline bool detect_ef(const uint8_t* rom, uint32_t size) {
    if (size != 65536) return false;

    int hotspot_refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        if (opcode == 0xAD || opcode == 0x8D || opcode == 0x4C || opcode == 0x20 ||
            opcode == 0xAE || opcode == 0xAC || opcode == 0x8E || opcode == 0x8C) {
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if (addr >= 0x1FE0 && addr <= 0x1FEF)
                hotspot_refs++;
        }
    }
    return hotspot_refs >= 2;
}

// Detect F0 (Megaboy) signature in 64KB ROM.
// F0 uses a single hotspot at $1FF0 that auto-increments the bank counter.
inline bool detect_f0(const uint8_t* rom, uint32_t size) {
    if (size != 65536) return false;

    int refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        if (opcode == 0xAD || opcode == 0x8D || opcode == 0x4C || opcode == 0x20 ||
            opcode == 0xAE || opcode == 0xAC || opcode == 0x8E || opcode == 0x8C) {
            uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);
            if (addr == 0x1FF0) refs++;
        }
    }
    // F0 is characterized by frequent $1FF0 accesses (bank increment)
    return refs >= 3;
}

// Detect CV (Commavid) in small ROMs.
// CV carts are 2KB ROM + 1KB RAM. The ROM is only 2KB but the cart
// provides RAM in the lower 2KB range. We detect this by looking for
// code that writes to $1000-$13FF and reads from $1400-$17FF.
inline bool detect_cv(const uint8_t* rom, uint32_t size) {
    if (size != 2048) return false;

    int ram_write_refs = 0, ram_read_refs = 0;
    for (uint32_t i = 0; i + 2 < size; ++i) {
        uint8_t opcode = rom[i];
        uint16_t addr = rom[i + 1] | (static_cast<uint16_t>(rom[i + 2]) << 8);

        // Store to $1000-$13FF (RAM write port)
        if ((opcode == 0x8D || opcode == 0x8E || opcode == 0x8C) &&
            addr >= 0x1000 && addr <= 0x13FF)
            ram_write_refs++;
        // Load from $1400-$17FF (RAM read port)
        if ((opcode == 0xAD || opcode == 0xAE || opcode == 0xAC) &&
            addr >= 0x1400 && addr <= 0x17FF)
            ram_read_refs++;
    }
    return ram_write_refs >= 1 && ram_read_refs >= 1;
}

// ========================================================================
// PUBLIC API
// ========================================================================

// Create a mapper for the given ROM data and size.
// Auto-detects the banking scheme based on ROM size and content analysis.
inline std::unique_ptr<A2600Mapper> create(const uint8_t* rom, uint32_t size) {
    std::unique_ptr<A2600Mapper> mapper;

    // Starpath Supercharger: each multiload image is 8448 bytes
    if (size >= A2600MapperAR::LOAD_SIZE && (size % A2600MapperAR::LOAD_SIZE) == 0) {
        mapper = std::make_unique<A2600MapperAR>();
        log_info("Atari2600: Mapper = AR (Starpath Supercharger, %u load(s))\n",
                 size / A2600MapperAR::LOAD_SIZE);
    }
    else if (size <= 2048) {
        // 2KB: plain 2K card, or CV (Commavid) with RAM
        if (detect_cv(rom, size)) {
            mapper = std::make_unique<A2600MapperCV>();
            log_info("Atari2600: Mapper = CV (Commavid 2KB + RAM)\n");
        } else {
            mapper = std::make_unique<A2600Mapper2K>();
            log_info("Atari2600: Mapper = 2K (no bank switching)\n");
        }
    }
    else if (size <= 4096) {
        mapper = std::make_unique<A2600Mapper4K>();
        log_info("Atari2600: Mapper = 4K (no bank switching)\n");
    }
    else if (size == 8192) {
        // 8KB: check E0, FE, UA first (distinctive schemes), then F8/F8SC
        if (detect_e0(rom, size)) {
            mapper = std::make_unique<A2600MapperE0>();
            log_info("Atari2600: Mapper = E0 (Parker Brothers)\n");
        } else if (detect_fe(rom, size)) {
            mapper = std::make_unique<A2600MapperFE>();
            log_info("Atari2600: Mapper = FE (Activision)\n");
        } else if (detect_ua(rom, size)) {
            mapper = std::make_unique<A2600MapperUA>();
            log_info("Atari2600: Mapper = UA (UA Limited)\n");
        } else if (detect_superchip(rom, size)) {
            mapper = std::make_unique<A2600MapperF8SC>();
            log_info("Atari2600: Mapper = F8SC (8KB + Superchip)\n");
        } else {
            mapper = std::make_unique<A2600MapperF8>();
            log_info("Atari2600: Mapper = F8 (standard 8KB)\n");
        }
    }
    else if (size == 12288) {
        mapper = std::make_unique<A2600MapperFA>();
        log_info("Atari2600: Mapper = FA (CBS RAM Plus 12KB)\n");
    }
    else if (size == 16384) {
        // 16KB: check E7 (M-Network) first, then F6/F6SC
        if (detect_e7(rom, size)) {
            mapper = std::make_unique<A2600MapperE7>();
            log_info("Atari2600: Mapper = E7 (M-Network 16KB)\n");
        } else if (detect_superchip(rom, size)) {
            mapper = std::make_unique<A2600MapperF6SC>();
            log_info("Atari2600: Mapper = F6SC (16KB + Superchip)\n");
        } else {
            mapper = std::make_unique<A2600MapperF6>();
            log_info("Atari2600: Mapper = F6 (standard 16KB)\n");
        }
    }
    else if (size == 32768) {
        // 32KB: check 3E (Tigervision+RAM) before 3F, then F4/F4SC
        if (detect_3e(rom, size)) {
            mapper = std::make_unique<A2600Mapper3E>();
            log_info("Atari2600: Mapper = 3E (Tigervision + RAM 32KB)\n");
        } else if (detect_3f(rom, size)) {
            mapper = std::make_unique<A2600Mapper3F>();
            log_info("Atari2600: Mapper = 3F (Tigervision 32KB)\n");
        } else if (detect_superchip(rom, size)) {
            mapper = std::make_unique<A2600MapperF4SC>();
            log_info("Atari2600: Mapper = F4SC (32KB + Superchip)\n");
        } else {
            mapper = std::make_unique<A2600MapperF4>();
            log_info("Atari2600: Mapper = F4 (standard 32KB)\n");
        }
    }
    else if (size == 65536) {
        // 64KB: check F0 (Megaboy), then EF/EFSC, fall back to 3F
        if (detect_f0(rom, size)) {
            mapper = std::make_unique<A2600MapperF0>();
            log_info("Atari2600: Mapper = F0 (Megaboy 64KB)\n");
        } else if (detect_ef(rom, size)) {
            if (detect_superchip(rom, size)) {
                mapper = std::make_unique<A2600MapperEFSC>();
                log_info("Atari2600: Mapper = EFSC (64KB + Superchip)\n");
            } else {
                mapper = std::make_unique<A2600MapperEF>();
                log_info("Atari2600: Mapper = EF (64KB homebrew)\n");
            }
        } else {
            mapper = std::make_unique<A2600Mapper3F>();
            log_info("Atari2600: Mapper = 3F (Tigervision 64KB)\n");
        }
    }
    else if (size > 65536) {
        // Large ROMs: check 3E then 3F
        if (detect_3e(rom, size)) {
            mapper = std::make_unique<A2600Mapper3E>();
            log_info("Atari2600: Mapper = 3E (Tigervision + RAM %uKB)\n", size / 1024);
        } else {
            mapper = std::make_unique<A2600Mapper3F>();
            log_info("Atari2600: Mapper = 3F (Tigervision %uKB)\n", size / 1024);
        }
    }
    else {
        // Unusual size — fall back to 4K with mirroring
        mapper = std::make_unique<A2600Mapper4K>();
        log_info("Atari2600: Mapper = 4K (fallback for %u bytes)\n", size);
    }

    mapper->set_rom(rom, size);
    mapper->reset();
    return mapper;
}

} // namespace a2600_mapper_factory
