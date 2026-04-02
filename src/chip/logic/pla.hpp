#pragma once
// ============================================================================
// pla.hpp — MOS 906114-01 Programmable Logic Array (PLA)
// ============================================================================
//
// Commodore PLA MOS 906114-01 REV3 8411
// 28-pin DIP, mask-programmed AND-OR array with 16 inputs, 8 outputs,
// and ~30 product terms implementing the C64 memory map decode logic.
//
// References:
//   https://www.c64-wiki.com/wiki/PLA_(C64_chip)
//   http://skoe.de/docs/c64-dissected/pla/c64_pla_dissected_a4ss.pdf
// ============================================================================

#include "chip/logic/logic_chip_base.hpp"
#include "core/chip_debug_registry.hpp"

#include <cstdint>

// ── Pin interface structures ────────────────────────────────────────────────
// Kept as named structs so system-level code can work with individual signals.
//
// PLA MOS 906114-01 DIP has 28 pins; Pinout :

// Input pins structure
struct pla_inputs_t {
    // Left hand side, top>down pins:
    // Not needed:      pin  1/FE : N.C.(FE) - Used for programming field-programmable parts and not connected internally for mask-programmable parts
    bool a13;        // pin  2/I7 : A13 - Address bus high bits
    bool a14;        // pin  3/I6 : A14 - Address bus high bits
    bool a15;        // pin  4/I5 : A15 - Address bus high bits
    bool n_va14;     // pin  5/I4 : #VA14 - VIC-II address bit 14 (inverted)
    bool n_charen;   // pin  6/I3 : #CHAREN - Character ROM enable
    bool n_hiram;    // pin  7/I2 : #HIRAM - High RAM enable  
    bool n_loram;    // pin  8/I1 : #LORAM - Low RAM enable
    bool n_cas;      // pin  9/I0 : #CAS - Column Address Strobe from VIC-II
    // Note : pin 10 to 14 are outputs (see below)

    // Right hand side, bottom>up pins:
    // Note : pin 15 to 18 are outputs (see below)
    // Not needed:      pin 19/CE : CE - Chip Enable
    bool va12;       // pin 20/I15: VA12 - VIC-II address bits
    bool va13;       // pin 21/I14: VA13 - VIC-II address bits
    bool n_game;     // pin 22/I13: #GAME - Game cartridge
    bool n_exrom;    // pin 23/I12: #EXROM - External ROM
    bool r_w;        // pin 24/I11: R/#W - Read/Write
    bool n_aec;      // pin 25/I10: #AEC - Address Enable Control
    bool ba;         // pin 26/I9 : BA - Bus Available
    bool a12;        // pin 27/I8 : A12 - Address bus high bits
    // Not needed:      pin 28/VCC : Plus5Volt - +5V
};

// Output pins structure
struct pla_outputs_t {
    // Left hand side, top>down pins:
    bool n_romh;     // pin 10/F7 : #ROMH - ROM High select
    bool n_roml;     // pin 11/F6 : #ROML - ROM Low select  
    bool n_io;       // pin 12/F5 : #I/O - I/O select
    bool n_grw;      // pin 13/F4 : GR/#W - Graphics Read/Write aka Color RAM write enable (Connected to #WE on the color RAM)
                     // DETAILED _GRW SIGNAL BEHAVIOR:
                     // - Active (low) when ALL conditions met: I/O enabled + Color RAM address range + CPU write + proper config
                     // - When inactive (high): Hardware blocks Color RAM writes at chip level
                     // - Prevents Color RAM corruption when Character ROM is mapped instead of I/O region
                     // - Critical for proper C64 memory banking and Color RAM protection
    // Not needed:      pin 14/VSS : GND - Ground

    // Right hand side, bottom>up pins:
    bool n_charrom;  // pin 15/F3 : #CHARROM - Character ROM select
    bool n_kernal;   // pin 16/F2 : #KERNAL - Kernal ROM select
    bool n_basic;    // pin 17/F1 : #BASIC - Basic ROM select
    bool n_casram;   // pin 18/F0 : #CASRAM - RAM CAS select
};

// ── DECL register map ───────────────────────────────────────────────────────
// Two virtual registers packing the 16 input and 8 output pin states for the
// debug registry.  Bit assignments follow the PLA's I/F numbering.

#define PLA906114_DECL(REG, FLD, CMP)                                          \
    REG(0x00, INPUTS_LO,  "Input signals I0..I7")                              \
    FLD(INPUTS_LO, N_CAS,    0:0, "#CAS",    Flag, 0, 0)                      \
    FLD(INPUTS_LO, N_LORAM,  1:1, "#LORAM",  Flag, 0, 0)                      \
    FLD(INPUTS_LO, N_HIRAM,  2:2, "#HIRAM",  Flag, 0, 0)                      \
    FLD(INPUTS_LO, N_CHAREN, 3:3, "#CHAREN", Flag, 0, 0)                      \
    FLD(INPUTS_LO, N_VA14,   4:4, "#VA14",   Flag, 0, 0)                      \
    FLD(INPUTS_LO, A15,      5:5, "A15",     Flag, 0, 0)                      \
    FLD(INPUTS_LO, A14,      6:6, "A14",     Flag, 0, 0)                      \
    FLD(INPUTS_LO, A13,      7:7, "A13",     Flag, 0, 0)                      \
    REG(0x01, INPUTS_HI,  "Input signals I8..I15")                             \
    FLD(INPUTS_HI, A12,      0:0, "A12",     Flag, 0, 0)                      \
    FLD(INPUTS_HI, BA,       1:1, "BA",      Flag, 0, 0)                      \
    FLD(INPUTS_HI, N_AEC,    2:2, "#AEC",    Flag, 0, 0)                      \
    FLD(INPUTS_HI, R_W,      3:3, "R/#W",    Flag, 0, 0)                      \
    FLD(INPUTS_HI, N_EXROM,  4:4, "#EXROM",  Flag, 0, 0)                      \
    FLD(INPUTS_HI, N_GAME,   5:5, "#GAME",   Flag, 0, 0)                      \
    FLD(INPUTS_HI, VA13,     6:6, "VA13",    Flag, 0, 0)                      \
    FLD(INPUTS_HI, VA12,     7:7, "VA12",    Flag, 0, 0)                      \
    REG(0x02, OUTPUTS,    "Output signals F0..F7")                             \
    FLD(OUTPUTS, N_CASRAM,  0:0, "#CASRAM",  Flag, 0, 0)                      \
    FLD(OUTPUTS, N_BASIC,   1:1, "#BASIC",   Flag, 0, 0)                      \
    FLD(OUTPUTS, N_KERNAL,  2:2, "#KERNAL",  Flag, 0, 0)                      \
    FLD(OUTPUTS, N_CHARROM, 3:3, "#CHARROM", Flag, 0, 0)                      \
    FLD(OUTPUTS, N_GRW,     4:4, "GR/#W",    Flag, 0, 0)                      \
    FLD(OUTPUTS, N_IO,      5:5, "#I/O",     Flag, 0, 0)                      \
    FLD(OUTPUTS, N_ROML,    6:6, "#ROML",    Flag, 0, 0)                      \
    FLD(OUTPUTS, N_ROMH,    7:7, "#ROMH",    Flag, 0, 0)

namespace pla906114 { namespace reg {
PLA906114_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace pla906114::reg

DECL_EXTRACT(PLA906114, PLA906114_DECL)

// ============================================================================
// PLA906114 — MOS 906114-01 Programmable Logic Array
// ============================================================================

class PLA906114 : public LogicChipBase {
public:
    PLA906114()
        : LogicChipBase(ChipInfo{"906114-01", "MOS Technology",
                                 "MOS 906114-01 PLA"}) {
        init_regs(PLA906114_NUM_REGS);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(PLA906114_REG_INFO);
        register_debug_fields();
#endif
    }

    // ── ChipBase overrides ──────────────────────────────────────────────

    void reset() override {
        inputs_  = {};
        outputs_ = {};
        // Default input states (typical C64 boot state)
        inputs_.n_charen = true;    // Character ROM disabled initially
        inputs_.n_hiram  = true;    // High RAM enabled
        inputs_.n_loram  = true;    // Low RAM enabled
        inputs_.n_cas    = true;    // No CAS initially
        inputs_.n_va14   = true;    // VA14 high
        inputs_.n_aec    = false;   // CPU has bus control
        inputs_.ba       = true;    // Bus available
        inputs_.r_w      = true;    // Read mode
        inputs_.n_exrom  = true;    // No external ROM
        inputs_.n_game   = true;    // No game cartridge
        inputs_.va13     = false;
        inputs_.va12     = false;
        update_outputs();
    }

    // ── Core PLA interface ──────────────────────────────────────────────

    /// Evaluate all product terms and update output signals.
    void update_outputs();

    /// Tick from bus state — extracts A12-A15, R/W, AEC, BA and evaluates.
    /// Banking inputs (LORAM, HIRAM, CHAREN, EXROM, GAME) and VIC-specific
    /// inputs (VA12, VA13, VA14, CAS) must be set before calling tick.
    void tick(bus_state_t bus_state);

    /// Set A12-A15 from the high nybble of an address and evaluate.
    void set_cpu_address_bank(uint8_t addr_high);

    /// Set VA12-VA14 from the high nybble of a VIC-II address and evaluate.
    void set_vicii_address_bank(uint8_t va_high);

    /// Set the 5 banking input signals from a mode byte.
    /// Bits: 0=LORAM, 1=HIRAM, 2=CHAREN, 3=EXROM, 4=GAME.
    /// Uses positive logic: bit set = feature enabled = PLA variable true.
    void set_banking_mode(uint8_t mode);

    /// Check for Ultimax mode (#GAME = 0, #EXROM = 1).
    bool is_ultimax_mode() const {
        return inputs_.n_exrom && !inputs_.n_game;
    }

    // ── Pin access ──────────────────────────────────────────────────────

    pla_inputs_t&        inputs()        { return inputs_; }
    const pla_inputs_t&  inputs()  const { return inputs_; }
    pla_outputs_t&       outputs()       { return outputs_; }
    const pla_outputs_t& outputs() const { return outputs_; }

    // ── System integration ──────────────────────────────────────────────
    // Opaque callbacks for system-specific GUI rendering (e.g. C64 banking
    // tables).  Set by the owning system; left null for standalone use.

    using RenderCallback = void(*)(void* ctx, PLA906114& pla);

    void set_system_context(void* ctx,
                            RenderCallback debug_render = nullptr,
                            RenderCallback settings_render = nullptr) {
        system_ctx_       = ctx;
        debug_render_     = debug_render;
        settings_render_  = settings_render;
    }

    void* system_context() const { return system_ctx_; }

    // Layout virtuals — defined in pla_gui.cpp (GUI builds only)
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;

    bool has_debug_content() const override   { return debug_render_ != nullptr; }
    void render_debug_content() override      { if (debug_render_)    debug_render_(system_ctx_, *this); }
    bool has_settings_content() const override { return settings_render_ != nullptr; }
    void render_settings_content() override   { if (settings_render_) settings_render_(system_ctx_, *this); }
#endif

private:
    pla_inputs_t  inputs_{};
    pla_outputs_t outputs_{};

    void* system_ctx_ = nullptr;
    RenderCallback debug_render_    = nullptr;
    RenderCallback settings_render_ = nullptr;
    /// Pack input bools into regs_ for the debug registry.
    void sync_regs() {
        regs_[pla906114::reg::INPUTS_LO] =
            (uint8_t(inputs_.n_cas)    << 0) |
            (uint8_t(inputs_.n_loram)  << 1) |
            (uint8_t(inputs_.n_hiram)  << 2) |
            (uint8_t(inputs_.n_charen) << 3) |
            (uint8_t(inputs_.n_va14)   << 4) |
            (uint8_t(inputs_.a15)      << 5) |
            (uint8_t(inputs_.a14)      << 6) |
            (uint8_t(inputs_.a13)      << 7);
        regs_[pla906114::reg::INPUTS_HI] =
            (uint8_t(inputs_.a12)      << 0) |
            (uint8_t(inputs_.ba)       << 1) |
            (uint8_t(inputs_.n_aec)    << 2) |
            (uint8_t(inputs_.r_w)      << 3) |
            (uint8_t(inputs_.n_exrom)  << 4) |
            (uint8_t(inputs_.n_game)   << 5) |
            (uint8_t(inputs_.va13)     << 6) |
            (uint8_t(inputs_.va12)     << 7);
        regs_[pla906114::reg::OUTPUTS] =
            (uint8_t(outputs_.n_casram)  << 0) |
            (uint8_t(outputs_.n_basic)   << 1) |
            (uint8_t(outputs_.n_kernal)  << 2) |
            (uint8_t(outputs_.n_charrom) << 3) |
            (uint8_t(outputs_.n_grw)     << 4) |
            (uint8_t(outputs_.n_io)      << 5) |
            (uint8_t(outputs_.n_roml)    << 6) |
            (uint8_t(outputs_.n_romh)    << 7);
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        // All input/output flag fields are rendered by the DECL walk.
        debug_registry_
            .set_decl_entries(PLA906114_DECL_ENTRIES.data(), PLA906114_DECL_ENTRIES.size());
    }
#endif
};
