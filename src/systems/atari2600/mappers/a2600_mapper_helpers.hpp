#pragma once
/*
 * a2600_mapper_helpers.h — Composable utilities for Atari 2600 mappers
 *
 * Reusable building blocks that multiple mappers share, following the
 * same composable pattern as NES mapper_helpers (MMC3IRQ, etc.).
 */

#include <cstdint>
#include <cstring>

// ============================================================================
// SuperchipRAM — 128 bytes of extra RAM used by -SC mapper variants
// ============================================================================
//
// On real hardware, the Superchip is a 128×8 static RAM chip wired into
// the cartridge's 4KB address window at the lowest 256 bytes:
//
//   Write port: offsets $000–$07F (writes to RAM)
//   Read port:  offsets $080–$0FF (reads from RAM)
//
// The split-port design is necessary because the 6502 reads every address
// it writes to, so a single-port scheme would corrupt RAM. By placing the
// write port below the read port, the stale read on a write cycle goes to
// the read port (harmless) while the actual write targets the write port.
//
// Used by: F8SC, F6SC, F4SC, EFSC (and any future SC-family variant).

struct SuperchipRAM {
    uint8_t data[128] = {};

    void reset() { memset(data, 0, sizeof(data)); }

    // Returns true if offset is in the write port range ($000–$07F).
    // Caller should store `val` and NOT return ROM data.
    static inline bool is_write_port(uint16_t offset) {
        return offset <= 0x007F;
    }

    // Returns true if offset is in the read port range ($080–$0FF).
    // Caller should return data[] instead of ROM.
    static inline bool is_read_port(uint16_t offset) {
        return offset >= 0x0080 && offset <= 0x00FF;
    }

    // Handle a read at the given 12-bit offset.
    // Returns true + sets `out` if the address is in the read port.
    inline bool try_read(uint16_t offset, uint8_t& out) const {
        if (is_read_port(offset)) {
            out = data[offset & 0x7F];
            return true;
        }
        return false;
    }

    // Handle a write at the given 12-bit offset.
    // Returns true if the address is in the write port.
    inline bool try_write(uint16_t offset, uint8_t val) {
        if (is_write_port(offset)) {
            data[offset & 0x7F] = val;
            return true;
        }
        return false;
    }
};
