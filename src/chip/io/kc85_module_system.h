#pragma once
/*
 * kc85_module_system.h — KC85 Module Slot Controller
 *
 * The KC85 series (KC85/2, /3, /4) features a modular expansion system
 * with 8-bit D-Bus module slots.  Each slot provides address mapping,
 * active/inactive control, and optionally write-protect for ROM modules.
 *
 * Slot structure:
 *   - KC85/2: 2 internal slots + 4 expansion slots (D002 bus driver)
 *   - KC85/3: same as /2
 *   - KC85/4: built-in 2×64KB video RAM, extended module system
 *
 * Module types include: RAM (16KB/64KB), ROM (BASIC, user), I/O,
 *   joystick interface, V.24 serial, floppy controller, etc.
 *
 * Control: Each slot has a control byte:
 *   Bit 0: Module active (1 = mapped into address space)
 *   Bits 1-7: Module-specific (e.g. write-protect, bank select)
 *
 * I/O ports: Each slot pair shares an I/O port (typically $08/$0C etc.)
 *   Port+0: slot status read / structure byte of inserted module
 *   Port+1: slot control write
 */

#include "../../core/chip.h"
#include <cstdint>
#include <cstring>

// ============================================================================
// KC85 Module Types
// ============================================================================

enum class KC85ModuleType : uint8_t {
    NONE    = 0x00,  // Empty slot
    RAM_16K = 0xF6,  // M022: 16KB RAM
    RAM_64K = 0x79,  // M032: 64KB segmented RAM
    ROM_BASIC = 0xFB,// M006: BASIC ROM
    ROM_USER  = 0xFC,// User ROM module
    // ... more module types
};

// ============================================================================
// KC85 Module Slot
// ============================================================================

struct kc85_module_slot_t {
    KC85ModuleType type = KC85ModuleType::NONE;
    uint8_t  control = 0x00;      // Control byte
    uint8_t  structure_byte = 0x00; // Module identification byte
    uint8_t* data = nullptr;       // Module memory (ROM or RAM), non-owning
    uint32_t size = 0;             // Module memory size
    uint16_t base_address = 0;    // Base address in Z80 address space

    bool is_active() const { return (control & 0x01) != 0; }
    bool is_write_protected() const { return (control & 0x02) != 0; }
};

// ============================================================================
// KC85 Module System Controller
// ============================================================================

class kc85_module_system_t : public ChipBase {
public:
    static constexpr int MAX_SLOTS = 8;

    kc85_module_system_t()
        : ChipBase(ChipInfo("Module System", "VEB Mikroelektronik"))
    {
        category_ = "I/O";
    }

    void init() {
        for (auto& slot : slots_) {
            slot = {};
        }
    }

    void reset() {
        for (auto& slot : slots_) {
            slot.control = 0x00;  // Deactivate all modules
        }
    }

    // === Slot management ===

    /// Insert a module into a slot.
    void insert_module(int slot_idx, KC85ModuleType type, uint8_t* data,
                       uint32_t size, uint16_t base_addr) {
        if (slot_idx >= MAX_SLOTS) return;
        auto& slot = slots_[slot_idx];
        slot.type = type;
        slot.structure_byte = static_cast<uint8_t>(type);
        slot.data = data;
        slot.size = size;
        slot.base_address = base_addr;
        slot.control = 0x00;
    }

    /// Remove module from a slot.
    void remove_module(int slot_idx) {
        if (slot_idx >= MAX_SLOTS) return;
        slots_[slot_idx] = {};
    }

    // === I/O port access ===

    /// Read module structure byte (slot identification).
    uint8_t read_slot_status(int slot_idx) const {
        if (slot_idx >= MAX_SLOTS) return 0x00;
        return slots_[slot_idx].structure_byte;
    }

    /// Write module control byte.
    void write_slot_control(int slot_idx, uint8_t data) {
        if (slot_idx >= MAX_SLOTS) return;
        slots_[slot_idx].control = data;
    }

    /// Get a slot (for address-space mapping by the system).
    const kc85_module_slot_t& get_slot(int slot_idx) const {
        return slots_[slot_idx < MAX_SLOTS ? slot_idx : 0];
    }

private:
    kc85_module_slot_t slots_[MAX_SLOTS]{};
};
