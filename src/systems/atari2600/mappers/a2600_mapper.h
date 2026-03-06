#pragma once
/*
 * a2600_mapper.h — Atari 2600 cartridge bank-switching mapper base
 *
 * The Atari 2600 uses various bank-switching schemes identified by naming
 * conventions (F8, F6, E0, 3F, etc.). Each mapper handles reads and writes
 * in the cartridge address space ($1000–$1FFF, 12-bit offset 0x000–0xFFF)
 * and optionally snoops CPU bus activity for schemes that detect accesses
 * outside the cart space (e.g. 3F watches TIA writes, FE watches stack).
 *
 * Mapper lifecycle:
 *   1. Factory creates mapper based on ROM size + content analysis
 *   2. set_rom() provides a non-owning pointer to the ROM data
 *   3. reset() initializes to power-on state (typically selects last bank)
 *   4. read()/write() called per cart-space access
 *   5. bus_snoop() called per CPU cycle for snooping mappers
 */

#include <cstdint>
#include <cstring>

struct A2600Mapper {
    virtual ~A2600Mapper() = default;

    // Read from cartridge address space. offset is 12-bit (0x000–0xFFF).
    virtual uint8_t read(uint16_t offset) = 0;

    // Write to cartridge address space. offset is 12-bit (0x000–0xFFF).
    virtual void write(uint16_t offset, uint8_t data) { (void)offset; (void)data; }

    // Called on every CPU memory access for bus-snooping mappers (3F, FE).
    // addr is 13-bit (full 6507 address), data is the data bus value,
    // is_write is true for write cycles. Called AFTER the access is serviced,
    // so data reflects the actual value read/written.
    virtual void bus_snoop(uint16_t addr, uint8_t data, bool is_write) {
        (void)addr; (void)data; (void)is_write;
    }

    // Return true if bus_snoop() must be called every cycle.
    virtual bool needs_bus_snoop() const { return false; }

    // Display name of the banking scheme (e.g. "F8", "E0").
    virtual const char* name() const = 0;

    // Set ROM data pointer and size. Called once after creation.
    // The mapper does NOT own the ROM data.
    void set_rom(const uint8_t* rom, uint32_t size) {
        rom_ = rom;
        rom_size_ = size;
    }

    // Reset mapper state to power-on defaults.
    virtual void reset() = 0;

    // Current bank index (for display in configuration UI).
    virtual uint8_t current_bank() const { return 0; }

    // Total number of banks.
    virtual uint8_t bank_count() const { return 1; }

protected:
    const uint8_t* rom_ = nullptr;
    uint32_t rom_size_ = 0;
};
