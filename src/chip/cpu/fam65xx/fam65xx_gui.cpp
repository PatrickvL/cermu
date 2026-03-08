/*
 * fam65xx_gui.cpp - C++ GUI rendering functions for the FAM65XX CPU family
 *
 * This file contains modern C++ GUI rendering that works directly with the
 * template-based CPU implementation and adapts to processor traits for
 * variant-specific features. Uses the generic chip visualization system.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

// Include the modern fam65xx implementation
#include "fam65xx.hpp"
#include "fam65xx_decoder.h"
#include "fam65xx_types.h"

// Per-CPU headers for explicit template instantiations below
#include "mos6502.h"
#include "mos6510.h"
#include "mos7501.h"
#include "ricoh_2a03.h"
#include "mos6507.h"
#include "wdc65c02.h"
#include "synertek65c02.h"
#include "rockwell65c02.h"
#include "wdc_w65c02s.h"
#include "wdc65c816.h"

// Include GUI interface first (defines CERMU_HAS_GUI)
#include "../../../core/chip_layout.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#include <imgui.h>
#endif
#include "fam65xx_pin_states.h"

using namespace fam65xx;

// ============================================================================
// CPU TRAIT-BASED HELPER FUNCTIONS
// ============================================================================

// Template function to get processor name based on traits
template <const CPUTraits &Traits> const char *get_processor_name() {
  // Use a thread-local static buffer to avoid conflicts between template
  // instantiations
  static thread_local char processor_name_buffer[64];
  snprintf(processor_name_buffer, sizeof(processor_name_buffer), "%s %s",
           Traits.get_vendor(), Traits.get_chip_id());
  return processor_name_buffer;
}

#ifdef CERMU_HAS_GUI

// Safe default control-signal state for GUI rendering when no live bus state is available.
static constexpr bus_state_t FAM65XX_GUI_DEFAULT_STATE =
    BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_RES_BIT) |
    BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_NMI_BIT);


template <const CPUTraits &Traits>
void render_processor_features(fam65xx_t<Traits> *cpu) {
  if (ImGui::CollapsingHeader("Processor Features")) {
    ImGui::Indent(16.0f);

    ImGui::Text("Processor: %s", get_processor_name<Traits>());
    ImGui::Separator();

    // Show trait-based features
    ImGui::Text("Core Features:");
    ImGui::Indent(16.0f);

    if constexpr (Traits.has(CPUCoreFlags::HAS_DECIMAL_MODE)) {
      ImGui::Text("✓ Decimal Mode (BCD)");
    } else {
      ImGui::Text("✗ Decimal Mode (BCD)");
    }

    if constexpr (Traits.has(CPUCoreFlags::ILLEGAL_OPCODES)) {
      ImGui::Text("✓ Illegal Opcodes");
    } else {
      ImGui::Text("✗ Illegal Opcodes");
    }

    if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
      ImGui::Text("✓ CMOS Design");
    } else {
      ImGui::Text("✗ NMOS Design");
    }

    if constexpr (Traits.has(CPUCoreFlags::C816_16BIT)) {
      ImGui::Text("✓ 16-bit Extensions");
    } else {
      ImGui::Text("✗ 8-bit Only");
    }

    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text("Hardware Specifications:");
    ImGui::Indent(16.0f);
    ImGui::Text("Vendor: %s", Traits.get_vendor());
    ImGui::Text("Chip ID: %s", Traits.get_chip_id());
    ImGui::Text("Address Bits: %u", Traits.address_bits);

    uint32_t address_space = 1u << Traits.address_bits;
    if (address_space >= 1024 * 1024) {
      ImGui::Text("Address Space: %uMB", address_space / (1024 * 1024));
    } else if (address_space >= 1024) {
      ImGui::Text("Address Space: %uKB", address_space / 1024);
    } else {
      ImGui::Text("Address Space: %u bytes", address_space);
    }

    if constexpr (Traits.has_io_port()) {
      // Count bits set in io_port_mask (cross-platform)
      uint8_t mask = Traits.io_port_mask;
      int bit_count = 0;
      while (mask) {
        bit_count += mask & 1;
        mask >>= 1;
      }
      ImGui::Text("I/O Port Pins: %u available", bit_count);
    } else {
      ImGui::Text("I/O Port: None");
    }
    ImGui::Unindent(16.0f);

    // Processor-specific features
    if constexpr (Traits.has_io_port()) {
      ImGui::Separator();
      ImGui::Text("I/O Port Features:");
      ImGui::Indent(16.0f);
      ImGui::Text("✓ Memory-mapped I/O at $00/$01");
      ImGui::Text("✓ Bank switching support");
      ImGui::Unindent(16.0f);
    }

    if constexpr (Traits.has(CPUCoreFlags::ROCKWELL_BITS)) {
      ImGui::Separator();
      ImGui::Text("Rockwell Extensions:");
      ImGui::Indent(16.0f);
      ImGui::Text("✓ Bit manipulation (RMB/SMB/BBR/BBS)");
      ImGui::Unindent(16.0f);
    }

    ImGui::Unindent(16.0f);
  }
}

// ============================================================================
// ChipBase VIRTUAL METHOD IMPLEMENTATIONS (template definitions)
// Must be inside namespace fam65xx for proper linkage of explicit instantiations
// ============================================================================

namespace fam65xx {

template <const CPUTraits &Traits>
void fam65xx_t<Traits>::render_settings_content() {
    ImGui::Text("%s %s Configuration", Traits.get_vendor(), Traits.get_chip_id());
    ImGui::Separator();

    ImGui::Text("Processor Family: MOS Technology 65xx");
    ImGui::Text("Architecture: 8-bit microprocessor");

    // Calculate address space from CPUTraits
    uint32_t address_space_kb = (1u << Traits.address_bits) / 1024;
    if (address_space_kb >= 1024) {
      ImGui::Text("Address Space: %uMB (%u-bit addressing)",
                  address_space_kb / 1024, Traits.address_bits);
    } else {
      ImGui::Text("Address Space: %uKB (%u-bit addressing)", address_space_kb,
                  Traits.address_bits);
    }

    ImGui::Text("Data Width: 8 bits");

    ImGui::Separator();

    // Show processor-specific configuration options
    render_processor_features<Traits>(this);
}

template <const CPUTraits &Traits>
ChipLayout* fam65xx_t<Traits>::get_chip_layout() const {
    static ChipLayout layout = create_cpu_pin_layout<Traits>();
    return &layout;
}

template <const CPUTraits &Traits>
std::vector<PinSignalState> fam65xx_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    bus_state_t bus_state = this->bus_snapshot_;
    if (bus_state == 0) {
      bus_state = FAM65XX_GUI_DEFAULT_STATE;
      BUS_SET_ADDR(bus_state, this->get(REG_AB));
      BUS_SET_DATA(bus_state, this->get(REG_DL));
    }
    return get_cpu_pin_states<Traits>(this, &layout, bus_state);
}

template <const CPUTraits &Traits>
const char* fam65xx_t<Traits>::get_layout_chip_name() const {
    return get_processor_name<Traits>();
}

// Explicit template instantiations for all CPU variants
template void fam65xx_t<MOS6502Traits>::render_settings_content();
template ChipLayout* fam65xx_t<MOS6502Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<MOS6502Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<MOS6502Traits>::get_layout_chip_name() const;

template void fam65xx_t<MOS6510Traits>::render_settings_content();
template ChipLayout* fam65xx_t<MOS6510Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<MOS6510Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<MOS6510Traits>::get_layout_chip_name() const;

template void fam65xx_t<CSG7501Traits>::render_settings_content();
template ChipLayout* fam65xx_t<CSG7501Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<CSG7501Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<CSG7501Traits>::get_layout_chip_name() const;

template void fam65xx_t<RICOH_2A03Traits>::render_settings_content();
template ChipLayout* fam65xx_t<RICOH_2A03Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<RICOH_2A03Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<RICOH_2A03Traits>::get_layout_chip_name() const;

template void fam65xx_t<MOS6507Traits>::render_settings_content();
template ChipLayout* fam65xx_t<MOS6507Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<MOS6507Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<MOS6507Traits>::get_layout_chip_name() const;

template void fam65xx_t<SYNERTEK_65C02Traits>::render_settings_content();
template ChipLayout* fam65xx_t<SYNERTEK_65C02Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<SYNERTEK_65C02Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<SYNERTEK_65C02Traits>::get_layout_chip_name() const;

template void fam65xx_t<WDC_65C02_EARLYTraits>::render_settings_content();
template ChipLayout* fam65xx_t<WDC_65C02_EARLYTraits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<WDC_65C02_EARLYTraits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<WDC_65C02_EARLYTraits>::get_layout_chip_name() const;

template void fam65xx_t<WDC_W65C02STraits>::render_settings_content();
template ChipLayout* fam65xx_t<WDC_W65C02STraits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<WDC_W65C02STraits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<WDC_W65C02STraits>::get_layout_chip_name() const;

template void fam65xx_t<ROCKWELL_R65C02Traits>::render_settings_content();
template ChipLayout* fam65xx_t<ROCKWELL_R65C02Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<ROCKWELL_R65C02Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<ROCKWELL_R65C02Traits>::get_layout_chip_name() const;

template void fam65xx_t<WDC_65C816Traits>::render_settings_content();
template ChipLayout* fam65xx_t<WDC_65C816Traits>::get_chip_layout() const;
template std::vector<PinSignalState> fam65xx_t<WDC_65C816Traits>::get_layout_pin_states(ChipLayout&);
template const char* fam65xx_t<WDC_65C816Traits>::get_layout_chip_name() const;

} // namespace fam65xx

#endif // CERMU_HAS_GUI