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

#include "io_chip_base.h"
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

// Backward compat: old REG_TABLE is just the REG rows from the DECL
#define KC85_MOD_REG_TABLE(X) KC85_MOD_DECL(X, DECL_FLD_NOP, DECL_CMP_NOP)

namespace kc85_mod_regs {
    #define KC85_X_CONST_(a, s, l) constexpr uint8_t s = a;
    KC85_MOD_REG_TABLE(KC85_X_CONST_)
    #undef KC85_X_CONST_
    constexpr uint8_t REG_COUNT = 8;
} // namespace kc85_mod_regs

// --- RegEntry ---
#define KC85_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry KC85_MOD_REG_INFO[] = { KC85_MOD_REG_TABLE(KC85_X_INFO_) };
#undef KC85_X_INFO_

// --- FieldEntry ---
#define KC85_X_FLD_INFO_(reg, fld, hilo, desc, kind, ds, dm) \
    { #fld, desc, kc85_mod_regs::reg, BF_LO(hilo), BF_WIDTH(hilo), DataKind::kind, (uint8_t)(ds), (uint16_t)(dm) },
static constexpr FieldEntry KC85_MOD_FLD_INFO[] = {
    KC85_MOD_DECL(DECL_REG_NOP, KC85_X_FLD_INFO_, DECL_CMP_NOP)
};
#undef KC85_X_FLD_INFO_
static constexpr size_t KC85_MOD_NUM_FIELDS = sizeof(KC85_MOD_FLD_INFO) / sizeof(KC85_MOD_FLD_INFO[0]);

// --- DeclOrder ---
#define KC85_X_ORD_REG_(a, s, l)                                              { DeclRowType::Reg, (uint16_t)(a) },
#define KC85_X_ORD_FLD_(r, f, hilo, d, k, ds, dm)                            { DeclRowType::Field, 0 },
#define KC85_X_ORD_CMP_(s, d, k, b, ds, dm, r1, h1, d1, r2, h2, d2)         { DeclRowType::Compound, 0 },
static constexpr DeclOrderEntry KC85_MOD_DECL_ORDER_RAW[] = {
    KC85_MOD_DECL(KC85_X_ORD_REG_, KC85_X_ORD_FLD_, KC85_X_ORD_CMP_)
};
#undef KC85_X_ORD_REG_
#undef KC85_X_ORD_FLD_
#undef KC85_X_ORD_CMP_
static constexpr auto KC85_MOD_DECL_ORDER = assign_decl_indices(KC85_MOD_DECL_ORDER_RAW);

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

    // Register file mirror (slot control bytes)
    uint8_t regs_[kc85_mod_regs::REG_COUNT]{};

    void update_regs() {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            regs_[i] = slots_[i].control;
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_.set_registers(regs_, kc85_mod_regs::REG_COUNT, KC85_MOD_REG_INFO);
        debug_registry_.set_decl_order(KC85_MOD_DECL_ORDER.data(), KC85_MOD_DECL_ORDER.size(),
                                       KC85_MOD_FLD_INFO, KC85_MOD_NUM_FIELDS,
                                       nullptr, 0, nullptr);
        // All slot control values and Active/WProt flags are in the DECL walk.
    }
#endif
};
