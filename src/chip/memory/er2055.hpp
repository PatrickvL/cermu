#pragma once
/*
 * er2055.hpp — General Instrument ER2055 Electrically Alterable ROM (EAROM)
 *
 * 512-bit (64 × 8) Electrically Alterable Read Only Memory.
 * DIP-22 package, 0.4" row spacing.
 *
 * Atari part number: 137161-001
 *
 * Used on many Atari arcade PCBs for non-volatile high score storage:
 *   Asteroids Deluxe, Black Widow, Centipede, Dig Dug, Gravitar,
 *   Liberator, Millipede, Red Baron, Space Duel, Tempest, and others.
 *
 * Control pin truth table:
 *
 *    CS1* CS2  C1  C2  Operation
 *    ─────────────────────────────
 *     1    x    x   x  Standby (chip not selected)
 *     0    0    x   x  Standby (chip not selected)
 *     0    1    0   0  Standby
 *     0    1    1   0  Read:  data latch ← memory[address]
 *     0    1    0   1  Write: memory[address] ← data latch
 *     0    1    1   1  Erase: memory[address] ← 0x00
 *
 * All operations are clocked on the rising edge of CK.
 * The host CPU typically writes address + data, then strobes the control
 * register to trigger the operation.
 *
 * MAME reference: src/devices/machine/er2055.cpp
 */

#include "chip/memory/memory_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include "core/system_lines.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// ER2055 DECL — register map
// ============================================================================
//
// The ER2055's "registers" model its internal latches and control state,
// stored in the ChipBase::regs_ array.

#define ER2055_DECL(REG, FLD, CMP) \
    REG(0x00, ADDR_LATCH, "Address Latch (6-bit)")                        \
    REG(0x01, DATA_IN,    "Data Input Latch")                             \
    REG(0x02, DATA_OUT,   "Data Output Latch")                            \
    REG(0x03, CONTROL,    "Control State")                                \
      FLD(CONTROL, CS1,     0:0, "Chip Select 1 (decoded)",  Flag, 0, 0)  \
      FLD(CONTROL, CS2,     1:1, "Chip Select 2",            Flag, 0, 0)  \
      FLD(CONTROL, C1,      2:2, "Control 1",                Flag, 0, 0)  \
      FLD(CONTROL, C2,      3:3, "Control 2",                Flag, 0, 0)

// --- Address constants ---
namespace er2055 { namespace reg {
ER2055_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace er2055::reg

DECL_EXTRACT(ER2055, ER2055_DECL)

// --- Bitfield accessors ---
namespace er2055 { namespace fld {
#define ER2055_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
ER2055_DECL(DECL_REG_NOP, ER2055_X_FLD_NS_, DECL_CMP_NOP)
#undef ER2055_X_FLD_NS_
} } // namespace er2055::fld

// ============================================================================
// ER2055 bus pin definitions
// ============================================================================
//
// The ER2055 tick() accepts a bus_state_t with address in [ADDR], data in
// [DATA], and control signals in these dedicated bit positions.  The system
// constructs this bus from its own I/O decode logic.
//
// Address: BUS_GET_ADDR bits [5:0] — 6-bit EAROM address
// Data:    BUS_GET_DATA / BUS_SET_DATA — 8-bit data in/out
// Control: custom bits below

namespace er2055 {
    inline constexpr int CS1_BIT = 55;  // /CS1 pin (active-high in bus = chip selected)
    inline constexpr int CS2_BIT = 56;  // CS2 pin (active-high = chip selected)
    inline constexpr int C1_BIT  = 57;  // C1 control mode
    inline constexpr int C2_BIT  = 58;  // C2 control mode
    inline constexpr int CK_BIT  = 59;  // CK clock (rising edge triggers operation)
} // namespace er2055


// ============================================================================
// ER2055 chip class
// ============================================================================

class ER2055 : public MemoryChipBase {
public:
    static constexpr int MEMORY_SIZE = 64;   // 64 bytes (512 bits)

    ER2055()
        : MemoryChipBase(
              ChipInfo{"ER2055", "General Instrument",
                       "ER2055 512-bit Electrically Alterable ROM (EAROM)"},
              MEMORY_SIZE,
              MemoryType::EPROM,
              nullptr,          // no system bus needed
              "EAROM",
              0) {
        init_regs(ER2055_NUM_REGS);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(ER2055_REG_INFO);
        register_debug_fields();
#endif
    }

    void reset() override {
        regs_.data[er2055::reg::ADDR_LATCH] = 0;
        regs_.data[er2055::reg::DATA_IN]    = 0;
        regs_.data[er2055::reg::DATA_OUT]   = 0;
        regs_.data[er2055::reg::CONTROL]    = 0;
        bus_snapshot_ = 0;
        // Preserve memory contents across reset — EAROM is non-volatile.
        // On first power-up the memory is zero-filled by MemoryChipBase.
    }

    // ── Primary interface — tick(bus_state_t) ───────────────────────────
    //
    // The system constructs a bus_state_t with:
    //   ADDR [5:0]   — 6-bit EAROM address (from CPU address bus)
    //   DATA [7:0]   — 8-bit data (CPU → EAROM on write, EAROM → CPU on read)
    //   er2055::CS1_BIT — chip select 1 (decoded active-high)
    //   er2055::CS2_BIT — chip select 2 (active-high)
    //   er2055::C1_BIT  — control pin 1
    //   er2055::C2_BIT  — control pin 2
    //   er2055::CK_BIT  — clock (rising edge triggers operation)
    //
    // Returns the bus with DATA set to the output latch contents.

    bus_state_t tick(bus_state_t bus) {
        namespace fld = er2055::fld;
        constexpr auto ADDR_LATCH = er2055::reg::ADDR_LATCH;
        constexpr auto DATA_IN    = er2055::reg::DATA_IN;
        constexpr auto DATA_OUT   = er2055::reg::DATA_OUT;
        constexpr auto CONTROL    = er2055::reg::CONTROL;

        // Latch address and data from bus
        regs_.data[ADDR_LATCH] = BUS_GET_ADDR(bus) & 0x3F;
        regs_.data[DATA_IN]    = BUS_GET_DATA(bus);

        // Read control pin states from bus
        bool cs1 = BUS_GET_BIT(bus, er2055::CS1_BIT);
        bool cs2 = BUS_GET_BIT(bus, er2055::CS2_BIT);
        bool c1  = BUS_GET_BIT(bus, er2055::C1_BIT);
        bool c2  = BUS_GET_BIT(bus, er2055::C2_BIT);
        bool ck  = BUS_GET_BIT(bus, er2055::CK_BIT);

        // Edge detection: compare current CK against bus_snapshot_
        bool ck_prev   = BUS_GET_BIT(bus_snapshot_, er2055::CK_BIT);
        bool selected  = cs1 && cs2;
        bool ck_rising = ck && !ck_prev;

        if (selected && ck_rising) {
            uint8_t addr = regs_.data[ADDR_LATCH];
            if (c1 && !c2) {
                // READ: latch memory contents to data output
                regs_.data[DATA_OUT] = data()[addr];
            } else if (!c1 && c2) {
                // WRITE: store data input into memory
                data()[addr] = regs_.data[DATA_IN];
            } else if (c1 && c2) {
                // ERASE: clear memory location
                data()[addr] = 0x00;
            }
            // c1=0, c2=0: standby — no operation
        }

        // Update control register with current pin states
        regs_.data[CONTROL] = (cs1 ? fld::CONTROL_CS1 : 0)
                            | (cs2 ? fld::CONTROL_CS2 : 0)
                            | (c1  ? fld::CONTROL_C1  : 0)
                            | (c2  ? fld::CONTROL_C2  : 0);

        // Return bus with data output latch on the data lines
        BUS_SET_DATA(bus, regs_.data[DATA_OUT]);
        bus_snapshot_ = bus;
        return bus;
    }

    /// Read the data output latch (convenience for system code that
    /// doesn't need a full bus transaction).
    uint8_t read_data() const { return regs_.data[er2055::reg::DATA_OUT]; }

    // ── Direct memory access (save state / bulk load) ───────────────────

    uint8_t  read_byte(uint8_t addr) const { return data()[addr & 0x3F]; }
    void     write_byte(uint8_t addr, uint8_t val) { data()[addr & 0x3F] = val; }

    // ── Pin layout (GUI) ────────────────────────────────────────────────
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .set_decl_entries(ER2055_DECL_ENTRIES.data(), ER2055_DECL_ENTRIES.size());
    }
#endif
};
