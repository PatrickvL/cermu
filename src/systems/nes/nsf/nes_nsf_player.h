#pragma once
/**
 * NES NSF Player — Music File Playback
 *
 * Handles loading and playback of NSF (NES Sound Format) music files
 * on the NES system, analogous to c64_sid_player for the Commodore 64.
 *
 * Workflow:
 *   1. NSF file is parsed by nsf_format.cpp → nsf_header_t in metadata
 *   2. NsfPlayer::apply_load() creates an NsfCartridge, uploads font,
 *      writes info page, builds a 6502 player stub, and starts the CPU
 *   3. Subtune selection is handled by NsfPlayer::switch_subtune()
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
class NsfCartridge;

/// Static-only class encapsulating all NSF player operations.
struct NsfPlayer {
    /// Write the NSF info page to the NES PPU nametable.
    static void write_info_page(PPU* ppu,
                                const nsf_header_t* nsf,
                                uint16_t subtune);

    /// Apply a full NSF load to the NES system.
    /// Returns the created NsfCartridge (caller keeps reference for subtune switching).
    static std::shared_ptr<NsfCartridge> apply_load(
        RICOH_2A03* cpu,
        PPU* ppu,
        uint8_t* cpu_ram,
        const nsf_header_t* nsf,
        const program_data_t* prog,
        uint16_t subtune,
        bool is_pal);

    /// Switch to a different subtune without full reload.
    static void switch_subtune(
        RICOH_2A03* cpu,
        PPU* ppu,
        uint8_t* cpu_ram,
        NsfCartridge* nsf_cart,
        const nsf_header_t* nsf,
        const uint8_t* payload,
        size_t payload_size,
        uint16_t subtune,
        bool is_pal);

private:
    // Stub addresses in NES CPU RAM
    static constexpr uint16_t STUB_BASE    = 0x0700;
    static constexpr uint16_t NMI_HANDLER  = 0x0750;
    static constexpr uint16_t IRQ_HANDLER  = 0x0770;

    // NES CPU vectors (in ROM space — written to NsfCartridge)
    static constexpr uint16_t VECTOR_NMI   = 0xFFFA;
    static constexpr uint16_t VECTOR_RESET = 0xFFFC;
    static constexpr uint16_t VECTOR_IRQ   = 0xFFFE;

    // Internal helpers
    static void build_nmi_handler(uint8_t* ram, uint16_t play_addr);
    static void build_irq_handler(uint8_t* ram);
    static void build_init_stub(uint8_t* ram, const nsf_header_t* nsf,
                                uint16_t subtune, bool is_pal);
    static void write_vectors(NsfCartridge* cart);
};

} // namespace nes_system
