#pragma once
/**
 * NES NSF Player — Music File Playback
 *
 * Handles loading and playback of NSF (NES Sound Format) music files
 * on the NES system, analogous to c64_sid_player for the Commodore 64.
 *
 * Workflow:
 *   1. NSF file is parsed by nsf_format.cpp → nsf_header_t in metadata
 *   2. nes_apply_nsf_load() creates an NsfCartridge, uploads font,
 *      writes info page, builds a 6502 player stub, and starts the CPU
 *   3. Subtune selection is handled by nes_nsf_switch_subtune()
 *
 * The 6502 stub uses NMI (VBlank) interrupts for play routine timing,
 * which is the standard mechanism for NSF playback on the NES.
 */

#include "../../core/formats/nsf_format.h"
#include "../../core/formats/format_handler.h"
#include "../../chip/cpu/fam65xx/ricoh_2a03.h"
#include <cstdint>
#include <memory>

namespace nes_system {

class PPU;
class MemoryBus;
class Cartridge;
class NsfCartridge;

/**
 * Write the NSF info page to the NES PPU nametable.
 *
 * Displays a formatted screen showing:
 *   - Title bar with "NSF PLAYER"
 *   - Song title, artist, copyright
 *   - Technical details (load/init/play addresses, region, chips)
 *   - Current subtune / total subtunes
 *   - Key help
 *
 * @param ppu       PPU instance for nametable writes
 * @param nsf       Parsed NSF header
 * @param subtune   Current 0-based subtune index
 */
void nes_write_nsf_info_page(PPU* ppu,
                              const nsf_header_t* nsf,
                              uint16_t subtune);

/**
 * Apply a full NSF load to the NES system.
 *
 * This replaces the normal cartridge with an NsfCartridge, uploads
 * the font to CHR-RAM, writes the info page, builds the 6502 player
 * stub in CPU RAM, and sets the CPU to start executing it.
 *
 * @param cpu       NES 6502 CPU handle
 * @param ppu       PPU instance
 * @param bus       Memory bus
 * @param nsf       Parsed NSF header
 * @param prog      Program data (NSF payload)
 * @param subtune   Initial 0-based subtune index
 * @param is_pal    True for PAL timing
 * @return          The created NsfCartridge (caller keeps reference for subtune switching)
 */
std::shared_ptr<NsfCartridge> nes_apply_nsf_load(
    RICOH_2A03* cpu,
    PPU* ppu,
    MemoryBus* bus,
    const nsf_header_t* nsf,
    const program_data_t* prog,
    uint16_t subtune,
    bool is_pal);

/**
 * Switch to a different subtune without full reload.
 *
 * Silences the APU, reloads NSF data (in case of self-modification),
 * updates the info page, rebuilds the 6502 stub, and restarts the CPU.
 *
 * @param cpu           NES 6502 CPU handle
 * @param ppu           PPU instance
 * @param bus           Memory bus
 * @param nsf_cart      The NsfCartridge to reload data into
 * @param nsf           Parsed NSF header
 * @param payload       Original NSF payload data
 * @param payload_size  Size of the payload
 * @param subtune       New 0-based subtune index
 * @param is_pal        True for PAL timing
 */
void nes_nsf_switch_subtune(
    RICOH_2A03* cpu,
    PPU* ppu,
    MemoryBus* bus,
    NsfCartridge* nsf_cart,
    const nsf_header_t* nsf,
    const uint8_t* payload,
    size_t payload_size,
    uint16_t subtune,
    bool is_pal);

} // namespace nes_system
