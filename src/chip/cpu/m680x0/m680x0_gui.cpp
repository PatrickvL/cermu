/*
 * m680x0_gui.cpp — MC68000 family chip layout and GUI rendering
 *
 * Hardware-accurate 64-pin DIP pinout based on the Motorola MC68000 datasheet.
 * Pin numbering follows Motorola's MC68000 User's Manual.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/cpu/m680x0/m680x0.hpp"
#include "chip/cpu/m680x0/mc68000.hpp"
#include "chip/cpu/m680x0/mc68010.hpp"
#include "chip/cpu/m680x0/mc68020.hpp"

#include "core/chip_layout.hpp"
#ifdef CERMU_HAS_GUI
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#include <imgui.h>
#endif

namespace m680x0 {

// ════════════════════════════════════════════════════════════════════
// Debug field registration
// ════════════════════════════════════════════════════════════════════

#ifdef CERMU_HAS_CHIP_DEBUG

template <const M680x0Traits& Traits>
void m680x0_t<Traits>::register_debug_fields() {
    // Point the debug registry at the RegisterFile backing store + DECL table.
    // All register values are data-driven — no per-register callbacks.
    this->debug_registry_.set_registers(regs_.debug_ptr(), regs_.debug_size(), M68K_REG_INFO);
    this->debug_registry_.set_decl_entries(M68K_DECL_ENTRIES.data(), M68K_DECL_ENTRIES.size());
}

template void m680x0_t<MC68000Traits>::register_debug_fields();
template void m680x0_t<MC68010Traits>::register_debug_fields();
template void m680x0_t<MC68020Traits>::register_debug_fields();

#endif // CERMU_HAS_CHIP_DEBUG

#ifdef CERMU_HAS_GUI

// ════════════════════════════════════════════════════════════════════
// Chip Layout — MC68000 64-pin DIP
// ════════════════════════════════════════════════════════════════════
//
// Pin numbering follows Motorola MC68000 data sheet.
//
// Left side (pins 1-32):
//   1: D4,  2: D3,  3: D2,  4: D1,  5: D0,  6: /AS,  7: /UDS,  8: /LDS,
//   9: R/W, 10: /DTACK, 11: /BG, 12: /BGACK, 13: /BR, 14: VCC,
//   15: CLK, 16: GND, 17: /HALT, 18: /RESET, 19: /VMA, 20: E,
//   21: /VPA, 22: /BERR, 23: /IPL2, 24: /IPL1, 25: /IPL0,
//   26: FC2, 27: FC1, 28: FC0, 29: A1, 30: A2, 31: A3, 32: A4
//
// Right side (pins 64-33, top to bottom):
//   64: D5, 63: D6, 62: D7, 61: D8, 60: D9, 59: D10, 58: D11,
//   57: D12, 56: D13, 55: D14, 54: D15, 53: GND, 52: A23, 51: A22,
//   50: A21, 49: A20, 48: A19, 47: A18, 46: A17, 45: A16, 44: A15,
//   43: A14, 42: A13, 41: A12, 40: A11, 39: A10, 38: A9, 37: A8,
//   36: A7, 35: A6, 34: A5, 33: VCC

template <const M680x0Traits& Traits>
ChipLayout* m680x0_t<Traits>::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_custom_dip(64);
        layout.left_pins.clear();
        layout.right_pins.clear();

        // MC68000 64-pin DIP pinout
        // Left side: pins 1-32 (top to bottom)
        // Right side: pins 64-33 (top to bottom)
        PIN_LR(layout,  1, D4,      D5,     64);
        PIN_LR(layout,  2, D3,      D6,     63);
        PIN_LR(layout,  3, D2,      D7,     62);
        PIN_LR(layout,  4, D1,      D8,     61);
        PIN_LR(layout,  5, D0,      D9,     60);
        PIN_LR(layout,  6, _AS_M68K, D10,   59);
        PIN_LR(layout,  7, _UDS,    D11,    58);
        PIN_LR(layout,  8, _LDS,    D12,    57);
        PIN_LR(layout,  9, RW,      D13,    56);
        PIN_LR(layout, 10, _DTACK,  D14,    55);
        PIN_LR(layout, 11, _BG,     D15,    54);
        PIN_LR(layout, 12, _BGACK,  GND,    53);
        PIN_LR(layout, 13, _BR,     A23,    52);
        PIN_LR(layout, 14, VCC,     A22,    51);
        PIN_LR(layout, 15, CLK,     A21,    50);
        PIN_LR(layout, 16, GND,     A20,    49);
        PIN_LR(layout, 17, _HALT,   A19,    48);
        PIN_LR(layout, 18, _RES,    A18,    47);
        PIN_LR(layout, 19, _VMA,    A17,    46);
        PIN_LR(layout, 20, E_CLK,   A16,    45);
        PIN_LR(layout, 21, _VPA_M68K, A15,  44);
        PIN_LR(layout, 22, _BERR,   A14,    43);
        PIN_LR(layout, 23, _IPL2,   A13,    42);
        PIN_LR(layout, 24, _IPL1,   A12,    41);
        PIN_LR(layout, 25, _IPL0,   A11,    40);
        PIN_LR(layout, 26, FC2,     A10,    39);
        PIN_LR(layout, 27, FC1,     A9,     38);
        PIN_LR(layout, 28, FC0,     A8,     37);
        PIN_LR(layout, 29, A1,      A7,     36);
        PIN_LR(layout, 30, A2,      A6,     35);
        PIN_LR(layout, 31, A3,      A5,     34);
        PIN_LR(layout, 32, A4,      VCC,    33);

        return layout;
    }();
    return &layout;
}

// Default bus state for GUI when no live state is available.
// All active-low control signals deasserted (high).
static constexpr bus_state_t M68K_GUI_DEFAULT_STATE =
    BUS_BIT(BUS_RW_BIT)      |   // R/W high (read)
    BUS_BIT(BUS_RES_BIT)     |   // RESET deasserted
    BUS_BIT(M68K_AS_BIT)     |   // AS deasserted
    BUS_BIT(M68K_LDS_BIT)    |   // LDS deasserted
    BUS_BIT(M68K_UDS_BIT)    |   // UDS deasserted
    BUS_BIT(M68K_DTACK_BIT)  |   // DTACK deasserted
    BUS_BIT(M68K_BR_BIT)     |   // BR deasserted
    BUS_BIT(M68K_BGACK_BIT)  |   // BGACK deasserted
    BUS_BIT(M68K_VPA_BIT)    |   // VPA deasserted
    BUS_BIT(M68K_BERR_BIT)   |   // BERR deasserted
    BUS_BIT(M68K_HALT_BIT)   |   // HALT deasserted
    BUS_BIT(M68K_IPL0_BIT)   |   // IPL0 deasserted (no interrupt)
    BUS_BIT(M68K_IPL1_BIT)   |   // IPL1 deasserted
    BUS_BIT(M68K_IPL2_BIT);      // IPL2 deasserted

template <const M680x0Traits& Traits>
std::vector<PinSignalState> m680x0_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    bus_state_t bus = this->bus_prev_;
    if (bus == 0) bus = M68K_GUI_DEFAULT_STATE;

    auto ps = populate_pin_states_from_bus(layout, bus);

    // M68K-specific signal overlays from bus bits.
    // Left pins: indices 0..31, Right pins: indices 32..63
    // Index = pin_number - 1 for left, index = 64 - pin_number + 32 for right

    // /AS (pin 6, left idx 5)
    ps[5].signal_level    = BUS_GET_BIT(bus, M68K_AS_BIT) != 0;
    ps[5].drive_direction = true;
    ps[5].signal_valid    = true;

    // /UDS (pin 7, left idx 6)
    ps[6].signal_level    = BUS_GET_BIT(bus, M68K_UDS_BIT) != 0;
    ps[6].drive_direction = true;
    ps[6].signal_valid    = true;

    // /LDS (pin 8, left idx 7)
    ps[7].signal_level    = BUS_GET_BIT(bus, M68K_LDS_BIT) != 0;
    ps[7].drive_direction = true;
    ps[7].signal_valid    = true;

    // /DTACK (pin 10, left idx 9) — input
    ps[9].signal_level    = BUS_GET_BIT(bus, M68K_DTACK_BIT) != 0;
    ps[9].drive_direction = false;
    ps[9].signal_valid    = true;

    // /BG (pin 11, left idx 10)
    ps[10].signal_level    = BUS_GET_BIT(bus, M68K_BG_BIT) != 0;
    ps[10].drive_direction = true;
    ps[10].signal_valid    = true;

    // /BGACK (pin 12, left idx 11) — input
    ps[11].signal_level    = BUS_GET_BIT(bus, M68K_BGACK_BIT) != 0;
    ps[11].drive_direction = false;
    ps[11].signal_valid    = true;

    // /BR (pin 13, left idx 12) — input
    ps[12].signal_level    = BUS_GET_BIT(bus, M68K_BR_BIT) != 0;
    ps[12].drive_direction = false;
    ps[12].signal_valid    = true;

    // /HALT (pin 17, left idx 16) — bidirectional
    ps[16].signal_level    = BUS_GET_BIT(bus, M68K_HALT_BIT) != 0;
    ps[16].drive_direction = true;
    ps[16].signal_valid    = true;

    // /VMA (pin 19, left idx 18)
    ps[18].signal_level    = BUS_GET_BIT(bus, M68K_VMA_BIT) != 0;
    ps[18].drive_direction = true;
    ps[18].signal_valid    = true;

    // E (pin 20, left idx 19) — clock output
    ps[19].signal_level    = BUS_GET_BIT(bus, M68K_E_BIT) != 0;
    ps[19].drive_direction = true;
    ps[19].signal_valid    = true;

    // /VPA (pin 21, left idx 20) — input
    ps[20].signal_level    = BUS_GET_BIT(bus, M68K_VPA_BIT) != 0;
    ps[20].drive_direction = false;
    ps[20].signal_valid    = true;

    // /BERR (pin 22, left idx 21) — input
    ps[21].signal_level    = BUS_GET_BIT(bus, M68K_BERR_BIT) != 0;
    ps[21].drive_direction = false;
    ps[21].signal_valid    = true;

    // /IPL2 (pin 23, left idx 22) — input
    ps[22].signal_level    = BUS_GET_BIT(bus, M68K_IPL2_BIT) != 0;
    ps[22].drive_direction = false;
    ps[22].signal_valid    = true;

    // /IPL1 (pin 24, left idx 23) — input
    ps[23].signal_level    = BUS_GET_BIT(bus, M68K_IPL1_BIT) != 0;
    ps[23].drive_direction = false;
    ps[23].signal_valid    = true;

    // /IPL0 (pin 25, left idx 24) — input
    ps[24].signal_level    = BUS_GET_BIT(bus, M68K_IPL0_BIT) != 0;
    ps[24].drive_direction = false;
    ps[24].signal_valid    = true;

    // FC2 (pin 26, left idx 25)
    ps[25].signal_level    = BUS_GET_BIT(bus, M68K_FC2_BIT) != 0;
    ps[25].drive_direction = true;
    ps[25].signal_valid    = true;

    // FC1 (pin 27, left idx 26)
    ps[26].signal_level    = BUS_GET_BIT(bus, M68K_FC1_BIT) != 0;
    ps[26].drive_direction = true;
    ps[26].signal_valid    = true;

    // FC0 (pin 28, left idx 27)
    ps[27].signal_level    = BUS_GET_BIT(bus, M68K_FC0_BIT) != 0;
    ps[27].drive_direction = true;
    ps[27].signal_valid    = true;

    return ps;
}

template <const M680x0Traits& Traits>
const char* m680x0_t<Traits>::get_layout_chip_name() const {
    return Traits.chip_id;
}

template <const M680x0Traits& Traits>
void m680x0_t<Traits>::render_debug_content() {
    // DECL-based register/field walk + builder-API categories
    debug_registry_.render(this);
}

// ════════════════════════════════════════════════════════════════════
// Explicit template instantiations
// ════════════════════════════════════════════════════════════════════

template ChipLayout*                 m680x0_t<MC68000Traits>::create_chip_layout() const;
template std::vector<PinSignalState> m680x0_t<MC68000Traits>::get_layout_pin_states(ChipLayout&);
template const char*                 m680x0_t<MC68000Traits>::get_layout_chip_name() const;
template void                        m680x0_t<MC68000Traits>::render_debug_content();

template ChipLayout*                 m680x0_t<MC68010Traits>::create_chip_layout() const;
template std::vector<PinSignalState> m680x0_t<MC68010Traits>::get_layout_pin_states(ChipLayout&);
template const char*                 m680x0_t<MC68010Traits>::get_layout_chip_name() const;
template void                        m680x0_t<MC68010Traits>::render_debug_content();

template ChipLayout*                 m680x0_t<MC68020Traits>::create_chip_layout() const;
template std::vector<PinSignalState> m680x0_t<MC68020Traits>::get_layout_pin_states(ChipLayout&);
template const char*                 m680x0_t<MC68020Traits>::get_layout_chip_name() const;
template void                        m680x0_t<MC68020Traits>::render_debug_content();

#endif // CERMU_HAS_GUI

} // namespace m680x0
