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
// Helper function to format processor flags (unchanged from original)
static void format_processor_flags(uint8_t flags, char *buffer,
                                   size_t buffer_size) {
  snprintf(buffer, buffer_size, "%c%c%c%c%c%c%c%c",
           (flags & FLAG_N) ? 'N' : 'n', (flags & FLAG_V) ? 'V' : 'v',
           (flags & FLAG_U) ? 'U' : 'u', (flags & FLAG_B) ? 'B' : 'b',
           (flags & FLAG_D) ? 'D' : 'd', (flags & FLAG_I) ? 'I' : 'i',
           (flags & FLAG_Z) ? 'Z' : 'z', (flags & FLAG_C) ? 'C' : 'c');
}

// Flag names for processor status register
static const char *flag_names[] = {"C", "Z", "I", "D", "B", "U", "V", "N"};

#ifdef CERMU_HAS_GUI
// ============================================================================
// CPU-SPECIFIC CHIP VISUALIZATION HELPERS
// ============================================================================

template <const CPUTraits &Traits>
void render_chip_visualization(fam65xx_t<Traits> *cpu, ImVec2 chip_center,
                               bus_state_t bus_state) {
  // Get global renderer and chip layout
  ChipVisualization &renderer = GetGlobalChipRenderer();
  static ChipLayout layout = create_cpu_pin_layout<Traits>();

  // Get current pin states from CPU and bus state
  const ChipLayout *const_layout = &layout;
  std::vector<PinSignalState> pin_states =
      get_cpu_pin_states<Traits>(cpu, const_layout, bus_state);

  // Render the chip using global renderer
  const char *chip_name = get_processor_name<Traits>();
  renderer.render(layout, chip_center, pin_states, chip_name);
}

// Safe default control-signal state for GUI rendering when no live bus state is available.
static constexpr bus_state_t FAM65XX_GUI_DEFAULT_STATE =
    BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_RES_BIT) |
    BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_NMI_BIT);

// Fallback version without bus state
template <const CPUTraits &Traits>
void render_chip_visualization(fam65xx_t<Traits> *cpu, ImVec2 chip_center) {
  // Create default bus state from CPU registers if possible
  bus_state_t bus_state = FAM65XX_GUI_DEFAULT_STATE;
  if (cpu) {
    BUS_SET_ADDR(bus_state, cpu->get(REG_AB));
    BUS_SET_DATA(bus_state, cpu->get(REG_DL));
  }
  render_chip_visualization<Traits>(cpu, chip_center, bus_state);
}

// ============================================================================
// TEMPLATE-BASED GUI RENDERING FUNCTIONS
// ============================================================================

template <const CPUTraits &Traits>
void render_cpu_registers(fam65xx_t<Traits> *cpu) {
  if (ImGui::CollapsingHeader("CPU Registers",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(16.0f);

    // Main registers - use native accessors
    ImGui::Text("Accumulator (A):       $%02X (%d)", cpu->get(REG_A),
                cpu->get(REG_A));
    ImGui::Text("X Index (X):           $%02X (%d)", cpu->get(REG_X),
                cpu->get(REG_X));
    ImGui::Text("Y Index (Y):           $%02X (%d)", cpu->get(REG_Y),
                cpu->get(REG_Y));
    ImGui::Text("Stack Pointer (S):   $%04X", cpu->get(REG_SP));
    ImGui::Text("Program Counter:     $%04X", cpu->get(REG_PC));

    ImGui::Separator();

    // Processor status with detailed breakdown
    uint8_t p_reg = cpu->get(REG_P);
    char flag_buffer[16];
    format_processor_flags(p_reg, flag_buffer, sizeof(flag_buffer));
    ImGui::Text("Processor Status (P):  $%02X (%s)", p_reg, flag_buffer);

    ImGui::Indent(16.0f);
    for (int i = 0; i < 8; i++) {
      bool flag_set = (p_reg & (1 << i)) != 0;
      ImGui::Text("  %s: %s", flag_names[i], flag_set ? "Set" : "Clear");
    }
    ImGui::Unindent(16.0f);

    // Show 65C816-specific registers if available
    if constexpr (Traits.has(CPUCoreFlags::C816_16BIT)) {
      ImGui::Separator();
      ImGui::Text("65C816 Extended Registers:");
      ImGui::Indent(16.0f);
      // Note: These would need additional accessors in the CPU template
      ImGui::Text("Direct Page (D):      $%04X", 0);   // Placeholder
      ImGui::Text("Data Bank (DB):         $%02X", 0); // Placeholder
      ImGui::Text("Program Bank (PB):      $%02X", 0); // Placeholder
      ImGui::Unindent(16.0f);
    }

    ImGui::Unindent(16.0f);
  }
}

template <const CPUTraits &Traits>
void render_internal_state(fam65xx_t<Traits> *cpu) {
  if (ImGui::CollapsingHeader("Internal State",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(16.0f);

    // Internal registers - use native accessors
    ImGui::Text("Instruction Register:  $%02X", cpu->get(REG_IR));
    ImGui::Text("Data Latch:            $%02X", cpu->get(REG_DL));

    // Format address bus display based on address width
    if constexpr (Traits.address_bits <= 16) {
      ImGui::Text("Address Bus:         $%04X", cpu->get(REG_AB));
    } else if constexpr (Traits.address_bits <= 20) {
      ImGui::Text("Address Bus:        $%05X", cpu->get(REG_AB));
    } else {
      ImGui::Text("Address Bus:       $%06X", cpu->get(REG_AB));
    }

    ImGui::Text("Cycle Index:           %d", cpu->half_cycle);

    ImGui::Separator();

    // Current opcode information with full disassembly
    ImGui::Text("Current Opcode Info:");
    ImGui::Indent(16.0f);

    // Show disassembled instruction if available
    if (cpu) {
      uint16_t pc = cpu->get(REG_PC);
      uint8_t opcode = cpu->get(REG_IR);
      
      // Get operand bytes (note: these might not be valid if instruction hasn't fully fetched yet)
      // For now we'll use placeholder values - in a real implementation you'd read from memory
      uint8_t operand1 = 0x00;
      uint8_t operand2 = 0x00;
      
      // Disassemble the instruction
      char disasm_buffer[64];
      fam65xx_disassemble_instruction(pc, cpu->opcode_entry, operand1, operand2,
                                      disasm_buffer, sizeof(disasm_buffer));

      ImGui::Text("Instruction Cycle:   %d", cpu->half_cycle);
      ImGui::Text("Program Counter:     $%04X", pc);
      ImGui::Text("Opcode Byte:         $%02X", opcode);
      ImGui::Text("Current Instruction: %s", disasm_buffer);
      ImGui::Text("Opcode Done:         %s", cpu->opdone() ? "Yes" : "No");
    } else {
      ImGui::Text("Current Instruction: N/A (CPU not available)");
    }

    // Show processor-specific execution features
    if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
      ImGui::Text("CMOS Features:       Active");
    }
    if constexpr (Traits.has(CPUCoreFlags::ILLEGAL_OPCODES)) {
      ImGui::Text("Illegal Opcodes:     Supported");
    }
    ImGui::Unindent(16.0f);

    ImGui::Unindent(16.0f);
  }
}

template <const CPUTraits &Traits>
void render_chip_visualization(fam65xx_t<Traits> *cpu) {
  if (ImGui::CollapsingHeader("Chip Visualization",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(16.0f);

    // Calculate available space
    ImVec2 available_size = ImGui::GetContentRegionAvail();
    float chip_width = 300.0f;
    float chip_height = 400.0f;

    // Center the chip in available space
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

    // Get the pin layout for this CPU type to determine actual chip dimensions
    static ChipLayout layout = create_cpu_pin_layout<Traits>();

    ImVec2 chip_center = ImVec2(cursor_pos.x + available_size.x / 2,
                                cursor_pos.y + layout.package.height / 2 + 20);

    // Reserve space for the chip drawing
    ImVec2 dummy_size = ImVec2(available_size.x, layout.package.height + 40);
    ImGui::Dummy(dummy_size);

    // Draw the CPU-specific chip visualization
    render_chip_visualization<Traits>(cpu, chip_center);

    ImGui::Separator();

    // Render pin legend using the global visualization system
    ChipVisualization &renderer = GetGlobalChipRenderer();
    renderer.render_legend();

    ImGui::Unindent(16.0f);
  }
}

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

template <const CPUTraits &Traits>
void render_interrupt_state(fam65xx_t<Traits> *cpu) {
  if (ImGui::CollapsingHeader("Interrupt State")) {
    ImGui::Indent(16.0f);

    // Note: These would need public accessors or friend functions
    ImGui::Text("Interrupt System:");
    ImGui::Indent(16.0f);
    ImGui::Text("IRQ Disabled:   %s",
                (cpu->get(REG_P) & FLAG_I) ? "Yes" : "No");

    // Show NMI edge detection for NMOS processors
    if constexpr (Traits.is_nmos()) {
      ImGui::Text("NMI Edge Detection: Active (NMOS)");
    }

    // Show 65C02-specific interrupt features
    if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
      ImGui::Text("WAI/STP Support:    Available");
    }

    ImGui::Unindent(16.0f);

    ImGui::Unindent(16.0f);
  }
}

// ============================================================================
// ChipBase VIRTUAL METHOD IMPLEMENTATIONS (template definitions)
// Must be inside namespace fam65xx for proper linkage of explicit instantiations
// ============================================================================

namespace fam65xx {

template <const CPUTraits &Traits>
void fam65xx_t<Traits>::render_debug_content() {
    // Create two-column layout: chip visualization on left, debugging info on
    // right
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    // Left column: Chip Visualization (fixed width ~250px, 25% wider)
    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      ImGui::Text("Chip Visualization");
      ImGui::Separator();

      // Calculate chip center for visualization
      ImVec2 chip_center = ImGui::GetCursorScreenPos();
      ImVec2 content_region = ImGui::GetContentRegionAvail();
      chip_center.x += content_region.x * 0.5f;
      chip_center.y += 200.0f; // Space for the chip

      // Compute bus state from registers if not externally set
      bus_state_t bus_state = this->bus_snapshot_;
      if (bus_state == 0) {
        bus_state = FAM65XX_GUI_DEFAULT_STATE;
        BUS_SET_ADDR(bus_state, this->get(REG_AB));
        BUS_SET_DATA(bus_state, this->get(REG_DL));
      }

      // Show chip visualization with bus state
      render_chip_visualization<Traits>(this, chip_center, bus_state);
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f); // Small gap between columns

    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(
        window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      render_cpu_registers<Traits>(this);
      ImGui::Separator();

      render_internal_state<Traits>(this);
      ImGui::Separator();

      render_interrupt_state<Traits>(this);
      ImGui::Separator();

      render_processor_features<Traits>(this);
    }
    ImGui::EndChild();
}

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
void fam65xx_t<Traits>::render_layout_content() {
    static ChipLayout layout = create_cpu_pin_layout<Traits>();
    bus_state_t bus_state = this->bus_snapshot_;
    if (bus_state == 0) {
      bus_state = FAM65XX_GUI_DEFAULT_STATE;
      BUS_SET_ADDR(bus_state, this->get(REG_AB));
      BUS_SET_DATA(bus_state, this->get(REG_DL));
    }
    std::vector<PinSignalState> pin_states =
        get_cpu_pin_states<Traits>(this, &layout, bus_state);
    render_chip_layout(layout, pin_states, get_processor_name<Traits>());
}

// Explicit template instantiations for all CPU variants
template void fam65xx_t<MOS6502Traits>::render_debug_content();
template void fam65xx_t<MOS6502Traits>::render_settings_content();
template void fam65xx_t<MOS6502Traits>::render_layout_content();

template void fam65xx_t<MOS6510Traits>::render_debug_content();
template void fam65xx_t<MOS6510Traits>::render_settings_content();
template void fam65xx_t<MOS6510Traits>::render_layout_content();

template void fam65xx_t<CSG7501Traits>::render_debug_content();
template void fam65xx_t<CSG7501Traits>::render_settings_content();
template void fam65xx_t<CSG7501Traits>::render_layout_content();

template void fam65xx_t<RICOH_2A03Traits>::render_debug_content();
template void fam65xx_t<RICOH_2A03Traits>::render_settings_content();
template void fam65xx_t<RICOH_2A03Traits>::render_layout_content();

template void fam65xx_t<SYNERTEK_65C02Traits>::render_debug_content();
template void fam65xx_t<SYNERTEK_65C02Traits>::render_settings_content();
template void fam65xx_t<SYNERTEK_65C02Traits>::render_layout_content();

template void fam65xx_t<WDC_65C02_EARLYTraits>::render_debug_content();
template void fam65xx_t<WDC_65C02_EARLYTraits>::render_settings_content();
template void fam65xx_t<WDC_65C02_EARLYTraits>::render_layout_content();

template void fam65xx_t<WDC_W65C02STraits>::render_debug_content();
template void fam65xx_t<WDC_W65C02STraits>::render_settings_content();
template void fam65xx_t<WDC_W65C02STraits>::render_layout_content();

template void fam65xx_t<ROCKWELL_R65C02Traits>::render_debug_content();
template void fam65xx_t<ROCKWELL_R65C02Traits>::render_settings_content();
template void fam65xx_t<ROCKWELL_R65C02Traits>::render_layout_content();

template void fam65xx_t<WDC_65C816Traits>::render_debug_content();
template void fam65xx_t<WDC_65C816Traits>::render_settings_content();
template void fam65xx_t<WDC_65C816Traits>::render_layout_content();

} // namespace fam65xx

#endif // CERMU_HAS_GUI

// ============================================================================
// Stub implementations when ImGui is not available
// ============================================================================

#ifndef CERMU_HAS_GUI

namespace fam65xx {

template <const CPUTraits &Traits>
void fam65xx_t<Traits>::render_debug_content() {}

template <const CPUTraits &Traits>
void fam65xx_t<Traits>::render_settings_content() {}

template <const CPUTraits &Traits>
void fam65xx_t<Traits>::render_layout_content() {}

// Explicit template instantiations for non-GUI builds
template void fam65xx_t<MOS6502Traits>::render_debug_content();
template void fam65xx_t<MOS6502Traits>::render_settings_content();
template void fam65xx_t<MOS6502Traits>::render_layout_content();

template void fam65xx_t<MOS6510Traits>::render_debug_content();
template void fam65xx_t<MOS6510Traits>::render_settings_content();
template void fam65xx_t<MOS6510Traits>::render_layout_content();

template void fam65xx_t<CSG7501Traits>::render_debug_content();
template void fam65xx_t<CSG7501Traits>::render_settings_content();
template void fam65xx_t<CSG7501Traits>::render_layout_content();

template void fam65xx_t<RICOH_2A03Traits>::render_debug_content();
template void fam65xx_t<RICOH_2A03Traits>::render_settings_content();
template void fam65xx_t<RICOH_2A03Traits>::render_layout_content();

template void fam65xx_t<SYNERTEK_65C02Traits>::render_debug_content();
template void fam65xx_t<SYNERTEK_65C02Traits>::render_settings_content();
template void fam65xx_t<SYNERTEK_65C02Traits>::render_layout_content();

template void fam65xx_t<WDC_65C02_EARLYTraits>::render_debug_content();
template void fam65xx_t<WDC_65C02_EARLYTraits>::render_settings_content();
template void fam65xx_t<WDC_65C02_EARLYTraits>::render_layout_content();

template void fam65xx_t<WDC_W65C02STraits>::render_debug_content();
template void fam65xx_t<WDC_W65C02STraits>::render_settings_content();
template void fam65xx_t<WDC_W65C02STraits>::render_layout_content();

template void fam65xx_t<ROCKWELL_R65C02Traits>::render_debug_content();
template void fam65xx_t<ROCKWELL_R65C02Traits>::render_settings_content();
template void fam65xx_t<ROCKWELL_R65C02Traits>::render_layout_content();

template void fam65xx_t<WDC_65C816Traits>::render_debug_content();
template void fam65xx_t<WDC_65C816Traits>::render_settings_content();
template void fam65xx_t<WDC_65C816Traits>::render_layout_content();

} // namespace fam65xx

#endif // !CERMU_HAS_GUI