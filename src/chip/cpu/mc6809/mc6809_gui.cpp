/*
 * mc6809_gui.cpp — MC6809 CPU family chip layout and GUI rendering
 *
 * Hardware-accurate 40-pin DIP pinout based on the Motorola MC6809 datasheet.
 * The MC6809 and MC6809E share the same 40-pin DIP package but differ in
 * clock-related pins (MC6809 has EXTAL/XTAL, MC6809E has E/Q clock inputs).
 * The HD6309 is pin-compatible with the MC6809.
 *
 * Pinout reference: Motorola MC6809/MC6809E Datasheet (DS9846-R2)
 */

#include "chip/cpu/mc6809/mc6809.hpp"
#include "chip/cpu/mc6809/motorola_mc6809.hpp"
#include "chip/cpu/mc6809/motorola_mc6809e.hpp"
#include "chip/cpu/mc6809/hitachi_hd6309.hpp"

#include "core/chip_layout.hpp"
#ifdef CERMU_HAS_GUI
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#include <imgui.h>
#endif

using namespace mc6809;

// ============================================================================
// Debug field registration
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG

namespace mc6809 {

template <const MC6809Traits& Traits>
void mc6809_t<Traits>::register_debug_fields() {
    // Point the debug registry at the RegisterFile backing store + DECL table.
    // Zero-copy — no sync needed.
    this->debug_registry_.set_registers(regs_.debug_ptr(), regs_.debug_size(), MC6809_REG_INFO);
    this->debug_registry_.set_decl_entries(MC6809_DECL_ENTRIES.data(), MC6809_DECL_ENTRIES.size());
}

template void mc6809_t<MotorolaMC6809Traits>::register_debug_fields();
template void mc6809_t<MotorolaMC6809ETtraits>::register_debug_fields();
template void mc6809_t<HitachiHD6309Traits>::register_debug_fields();

} // namespace mc6809

#endif // CERMU_HAS_CHIP_DEBUG

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase virtual method implementations (template definitions)
// ============================================================================

namespace mc6809 {

template <const MC6809Traits& Traits>
ChipLayout* mc6809_t<Traits>::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        // Motorola MC6809 40-pin DIP pinout
        // MC6809 and MC6809E differ on pins 34-35 (clock-related)
        PIN_LR(layout,  1, VSS,      VCC,    40);
        PIN_LR(layout,  2, _NMI,     _HALT,  39);
        PIN_LR(layout,  3, _IRQ,     XTAL,   38);  // MC6809E: TSC
        PIN_LR(layout,  4, _FIRQ,    EXTAL,  37);  // MC6809E: BS
        PIN_LR(layout,  5, BS,       _RES,   36);  // MC6809E: BA
        PIN_LR(layout,  6, BA,       ENABLE, 35);  // MC6809E: E (input)
        PIN_LR(layout,  7, VCC,      Q_CLK,  34);   // MC6809E: Q (input)
        PIN_LR(layout,  8, A0,       D0,     33);
        PIN_LR(layout,  9, A1,       D1,     32);
        PIN_LR(layout, 10, A2,       D2,     31);
        PIN_LR(layout, 11, A3,       D3,     30);
        PIN_LR(layout, 12, A4,       D4,     29);
        PIN_LR(layout, 13, A5,       D5,     28);
        PIN_LR(layout, 14, A6,       D6,     27);
        PIN_LR(layout, 15, A7,       D7,     26);
        PIN_LR(layout, 16, A8,       RW,     25);
        PIN_LR(layout, 17, A9,       BUSY,   24);   // MC6809E: BUSY
        PIN_LR(layout, 18, A10,      AVMA,   23);   // MC6809E: AVMA
        PIN_LR(layout, 19, A11,      LIC,    22);
        PIN_LR(layout, 20, A12,      A15,    21);

        return layout;
    }();
    return &layout;
}

// Safe default bus state for GUI rendering when no live bus state is available.
static constexpr bus_state_t MC6809_GUI_DEFAULT_STATE =
    BUS_BIT(BUS_RW_BIT)         |
    BUS_BIT(BUS_RDY_BIT)        |
    BUS_BIT(BUS_RES_BIT)        |
    BUS_BIT(BUS_IRQ_BIT)        |
    BUS_BIT(BUS_NMI_BIT)        |
    BUS_BIT(MC6809_FIRQ_BIT)    |
    BUS_BIT(MC6809_HALT_BIT)    |
    BUS_BIT(MC6809_LIC_BIT);

template <const MC6809Traits& Traits>
std::vector<PinSignalState> mc6809_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    bus_state_t bus = this->bus_prev_;
    if (bus == 0) bus = MC6809_GUI_DEFAULT_STATE;

    auto ps = populate_pin_states_from_bus(layout, bus);

    // MC6809-specific control signals

    // BS (pin 5, idx 4)
    ps[4].signal_level    = BUS_GET_BIT(bus, MC6809_BS_BIT) != 0;
    ps[4].drive_direction = true;
    ps[4].signal_valid    = true;

    // BA (pin 6, idx 5)
    ps[5].signal_level    = BUS_GET_BIT(bus, MC6809_BA_BIT) != 0;
    ps[5].drive_direction = true;
    ps[5].signal_valid    = true;

    // R/W (pin 25, idx 24)
    ps[24].signal_level    = BUS_GET_BIT(bus, BUS_RW_BIT) != 0;
    ps[24].drive_direction = true;
    ps[24].signal_valid    = true;

    // BUSY (pin 24, idx 23)
    ps[23].signal_level    = BUS_GET_BIT(bus, MC6809_BUSY_BIT) != 0;
    ps[23].drive_direction = true;
    ps[23].signal_valid    = true;

    // AVMA (pin 23, idx 22)
    ps[22].signal_level    = BUS_GET_BIT(bus, MC6809_AVMA_BIT) != 0;
    ps[22].drive_direction = true;
    ps[22].signal_valid    = true;

    // LIC (pin 22, idx 21)
    ps[21].signal_level    = BUS_GET_BIT(bus, MC6809_LIC_BIT) != 0;
    ps[21].drive_direction = true;
    ps[21].signal_valid    = true;

    // E (pin 35, idx 34)
    ps[34].signal_level    = BUS_GET_BIT(bus, MC6809_E_BIT) != 0;
    ps[34].drive_direction = Traits.has_internal_clock();
    ps[34].signal_valid    = true;

    // Q (pin 34, idx 33)
    ps[33].signal_level    = BUS_GET_BIT(bus, MC6809_Q_BIT) != 0;
    ps[33].drive_direction = Traits.has_internal_clock();
    ps[33].signal_valid    = true;

    return ps;
}

template <const MC6809Traits& Traits>
const char* mc6809_t<Traits>::get_layout_chip_name() const {
    return Traits.chip_id;
}

// ============================================================================
// Explicit template instantiations for all MC6809 variants
// ============================================================================

template ChipLayout*                 mc6809_t<MotorolaMC6809Traits>::create_chip_layout() const;
template std::vector<PinSignalState> mc6809_t<MotorolaMC6809Traits>::get_layout_pin_states(ChipLayout&);
template const char*                 mc6809_t<MotorolaMC6809Traits>::get_layout_chip_name() const;

template ChipLayout*                 mc6809_t<MotorolaMC6809ETtraits>::create_chip_layout() const;
template std::vector<PinSignalState> mc6809_t<MotorolaMC6809ETtraits>::get_layout_pin_states(ChipLayout&);
template const char*                 mc6809_t<MotorolaMC6809ETtraits>::get_layout_chip_name() const;

template ChipLayout*                 mc6809_t<HitachiHD6309Traits>::create_chip_layout() const;
template std::vector<PinSignalState> mc6809_t<HitachiHD6309Traits>::get_layout_pin_states(ChipLayout&);
template const char*                 mc6809_t<HitachiHD6309Traits>::get_layout_chip_name() const;

} // namespace mc6809

#endif // CERMU_HAS_GUI
