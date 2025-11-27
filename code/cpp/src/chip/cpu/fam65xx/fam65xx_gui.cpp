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
#include <map>
#include <memory>
#include <thread>
#include <type_traits>
#include <unordered_map>

// Include the modern fam65xx implementation
#include "fam65xx.hpp"
#include "fam65xx_decoder.h"
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.h"

// Include GUI interface first (defines IMGUI_VERSION)
#include "../../../core/chip_layout.h"
#include "../../../core/pin_macros.h"
#include "../../../gui/imgui_interface.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#include <imgui.h>
#endif
#include "fam65xx_layouts.h"

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

// Helper function to get addressing mode name (uses decoder)
static const char *get_addressing_mode_name(uint8_t am_index) {
  return fam65xx_get_addressing_mode_name(am_index);
}

// Helper function to get opcode name from operation enumeration
static const char *get_opcode_name(uint8_t op_index) {
  return fam65xx_get_opcode_name(op_index);
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

#ifdef IMGUI_VERSION
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

// Fallback version without bus state
template <const CPUTraits &Traits>
void render_chip_visualization(fam65xx_t<Traits> *cpu, ImVec2 chip_center) {
  // Create default bus state from CPU registers if possible
  bus_state_t bus_state = 0;
  if (cpu) {
    BUS_SET_ADDR(bus_state, cpu->get(REG_AB));
    BUS_SET_DATA(bus_state, cpu->get(REG_DL));
    // Set safe defaults for control signals
    bus_state |= BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) |
                 BUS_BIT(BUS_RES_BIT) | BUS_BIT(BUS_IRQ_BIT) |
                 BUS_BIT(BUS_NMI_BIT);
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

    // Current opcode information
    ImGui::Text("Current Opcode Info:");
    ImGui::Indent(16.0f);

    // Show current addressing mode and opcode information if available
    if (cpu) {
      const char *am_name =
          get_addressing_mode_name(cpu->opcode_entry.am_index);
      const char *op_name = get_opcode_name(cpu->opcode_entry.op_index);
      ImGui::Text("Current Opcode:      $%02X", cpu->get(REG_IR));
      ImGui::Text("Addressing Mode:     %s", am_name);
      ImGui::Text("Instruction:         %s", op_name);
      ImGui::Text("Instruction Cycle:   %d", cpu->half_cycle);
      ImGui::Text("Opcode Done:         %s", cpu->opdone() ? "Yes" : "No");
    } else {
      ImGui::Text("Current Opcode:      N/A (CPU not available)");
      ImGui::Text("Addressing Mode:     N/A");
      ImGui::Text("Instruction:         N/A");
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
// GENERIC CPU TYPE DETECTION AND DISPATCH
// ============================================================================

// Base class for type-erased CPU GUI rendering
class CPUGUIRenderer {
public:
  virtual ~CPUGUIRenderer() = default;
  virtual void render_debug_window(bool *show_window) = 0;
  virtual void render_settings_window(bool *show_window) = 0;
  virtual const char *get_processor_name() const = 0;
  virtual void update_bus_state(bus_state_t pins) = 0;
};

// Template implementation for specific CPU types
template <const CPUTraits &Traits>
class CPUGUIRendererImpl : public CPUGUIRenderer {
private:
  fam65xx_t<Traits> *cpu;
  bus_state_t last_bus_state;

public:
  explicit CPUGUIRendererImpl(fam65xx_t<Traits> *cpu_ptr)
      : cpu(cpu_ptr), last_bus_state(0) {}

  // Update the stored bus state (should be called from tick functions)
  void update_bus_state(bus_state_t pins) override { last_bus_state = pins; }

  void render_debug_window(bool *show_window) override {
    if (!cpu || !show_window || !*show_window)
      return;

    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug",
             get_processor_name());

    if (!ImGui::Begin(window_title, show_window)) {
      ImGui::End();
      return;
    }

    // Create two-column layout: chip visualization on left, debugging info on
    // right
    ImVec2 window_size = ImGui::GetWindowSize();

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

      // Show chip visualization with real bus state from emulation
      render_chip_visualization<Traits>(cpu, chip_center, last_bus_state);
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f); // Small gap between columns

    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(
        window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      render_cpu_registers<Traits>(cpu);
      ImGui::Separator();

      render_internal_state<Traits>(cpu);
      ImGui::Separator();

      render_interrupt_state<Traits>(cpu);
      ImGui::Separator();

      render_processor_features<Traits>(cpu);
    }
    ImGui::EndChild();

    ImGui::End();
  }

  void render_settings_window(bool *show_window) override {
    if (!cpu || !show_window || !*show_window)
      return;

    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings",
             get_processor_name());

    if (!ImGui::Begin(window_title, show_window)) {
      ImGui::End();
      return;
    }

    ImGui::Text("%s Configuration", get_processor_name());
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
    render_processor_features<Traits>(cpu);

    ImGui::End();
  }

  const char *get_processor_name() const override {
    // Use a thread-local static buffer to avoid conflicts between template
    // instantiations
    static thread_local char processor_name_buffer[64];
    snprintf(processor_name_buffer, sizeof(processor_name_buffer), "%s %s",
             Traits.get_vendor(), Traits.get_chip_id());
    return processor_name_buffer;
  }
};

// Factory function to create appropriate renderer
// This would be called by the specific CPU implementations (mos6502.cpp, etc.)
template <const fam65xx::CPUTraits &Traits>
CPUGUIRenderer *create_cpu_gui_renderer(fam65xx::fam65xx_t<Traits> *cpu) {
  return new CPUGUIRendererImpl<Traits>(cpu);
}

// Global storage for CPU renderers (keyed by chip pointer)
// In a real implementation, this might be part of the chip descriptor
static std::unordered_map<void *, std::unique_ptr<CPUGUIRenderer>>
    cpu_renderers;

// ============================================================================
// C INTERFACE FUNCTIONS (for chip descriptor callbacks)
// ============================================================================

extern "C" {

void fam65xx_render_debug_window(void *chip, bool *show_window) {
  // Look up the CPU renderer in our registry
  auto it = cpu_renderers.find(chip);
  if (it != cpu_renderers.end()) {
    it->second->render_debug_window(show_window);
  } else {
    // Fallback for unknown CPU types
    if (show_window && *show_window) {
      if (!ImGui::Begin("Unknown 65xx CPU Debug", show_window)) {
        ImGui::End();
        return;
      }
      ImGui::Text("CPU type not registered for GUI rendering");
      ImGui::Text("Chip pointer: %p", chip);
      ImGui::End();
    }
  }
}

void fam65xx_render_settings_window(void *chip, bool *show_window) {
  // Look up the CPU renderer in our registry
  auto it = cpu_renderers.find(chip);
  if (it != cpu_renderers.end()) {
    it->second->render_settings_window(show_window);
    return;
  }

  // Fallback for unknown CPU types
  if (!chip || !show_window || !*show_window)
    return;

  if (!ImGui::Begin("Unknown 65xx CPU Settings", show_window)) {
    ImGui::End();
    return;
  }

  ImGui::Text("65xx Family CPU Configuration");
  ImGui::Text("CPU type not registered for GUI rendering");
  ImGui::Separator();

  // Pin Configuration (static info, doesn't need CPU access)
  if (ImGui::CollapsingHeader("Pin Configuration")) {
    ImGui::Indent(16.0f);
    ImGui::Text("MOS 65xx DIP-40 Package (40 pins):");
    ImGui::Separator();

    ImGui::Text("Power and Clock:");
    ImGui::Text("  VCC (8) - +5V Power Supply");
    ImGui::Text("  VSS (21) - Ground (0V)");
    ImGui::Text("  φ0 (3) - Phase 0 Clock Input");
    ImGui::Text("  φ1 (37) - Phase 1 Clock Output");
    ImGui::Text("  φ2 (39) - Phase 2 Clock Output");

    ImGui::Separator();

    ImGui::Text("Address Bus (16 lines):");
    ImGui::Text("  A0-A15 (9-20, 22-25) - Address Lines");

    ImGui::Separator();

    ImGui::Text("Data Bus (8 lines):");
    ImGui::Text("  D0-D7 (26, 28-33) - Data Lines");

    ImGui::Separator();

    ImGui::Text("Control Lines:");
    ImGui::Text("  R/W̅ (34) - Read/Write");
    ImGui::Text("  SYNC (7) - Synchronize");
    ImGui::Text("  RDY (2) - Ready");

    ImGui::Separator();

    ImGui::Text("Interrupt Lines:");
    ImGui::Text("  IRQ̅ (4) - Interrupt Request");
    ImGui::Text("  NMI̅ (6) - Non-Maskable Interrupt");
    ImGui::Text("  RES̅ (40) - Reset");

    ImGui::Separator();

    ImGui::Text("Special:");
    ImGui::Text("  SO̅ (38) - Set Overflow");
    ImGui::Text("  BE (36) - Bus Enable");
    ImGui::Text("  ML̅ (35) - Memory Lock");

    ImGui::Unindent(16.0f);
  }

  ImGui::Separator();

  // CPU Controls (placeholder - would need CPU access for real functionality)
  if (ImGui::CollapsingHeader("CPU Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(16.0f);

    ImVec2 button_size = ImVec2(0, 0);
    if (ImGui::Button("Reset CPU", button_size)) {
      // Reset would require bus state - this is just UI placeholder
    }

    ImGui::SameLine();

    if (ImGui::Button("Trigger NMI", button_size)) {
      // NMI trigger would require pin manipulation
    }

    ImGui::SameLine();

    if (ImGui::Button("Trigger IRQ", button_size)) {
      // IRQ trigger would require pin manipulation
    }

    ImGui::Separator();

    static bool step_mode = false;
    ImGui::Checkbox("Single Step Mode", &step_mode);

    static bool trace_mode = false;
    ImGui::Checkbox("Instruction Trace", &trace_mode);

    static bool break_on_brk = true;
    ImGui::Checkbox("Break on BRK instruction", &break_on_brk);

    ImGui::Unindent(16.0f);
  }

  ImGui::End();
}

void fam65xx_update_bus_state(void *chip, bus_state_t bus_state) {
  // Update the bus state for the given CPU chip
  auto it = cpu_renderers.find(chip);
  if (it != cpu_renderers.end()) {
    it->second->update_bus_state(bus_state);
  }
}

} // extern "C"
#endif // IMGUI_VERSION

// ============================================================================
// C++ REGISTRATION API
// ============================================================================

#ifdef IMGUI_VERSION

namespace fam65xx {

template <const CPUTraits &Traits>
void register_cpu_for_gui(fam65xx_t<Traits> *cpu) {
  if (cpu) {
    cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<Traits>>(cpu);
  }
}

// Non-template registration functions for different CPU types
void register_mos6502_for_gui(void *cpu) {
  if (cpu) {
    cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<fam65xx::MOS6502>>(
        reinterpret_cast<fam65xx_t<fam65xx::MOS6502> *>(cpu));
  }
}

void register_nes6502_for_gui(void *cpu) {
  if (cpu) {
    cpu_renderers[cpu] =
        std::make_unique<CPUGUIRendererImpl<fam65xx::RICOH_2A03>>(
            reinterpret_cast<fam65xx_t<fam65xx::RICOH_2A03> *>(cpu));
  }
}

void register_rockwell65c02_for_gui(void *cpu) {
  if (cpu) {
    cpu_renderers[cpu] =
        std::make_unique<CPUGUIRendererImpl<fam65xx::ROCKWELL_R65C02>>(
            reinterpret_cast<fam65xx_t<fam65xx::ROCKWELL_R65C02> *>(cpu));
  }
}

void register_mos6510_for_gui(void *cpu) {
  if (cpu) {
    cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<fam65xx::MOS6510>>(
        reinterpret_cast<fam65xx_t<fam65xx::MOS6510> *>(cpu));
  }
}

void register_wdc65c816_for_gui(void *cpu) {
  if (cpu) {
    cpu_renderers[cpu] =
        std::make_unique<CPUGUIRendererImpl<fam65xx::WDC_65C816>>(
            reinterpret_cast<fam65xx_t<fam65xx::WDC_65C816> *>(cpu));
  }
}

void unregister_cpu_from_gui(void *cpu) {
  auto it = cpu_renderers.find(cpu);
  if (it != cpu_renderers.end()) {
    cpu_renderers.erase(it);
  }
}

// Simple non-template functions for rendering CPU windows
void render_cpu_debug_window_impl(void *cpu, const char *cpu_name) {
  auto it = cpu_renderers.find(cpu);
  if (it != cpu_renderers.end()) {
    bool show_window = true;
    it->second->render_debug_window(&show_window);
  }
}

void render_cpu_settings_window_impl(void *cpu, const char *cpu_name) {
  auto it = cpu_renderers.find(cpu);
  if (it != cpu_renderers.end()) {
    bool show_window = true;
    it->second->render_settings_window(&show_window);
  }
}

// Explicit template instantiations for CPUGUIRendererImpl - TEMPORARILY
// DISABLED template class CPUGUIRendererImpl<fam65xx::MOS6502>; template class
// CPUGUIRendererImpl<fam65xx::MOS6510>; template class
// CPUGUIRendererImpl<fam65xx::RICOH_2A03>; template class
// CPUGUIRendererImpl<fam65xx::ROCKWELL_R65C02>; template class
// CPUGUIRendererImpl<fam65xx::WDC_65C816>;

} // namespace fam65xx
#endif // IMGUI_VERSION