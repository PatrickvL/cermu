#pragma once
/**
 * NES NSF Player — Music File Playback
 *
 * Handles loading and playback of NSF (NES Sound Format) music files
 * on the NES system, analogous to c64_sid_player for the Commodore 64.
 *
 * Workflow:
 *   1. NSF file is parsed by nsf_format.cpp → nsf_header_t in metadata
 *   2. nes_system.cpp creates a standard Cartridge with MapperNsf
 *   3. NsfPlayer::setup_cpu() builds 6502 stubs, writes vectors, resets CPU
 *   4. NsfPlayer::write_info_page() renders the NSF info display
 *   5. Subtune selection calls NsfPlayer::switch_subtune()
 *
 * The 6502 stub uses NMI (VBlank) interrupts for play routine timing,
 * which is the standard mechanism for NSF playback on the NES.
 */

#include "../../../core/formats/nsf_format.h"
#include "../../../core/formats/format_handler.h"
#include "../../../chip/cpu/fam65xx/ricoh_2a03.h"
#include <cstdint>
#include <memory>

namespace nes_system {

class PPU;
class Mapper;

/// Static-only class encapsulating all NSF player operations.
struct NsfPlayer {
    /// Write the NSF info page to the NES PPU nametable.
    static void write_info_page(PPU* ppu,
                                const nsf_header_t* nsf,
                                uint16_t subtune);

    /// Build 6502 stubs in CPU RAM, write vectors to ROM buffer, reset CPU.
    /// @param prg_rom      Mutable pointer to NSF ROM in unified buffer
    /// @param prg_rom_size  Size of the ROM buffer
    /// @param bankswitched  Whether NSF uses bank switching
    /// @param bank_regs     Current bank register values (for vector mapping)
    /// @param load_addr     NSF load address (for non-bankswitched vector mapping)
    static void setup_cpu(
        RICOH_2A03* cpu,
        uint8_t* cpu_ram,
        uint8_t* prg_rom,
        size_t prg_rom_size,
        bool bankswitched,
        const uint8_t bank_regs[8],
        uint16_t load_addr,
        const nsf_header_t* nsf,
        uint16_t subtune,
        bool is_pal);

    /// Switch to a different subtune.
    /// Reloads ROM data, rebuilds stubs, writes vectors, resets CPU.
    /// @param mapper         The MapperNsf instance (for set_bank_regs)
    static void switch_subtune(
        RICOH_2A03* cpu,
        PPU* ppu,
        uint8_t* cpu_ram,
        uint8_t* prg_rom,
        size_t prg_rom_size,
        Mapper* mapper,
        const nsf_header_t* nsf,
        const uint8_t* payload,
        size_t payload_size,
        uint16_t subtune,
        bool is_pal);

    /// Write CPU vectors ($FFFA-$FFFF) into the ROM buffer.
    /// Maps through bank registers for bankswitched NSFs.
    static void write_vectors(
        uint8_t* prg_rom,
        size_t prg_rom_size,
        bool bankswitched,
        const uint8_t bank_regs[8],
        uint16_t load_addr);

private:
    // Stub addresses in NES CPU RAM
    static constexpr uint16_t STUB_BASE    = 0x0700;
    static constexpr uint16_t NMI_HANDLER  = 0x0750;
    static constexpr uint16_t IRQ_HANDLER  = 0x0770;

    // NES CPU vectors (in ROM space — written to ROM buffer)
    static constexpr uint16_t VECTOR_NMI   = 0xFFFA;
    static constexpr uint16_t VECTOR_RESET = 0xFFFC;
    static constexpr uint16_t VECTOR_IRQ   = 0xFFFE;

    // Internal helpers
    static void build_nmi_handler(uint8_t* ram, uint16_t play_addr);
    static void build_irq_handler(uint8_t* ram);
    static void build_init_stub(uint8_t* ram, const nsf_header_t* nsf,
                                uint16_t subtune, bool is_pal);

    /// Map a CPU vector address to a ROM buffer offset.
    /// Returns -1 if the address doesn't map into the ROM.
    static int32_t map_vector_offset(
        uint16_t addr,
        bool bankswitched,
        const uint8_t bank_regs[8],
        uint16_t load_addr,
        size_t prg_rom_size);
};

} // namespace nes_system
