#pragma once

#include <cstddef>
#include <cstdint>

#include "../../core/cermu.h"
#include "../../core/system_lines.h"
#include "c64_chips.h"

// =============================
// Bus Types & Macros
// =============================

// System line masks for cartridge signals (moved out of control lines to separate field)
#define SYS_MASK_EXROM (1 << 0)   // EXROM signal (bit 0)
#define SYS_MASK_GAME  (1 << 1)   // GAME signal (bit 1)

// C64 default bus state with pull-up resistors
// This represents the hardware state at the start of each cycle before any chip asserts lines:
// - Data bus: 0xFF (pull-ups on all 8 data lines)
// - IRQ, NMI: HIGH via pull-ups (inactive, active-low signals)
// - RDY: HIGH via pull-up (CPU ready)
// - BA: HIGH via pull-up (bus available, VIC-II pulls LOW during badlines)
// - AEC: HIGH via pull-up (CPU controls address bus, VIC-II pulls LOW to take control)
// - RW: HIGH (READ mode, pull-up on R/W line defaults to read)
// - RES: HIGH (not in reset, active-low signal)
//
// CRITICAL FIX: IRQ/NMI/RES are at bits 32-34, NOT in the 8-bit lines field!
// Must use BUS_BIT() to set them directly, not BUS_MASK_* which are for the legacy lines field.
#define C64_BUS_DEFAULT_STATE() \
    (BUS_STATE(0, 0xFF, BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY | BUS_MASK_RW) | BUS_BIT(BUS_RES_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_NMI_BIT) | BUS_BIT(BUS_CNT_BIT) | BUS_BIT(BUS_FLAG_BIT))

// Forward declaration to avoid circular dependency with c64_system.h
class C64System;
struct pla_906114_01_t;

// C64 bus controller structure
struct c64_bus_t {
    C64System* c64;  // Typed back-pointer to owning C64System instance
    bus_state_t default_state; // Default bus state with pull-up resistors (start of each tick)
    bus_state_t state;         // Current bus state (after all chip ticks)
    // System lines for control signals (includes EXROM and GAME)
    uint8_t system_lines;  // System-wide control lines including cartridge signals

    // Current PLA banking mode (0-31) derived from CPU port + cartridge signals
    uint8_t pla_banking_mode;  // Current banking mode for fast switching

    // UNIFIED MEMORY BUFFER FOR OPTIMIZED OPCODE FETCH - STRATEGIC LAYOUT
    // Layout optimized for branchless calculation: ROML + ROMH + KERNAL + BASIC + CHARROM + RAM
    // Offsets: ROML=0x0000, ROMH=0x2000, KERNAL=0x4000, BASIC=0x6000, CHARROM=0x8000, RAM=0x9000
    // Total: Up to 100KB unified buffer (36KB ROM space + 64KB RAM) for branchless memory access
    // Strategic CHIP numbering enables pure arithmetic: offset = chip << 12 (chip * 4096)
    // Dynamic allocation skips unused cartridge ROMs at buffer start to save memory
    uint8_t* unified_memory_buffer;   // Points to usable memory (may be offset from allocated memory)
    uint8_t* allocated_buffer;        // Points to actual allocated memory
    size_t allocated_size;            // Actual allocated size
    bool roml_present;                // Whether ROML cartridge ROM is attached
    bool romh_present;                // Whether ROMH cartridge ROM is attached

    // OPTIMIZED MEMORY BANKING - Cache-friendly layout
    // Banking configurations per mode (32 modes x 16 banks = 512 bytes)
    alignas(64) uint8_t cpu_encoded_chip_per_bank_per_mode[32][16]; // Encoded chip select for all PLA modes
    // 16 bytes: cpu_encoded_chip_per_bank mapping (4KB banks 0-15) - fits in single cache line
    alignas(16) uint8_t cpu_encoded_chip_per_bank[16]; // CPU banking configurations per mode

    // VIC-II active array for optimized access (raw CHIPs, no encoding)
    alignas(16) uint8_t vicii_chip_per_bank[16];

    // VIC-II banking configurations per mode (32 modes x 16 banks = 512 bytes)
    // VIC-II uses direct CHIP values, not encoded, since it only does read accesses
    alignas(64) uint8_t vicii_chip_per_bank_per_mode[32][16]; // VIC-II direct CHIP per mode

    // COMPACT I/O PAGE MAPPING - Efficient approach using IO page numbers
    // Maps IO page numbers (0-15 for $D000-$DFFF) directly to chip handlers
    // This is more efficient than the previous callback array approach
    struct io_page_handlers_t {
        bus_state_t (*read_handler)(void* context, bus_state_t bus_state);  // Direct chip register function signature
        void* chip_instance;  // Direct pointer to the chip instance for this IO page
        bus_state_t (*write_handler)(void* context, bus_state_t bus_state); // Direct chip register function signature
    };
    io_page_handlers_t io_handlers[16]; // One handler per IO page (0-15)

    // =============================
    // Methods
    // =============================

    ~c64_bus_t();

    void mode_switch(uint8_t mode);
    uint8_t generate_pla_mode(uint8_t cpu_port_bits);
    bus_state_t vic_read(bus_state_t bus_state, uint16_t address);
    bus_state_t REGISTER_CALL memory_tick(bus_state_t bus_state);
    void init_unified_pointers(C64System* c64_system, bool roml_present = false, bool romh_present = false);
    void system_attach(C64System* c64);
    void set_exrom_signal(bool active);
    void set_game_signal(bool active);
    void set_cartridge_signals(bool exrom_active, bool game_active);
    bool get_exrom_signal() const;
    bool get_game_signal() const;
    void on_banking_change(uint8_t banking_state);
    void populate_cpu_pla_mapping(struct pla_906114_01_t* pla);
    void populate_vicii_pla_mapping(struct pla_906114_01_t* pla);
    void generate_all_pla_modes(struct pla_906114_01_t* pla);
    void init_io_handlers();
    bool get_chip_description(uint8_t chip, chip_description_t* out) const;
    uint8_t read_memory(uint16_t addr);
    void write_memory(uint16_t addr, uint8_t value);

    // Ultra-optimized unified address calculation — pure branchless arithmetic
    static uint32_t unified_address_calc(uint8_t chip, uint16_t addr) {
        const uint32_t base = (uint32_t)chip << 12;
        const uint32_t mask = 0x1FFF | -(chip == CHIP_RAM);
        return base + (addr & mask);
    }

    void write_chip_byte(uint8_t chip, uint16_t address, uint8_t value) {
        unified_memory_buffer[unified_address_calc(chip, address)] = value;
    }

    void write_ram_byte(uint16_t address, uint8_t value) {
        write_chip_byte(CHIP_RAM, address, value);
    }

    uint8_t read_chip_byte(uint8_t chip, uint16_t address) const {
        return unified_memory_buffer[unified_address_calc(chip, address)];
    }

    uint8_t read_kernal_byte(uint16_t address) const {
        return read_chip_byte(CHIP_KERNAL, address);
    }

    uint16_t read_kernal_reset_vector() const {
        uint8_t reset_low = read_kernal_byte(0xFFFC);
        uint8_t reset_high = read_kernal_byte(0xFFFD);
        return (reset_high << 8) | reset_low;
    }

private:
    void update_pla_mode();
};

// Free functions (no bus param)
const char* c64_bus_chip_to_title(uint8_t chip);
const char* c64_bus_size_to_str(size_t size);
