#pragma once
/*
 * nes_bus.h -- NES unified bus structure with block-number dispatch
 *
 * All NES memory lives in a contiguous unified buffer of 1KB blocks.
 * CPU and PPU reads resolve to: unified_buf[(block << BLOCK_SHIFT) | sub_addr]
 *
 * CPU address space: 16 x 4KB pages ($0000-$FFFF)
 *   Pages 0-1  ($0000-$1FFF): WRAM (fast path, & 0x07FF mirroring)
 *   Pages 2-3  ($2000-$3FFF): BLOCK_PPU_REGS -> PPU register I/O dispatch
 *   Page 4     ($4000-$4FFF): BLOCK_APU_IO -> APU/IO register dispatch
 *   Page 5     ($5000-$5FFF): expansion (mapper-dependent)
 *   Pages 6-7  ($6000-$7FFF): PRG-RAM (mapper-dependent)
 *   Pages 8-15 ($8000-$FFFF): PRG-ROM banks (mapper-configured)
 *
 * PPU address space: 16 x 1KB pages ($0000-$3FFF)
 *   Pages 0-7   ($0000-$1FFF): CHR-ROM/RAM banks
 *   Pages 8-11  ($2000-$2FFF): Nametable (CIRAM) with mirroring
 *   Pages 12-15 ($3000-$3FFF): Mirror of $2000-$2FFF
 *   Palette     ($3F00-$3F1F): PPU-internal, not through bus
 */

#include <cstdint>
#include <cstring>

#include "core/cermu.hpp"         // likely/unlikely
#include "core/system_lines.hpp"  // bus_state_t, BUS_* macros
#include "systems/nes/bus/nes_bus_chips.hpp"               // BLOCK_*, unified buffer constants
#include "systems/nes/cartridge/nes_mapper.hpp"     // MapperBankConfig, MapperChrConfig

namespace nes_bus {

// ============================================================================
// BUS SIZING CONSTANTS
// ============================================================================

inline constexpr uint32_t CPU_PAGE_COUNT = 16;     // 16 x 4KB CPU pages
inline constexpr uint32_t CPU_PAGE_SHIFT = 12;     // 4KB = 1 << 12
inline constexpr uint32_t CPU_PAGE_MASK  = 0x0FFF; // 4KB - 1

inline constexpr uint32_t PPU_PAGE_COUNT = 16;     // 16 x 1KB PPU pages
inline constexpr uint32_t PPU_PAGE_SHIFT = 10;     // 1KB = 1 << 10
inline constexpr uint32_t PPU_PAGE_MASK  = 0x03FF; // 1KB - 1

// ============================================================================
// NES BUS STRUCTURE
// ============================================================================

struct nes_bus_t {
    // Non-copyable (owns unified buffer)
    nes_bus_t() = default;
    ~nes_bus_t();
    nes_bus_t(const nes_bus_t&) = delete;
    nes_bus_t& operator=(const nes_bus_t&) = delete;

    // ====================================================================
    // Unified memory buffer -- all NES memory in consecutive 1KB blocks.
    // Layout: [WRAM|CIRAM|reserved|PRG-RAM|CHR...|PRG-ROM...]
    // See nes_bus_chips.h for block assignments and sizing.
    // ====================================================================
    uint8_t* unified_buf = nullptr;
    uint32_t unified_buf_size = 0;

    // Convenience pointers into unified buffer (set by init / init_unified_buffer)
    uint8_t* cpu_ram = nullptr;        // -> BLOCK_WRAM   (WRAM_SIZE bytes)
    uint8_t* ciram = nullptr;          // -> BLOCK_CIRAM  (CIRAM_SIZE bytes)
    uint8_t* prg_ram = nullptr;        // -> BLOCK_PRG_RAM (prg_ram_size bytes)
    uint8_t* chr_data_ptr = nullptr;   // -> dynamic CHR region
    uint8_t* prg_rom_ptr = nullptr;       // -> dynamic PRG-ROM region (unified buf copy)

    // Region sizes (set by init_unified_buffer)
    uint32_t prg_ram_size = 0;
    uint32_t chr_data_size = 0;
    bool     chr_is_ram = false;
    uint32_t prg_rom_size = 0;

    // Block mapping metadata
    uint16_t chr_base_block = 0;       // First block of CHR data
    uint16_t prg_rom_base_block = 0;   // First block of PRG-ROM
    uint16_t total_blocks = 0;         // Total blocks in buffer

    // ====================================================================
    // Memory block arrays -- block number per page
    // ====================================================================
    //
    // CPU: 16 x 4KB pages. Each entry is the first of 4 consecutive
    //      1KB blocks. Read: unified_buf[(block << 10) | (addr & 0xFFF)]
    // PPU: 16 x 1KB pages. Each entry is a single block number.
    //      Read: unified_buf[(block << 10) | (addr & 0x3FF)]
    //
    // Sentinel values (>= BLOCK_SENTINEL_MIN) trigger I/O dispatch.
    //
    alignas(16) uint16_t cpu_read_block[CPU_PAGE_COUNT];
    alignas(16) uint16_t cpu_write_block[CPU_PAGE_COUNT];
    alignas(16) uint16_t ppu_read_block[PPU_PAGE_COUNT];
    alignas(16) uint16_t ppu_write_block[PPU_PAGE_COUNT];

    // (Legacy page pointer tables removed -- all dispatch uses block arrays.)

    // ====================================================================
    // Initialization
    // ====================================================================

    /// Allocate fixed-region buffer (WRAM, CIRAM, PRG-RAM space).
    /// Must be called once before any bus access.
    void init();

    /// Extend unified buffer with cartridge ROM/RAM data.
    /// Reallocates to include CHR + PRG-ROM alongside fixed regions.
    /// Preserves any existing WRAM content.
    void init_unified_buffer(const uint8_t* prg_rom_data, size_t prg_rom_sz,
                              const uint8_t* chr_data_in, size_t chr_sz,
                              bool chr_is_ram_in,
                              const uint8_t* prg_ram_data, size_t prg_ram_sz);

    // ====================================================================
    // Bank map update -- called after mapper register writes
    // ====================================================================

    /// Set CPU page pointers + block arrays from mapper's PRG bank config.
    void update_cpu_banks(const nes_system::MapperBankConfig& config);

    /// Set PPU page pointers + block arrays from mapper's CHR bank config.
    /// ciram_ptr points to the CIRAM region (bus_.ciram after Phase B).
    void update_ppu_banks(const nes_system::MapperChrConfig& config, uint8_t* ciram_ptr);

    // ====================================================================
    // Block helpers
    // ====================================================================

    /// Convert a pointer into the unified buffer to a block number.
    /// Returns BLOCK_OPEN_BUS for null or out-of-range pointers.
    inline uint16_t ptr_to_block(const uint8_t* ptr) const {
        if (!ptr || !unified_buf) return BLOCK_OPEN_BUS;
        ptrdiff_t offset = ptr - unified_buf;
        if (offset < 0 || offset >= static_cast<ptrdiff_t>(unified_buf_size))
            return BLOCK_OPEN_BUS;
        return static_cast<uint16_t>(offset >> BLOCK_SHIFT);
    }

    /// CPU block read -- 4KB page from block number.
    /// block must be < BLOCK_SENTINEL_MIN (caller checks).
    /// WRAM (blocks 0-1) mirrors at 2KB; all other regions use 4KB pages.
    /// Branchless: when block < BLOCK_CIRAM, XOR clears bit 11 → 0x07FF mask.
    /// Uses addition (not OR) because block numbers may not be 4-aligned,
    /// so bits 10-11 of sub_addr can overlap with the block field.
    inline uint8_t cpu_block_read(uint16_t block, uint16_t addr) const {
        const uint16_t mask = CPU_PAGE_MASK ^ (0x0800u * (block < BLOCK_CIRAM));
        return unified_buf[(static_cast<uint32_t>(block) << BLOCK_SHIFT) + (addr & mask)];
    }

    /// CPU block write -- 4KB page from block number.
    /// block must be < BLOCK_SENTINEL_MIN (caller checks).
    inline void cpu_block_write(uint16_t block, uint16_t addr, uint8_t data) {
        const uint16_t mask = CPU_PAGE_MASK ^ (0x0800u * (block < BLOCK_CIRAM));
        unified_buf[(static_cast<uint32_t>(block) << BLOCK_SHIFT) + (addr & mask)] = data;
    }

    // ====================================================================
    // Inline CPU read helper -- for DMA and debug peek
    // ====================================================================
    //
    // Does NOT handle I/O dispatch (PPU regs, APU, controllers).
    // Returns open bus (0xFF) for sentinel blocks.

    inline uint8_t cpu_read(uint16_t addr) const {
        uint16_t block = cpu_read_block[addr >> CPU_PAGE_SHIFT];
        if (likely(block < BLOCK_SENTINEL_MIN)) {
            return cpu_block_read(block, addr);
        }
        return 0xFF;  // open bus / I/O sentinel
    }

    // ====================================================================
    // Inline PPU read/write -- block-based dispatch
    // ====================================================================
    //
    // Palette ($3F00-$3F1F) must be intercepted BEFORE calling these.

    /// PPU block read -- 1KB page from block number.
    /// block must be < BLOCK_SENTINEL_MIN (caller checks).
    inline uint8_t ppu_block_read(uint16_t block, uint16_t addr) const {
        return unified_buf[(static_cast<uint32_t>(block) << BLOCK_SHIFT) | (addr & PPU_PAGE_MASK)];
    }

    /// PPU block write -- 1KB page from block number.
    inline void ppu_block_write(uint16_t block, uint16_t addr, uint8_t data) {
        unified_buf[(static_cast<uint32_t>(block) << BLOCK_SHIFT) | (addr & PPU_PAGE_MASK)] = data;
    }
};

} // namespace nes_bus
