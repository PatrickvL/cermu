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

#include "chip/io/io_chip_base.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>

// ============================================================================
// KC85 Module System REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 8 registers, 16 fields (Active + WriteProtect per slot)
#define KC85_MOD_DECL(REG, FLD, CMP) \
    REG(0x00, SLOT0_CTRL, "Slot 0 control") \
      FLD(SLOT0_CTRL, ACTIVE_0, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT0_CTRL, WPROT_0,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x01, SLOT1_CTRL, "Slot 1 control") \
      FLD(SLOT1_CTRL, ACTIVE_1, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT1_CTRL, WPROT_1,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x02, SLOT2_CTRL, "Slot 2 control") \
      FLD(SLOT2_CTRL, ACTIVE_2, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT2_CTRL, WPROT_2,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x03, SLOT3_CTRL, "Slot 3 control") \
      FLD(SLOT3_CTRL, ACTIVE_3, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT3_CTRL, WPROT_3,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x04, SLOT4_CTRL, "Slot 4 control") \
      FLD(SLOT4_CTRL, ACTIVE_4, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT4_CTRL, WPROT_4,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x05, SLOT5_CTRL, "Slot 5 control") \
      FLD(SLOT5_CTRL, ACTIVE_5, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT5_CTRL, WPROT_5,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x06, SLOT6_CTRL, "Slot 6 control") \
      FLD(SLOT6_CTRL, ACTIVE_6, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT6_CTRL, WPROT_6,  1:1, "Write protect", Flag, 0, 0) \
    REG(0x07, SLOT7_CTRL, "Slot 7 control") \
      FLD(SLOT7_CTRL, ACTIVE_7, 0:0, "Active",        Flag, 0, 0) \
      FLD(SLOT7_CTRL, WPROT_7,  1:1, "Write protect", Flag, 0, 0)

namespace kc85_mod_regs {
    KC85_MOD_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t REG_COUNT = 8;
} // namespace kc85_mod_regs

DECL_EXTRACT(KC85_MOD, KC85_MOD_DECL)

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

class kc85_module_system_t : public IoChipBase {
public:
    static constexpr int MAX_SLOTS = 8;

    kc85_module_system_t()
        : IoChipBase(ChipInfo("Module System", "VEB Mikroelektronik"))
    {
        init_regs(kc85_mod_regs::REG_COUNT);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        for (auto& slot : slots_) {
            slot = {};
        }
        update_regs();
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
        update_regs();
    }

    /// Get a slot (for address-space mapping by the system).
    const kc85_module_slot_t& get_slot(int slot_idx) const {
        return slots_[slot_idx < MAX_SLOTS ? slot_idx : 0];
    }

private:
    kc85_module_slot_t slots_[MAX_SLOTS]{};

    // Register file mirror (slot control bytes — backed by ChipBase::regs_)

    void update_regs() {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            regs_[i] = slots_[i].control;
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_.set_registers(regs_, kc85_mod_regs::REG_COUNT, KC85_MOD_REG_INFO);
        debug_registry_.set_decl_entries(KC85_MOD_DECL_ENTRIES.data(), KC85_MOD_DECL_ENTRIES.size());
        // All slot control values and Active/WProt flags are in the DECL walk.
    }
#endif
};
