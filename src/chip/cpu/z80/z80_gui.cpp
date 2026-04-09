/*
 * z80_gui.cpp — Z80 CPU family chip layout and GUI rendering
 *
 * Hardware-accurate 40-pin DIP pinout based on the Zilog Z80 CPU datasheet.
 * All Z80 variants (Z80, Z80A, Z80B, U880) share the same 40-pin DIP
 * package and pinout, differing only in rated clock speed and vendor.
 *
 * Pinout reference: Zilog Z80 CPU User Manual (UM0080), Rev. 11
 */

#include "chip/cpu/z80/z80.hpp"
#include "chip/cpu/z80/zilog_z80.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/cpu/z80/zilog_z80b.hpp"
#include "chip/cpu/z80/u880.hpp"
#include "chip/cpu/z80/sharp_sm83.hpp"

#include "core/chip_layout.hpp"
#ifdef CERMU_HAS_GUI
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#include <imgui.h>
#endif

using namespace z80;

// ============================================================================
// Debug registration (DECL-driven)
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG

template <const Z80Traits& Traits>
void z80_t<Traits>::register_debug_fields() {
    this->debug_registry_.set_registers(regs_.debug_ptr(), regs_.debug_size(), Z80_REG_INFO);
    this->debug_registry_.set_decl_entries(Z80_DECL_ENTRIES.data(), Z80_DECL_ENTRIES.size());
}

template void z80_t<ZilogZ80Traits>::register_debug_fields();
template void z80_t<ZilogZ80ATraits>::register_debug_fields();
template void z80_t<ZilogZ80BTraits>::register_debug_fields();
template void z80_t<U880Traits>::register_debug_fields();
template void z80_t<SharpSM83Traits>::register_debug_fields();

#endif // CERMU_HAS_CHIP_DEBUG

#ifdef CERMU_HAS_GUI

template <const Z80Traits& Traits>
void z80_t<Traits>::render_debug_content() {
    debug_registry_.render(this);
}

// ============================================================================
// ChipBase virtual method implementations (template definitions)
// ============================================================================

namespace z80 {

template <const Z80Traits& Traits>
ChipLayout* z80_t<Traits>::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        // Zilog Z80 40-pin DIP pinout (Z80 CPU User Manual)
        PIN_LR(layout,  1, A11,      A10,    40);
        PIN_LR(layout,  2, A12,      A9,     39);
        PIN_LR(layout,  3, A13,      A8,     38);
        PIN_LR(layout,  4, A14,      A7,     37);
        PIN_LR(layout,  5, A15,      A6,     36);
        PIN_LR(layout,  6, CLK,      A5,     35);
        PIN_LR(layout,  7, D4,       A4,     34);
        PIN_LR(layout,  8, D3,       A3,     33);
        PIN_LR(layout,  9, D5,       A2,     32);
        PIN_LR(layout, 10, D6,       A1,     31);
        PIN_LR(layout, 11, VCC,      A0,     30);
        PIN_LR(layout, 12, D2,       GND,    29); // 29 : Ground (GND)
        PIN_LR(layout, 13, D7,       _RFSH,  28);
        PIN_LR(layout, 14, D0,       _M1,    27);
        PIN_LR(layout, 15, D1,       _RES,   26); // 26 : Active-low RESET (asserted when low)
        PIN_LR(layout, 16, _INT,     _BUSRQ, 25); 
        PIN_LR(layout, 17, _NMI,     _WAIT,  24);
        PIN_LR(layout, 18, _HALT,    _BUSAK, 23);
        PIN_LR(layout, 19, _MREQ,    _WR,    22);
        PIN_LR(layout, 20, _IORQ,    _RD,    21);

        return layout;
    }();
    return &layout;
}

// Safe default bus state for GUI rendering when no live bus state is available.
static constexpr bus_state_t Z80_GUI_DEFAULT_STATE =
    BUS_BIT(BUS_RW_BIT)       |
    BUS_BIT(BUS_RDY_BIT)      |
    BUS_BIT(BUS_RES_BIT)      |
    BUS_BIT(BUS_IRQ_BIT)      |
    BUS_BIT(BUS_NMI_BIT)      |
    BUS_BIT(Z80_MREQ_BIT)     |
    BUS_BIT(Z80_IORQ_BIT)     |
    BUS_BIT(Z80_M1_BIT)       |
    BUS_BIT(Z80_RFSH_BIT)     |
    BUS_BIT(Z80_HALT_BIT)     |
    BUS_BIT(Z80_WAIT_BIT)     |
    BUS_BIT(Z80_BUSACK_BIT);

template <const Z80Traits& Traits>
std::vector<PinSignalState> z80_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    bus_state_t bus = this->bus_prev_;
    if (bus == 0) bus = Z80_GUI_DEFAULT_STATE;

    return build_pin_states(layout, bus, [&](auto& ps) {
        // Z80-specific control signals — overlay from bus bits
        // /MREQ (pin 19, idx 18)
        ps[18].signal_level    = BUS_GET_BIT(bus, Z80_MREQ_BIT) != 0;
        ps[18].drive_direction = true;
        ps[18].signal_valid    = true;

        // /IORQ (pin 20, idx 19)
        ps[19].signal_level    = BUS_GET_BIT(bus, Z80_IORQ_BIT) != 0;
        ps[19].drive_direction = true;
        ps[19].signal_valid    = true;

        // /RD (pin 21, idx 20)
        ps[20].signal_level    = BUS_GET_BIT(bus, BUS_RW_BIT) != 0;  // RW=1 means read idle
        ps[20].drive_direction = true;
        ps[20].signal_valid    = true;

        // /WR (pin 22, idx 21)
        ps[21].signal_level    = BUS_GET_BIT(bus, BUS_RW_BIT) != 0;  // Inverted: active when RW=0
        ps[21].drive_direction = true;
        ps[21].signal_valid    = true;

        // /BUSAK (pin 23, idx 22)
        ps[22].signal_level    = BUS_GET_BIT(bus, Z80_BUSACK_BIT) != 0;
        ps[22].drive_direction = true;
        ps[22].signal_valid    = true;

        // /M1 (pin 27, idx 26)
        ps[26].signal_level    = BUS_GET_BIT(bus, Z80_M1_BIT) != 0;
        ps[26].drive_direction = true;
        ps[26].signal_valid    = true;

        // /RFSH (pin 28, idx 27)
        ps[27].signal_level    = BUS_GET_BIT(bus, Z80_RFSH_BIT) != 0;
        ps[27].drive_direction = true;
        ps[27].signal_valid    = true;

        // /HALT (pin 18, idx 17)
        ps[17].signal_level    = BUS_GET_BIT(bus, Z80_HALT_BIT) != 0;
        ps[17].drive_direction = true;
        ps[17].signal_valid    = true;
    });
}

template <const Z80Traits& Traits>
const char* z80_t<Traits>::get_layout_chip_name() const {
    return Traits.chip_id;
}

// ============================================================================
// Explicit template instantiations for all Z80 variants
// ============================================================================

template ChipLayout*               z80_t<ZilogZ80Traits>::create_chip_layout() const;
template std::vector<PinSignalState> z80_t<ZilogZ80Traits>::get_layout_pin_states(ChipLayout&);
template const char*                z80_t<ZilogZ80Traits>::get_layout_chip_name() const;
template void                       z80_t<ZilogZ80Traits>::render_debug_content();

template ChipLayout*               z80_t<ZilogZ80ATraits>::create_chip_layout() const;
template std::vector<PinSignalState> z80_t<ZilogZ80ATraits>::get_layout_pin_states(ChipLayout&);
template const char*                z80_t<ZilogZ80ATraits>::get_layout_chip_name() const;
template void                       z80_t<ZilogZ80ATraits>::render_debug_content();

template ChipLayout*               z80_t<ZilogZ80BTraits>::create_chip_layout() const;
template std::vector<PinSignalState> z80_t<ZilogZ80BTraits>::get_layout_pin_states(ChipLayout&);
template const char*                z80_t<ZilogZ80BTraits>::get_layout_chip_name() const;
template void                       z80_t<ZilogZ80BTraits>::render_debug_content();

template ChipLayout*               z80_t<U880Traits>::create_chip_layout() const;
template std::vector<PinSignalState> z80_t<U880Traits>::get_layout_pin_states(ChipLayout&);
template const char*                z80_t<U880Traits>::get_layout_chip_name() const;
template void                       z80_t<U880Traits>::render_debug_content();

template ChipLayout*               z80_t<SharpSM83Traits>::create_chip_layout() const;
template std::vector<PinSignalState> z80_t<SharpSM83Traits>::get_layout_pin_states(ChipLayout&);
template const char*                z80_t<SharpSM83Traits>::get_layout_chip_name() const;
template void                       z80_t<SharpSM83Traits>::render_debug_content();

} // namespace z80

#endif // CERMU_HAS_GUI
