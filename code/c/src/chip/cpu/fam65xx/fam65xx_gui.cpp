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
#include <type_traits>
#include <unordered_map>
#include <memory>
#include <map>

// Include the modern fam65xx implementation
#include "fam65xx.hpp"
#include "fam65xx_types.h"
#include "fam65xx_processor_traits.hpp"

// Include generic chip visualization system
#include "../../gui/chip_visualization.h"
#include "cpu_pin_layouts.h"

// Include GUI interface
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>

using namespace fam65xx;

// ============================================================================
// CPU TRAIT-BASED HELPER FUNCTIONS
// ============================================================================

// Template function to get processor name based on traits
template<const CPUTraits& Traits>
const char* get_processor_name() {
    if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6502)>) {
        return "MOS 6502 (NMOS)";
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6510)>) {
        return "MOS 6510 (C64/C128)";
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_W65C02S)>) {
        return "WDC 65C02S (CMOS)";
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::ROCKWELL_R65C02)>) {
        return "Rockwell R65C02";
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_65C816)>) {
        return "WDC 65C816 (16-bit)";
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::RICOH_2A03)>) {
        return "RICOH 2A03 (NES)";
    } else {
        return "65xx Family CPU";
    }
}

// Helper function to get addressing mode name (unchanged from original)
static const char* get_addressing_mode_name(uint8_t am_index) {
    static const char* am_names[] = {
        "Implicit",     // 0
        "Accumulator",  // 1
        "Immediate",    // 2
        "Zero Page",    // 3
        "Zero Page,X",  // 4
        "Zero Page,Y",  // 5
        "Absolute",     // 6
        "Absolute,X",   // 7
        "Absolute,Y",   // 8
        "Indirect",     // 9
        "Indexed Indirect", // 10 (zp,X)
        "Indirect Indexed", // 11 (zp),Y
        "Relative",     // 12
        "ZP Indirect",  // 13 (zp) - 65C02
        "Absolute Indexed Indirect", // 14 (abs,X) - 65C02/65C816
        "Stack Relative", // 15 - 65C816
    };
    
    if (am_index < sizeof(am_names) / sizeof(am_names[0])) {
        return am_names[am_index];
    }
    return "Unknown";
}

// Helper function to format processor flags (unchanged from original)
static void format_processor_flags(uint8_t flags, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%c%c%c%c%c%c%c%c",
             (flags & FLAG_N) ? 'N' : 'n',
             (flags & FLAG_V) ? 'V' : 'v',
             (flags & FLAG_U) ? 'U' : 'u',
             (flags & FLAG_B) ? 'B' : 'b',
             (flags & FLAG_D) ? 'D' : 'd',
             (flags & FLAG_I) ? 'I' : 'i',
             (flags & FLAG_Z) ? 'Z' : 'z',
             (flags & FLAG_C) ? 'C' : 'c');
}

// Flag names for processor status register
static const char* flag_names[] = {
    "C", "Z", "I", "D", "B", "U", "V", "N"
};

// ============================================================================
// CPU-SPECIFIC CHIP VISUALIZATION HELPERS
// ============================================================================

// Template function to create and render CPU chip visualization
template<const CPUTraits& Traits>
void render_chip_visualization(fam65xx_t<Traits>* cpu, ImVec2 chip_center, bus_state_t bus_state) {
    static std::unique_ptr<ChipVisualization> chip_viz = nullptr;
    
    // Create chip visualization if not already created
    if (!chip_viz) {
        PinLayout layout = create_cpu_pin_layout<Traits>();
        chip_viz = std::make_unique<ChipVisualization>(layout);
    }
    
    // Get current pin states from CPU and bus state
    std::vector<PinState> pin_states = get_cpu_pin_states<Traits>(cpu, bus_state);
    
    // Render the chip
    const char* chip_name = get_processor_name<Traits>();
    chip_viz->render(chip_center, pin_states, chip_name);
}

// Fallback version without bus state
template<const CPUTraits& Traits>
void render_chip_visualization(fam65xx_t<Traits>* cpu, ImVec2 chip_center) {
    // Create default bus state from CPU registers if possible
    bus_state_t bus_state = 0;
    if (cpu) {
        BUS_SET_ADDR(bus_state, cpu->get(REG_AB));
        BUS_SET_DATA(bus_state, cpu->get(REG_DL));
        // Set safe defaults for control signals
        bus_state |= BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_RES_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_NMI_BIT);
    }
    render_chip_visualization<Traits>(cpu, chip_center, bus_state);
}

// ============================================================================
// TEMPLATE-BASED GUI RENDERING FUNCTIONS
// ============================================================================

template<const CPUTraits& Traits>
void render_cpu_registers(fam65xx_t<Traits>* cpu) {
    if (igCollapsingHeader_BoolPtr("CPU Registers", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Main registers - use native accessors
        igText("Accumulator (A):     $%02X (%d)", cpu->get(REG_A), cpu->get(REG_A));
        igText("X Index (X):         $%02X (%d)", cpu->get(REG_X), cpu->get(REG_X));
        igText("Y Index (Y):         $%02X (%d)", cpu->get(REG_Y), cpu->get(REG_Y));
        igText("Stack Pointer (S):   $%02X (Stack: $01%02X)", cpu->get(REG_S), cpu->get(REG_S));
        igText("Program Counter:     $%04X (%d)", cpu->get(REG_PC), cpu->get(REG_PC));
        
        igSeparator();
        
        // Processor status with detailed breakdown
        uint8_t p_reg = cpu->get(REG_P);
        char flag_buffer[16];
        format_processor_flags(p_reg, flag_buffer, sizeof(flag_buffer));
        igText("Processor Status (P): $%02X (%s)", p_reg, flag_buffer);
        
        igIndent(16.0f);
        for (int i = 0; i < 8; i++) {
            bool flag_set = (p_reg & (1 << i)) != 0;
            igText("  %s: %s", flag_names[i], flag_set ? "Set" : "Clear");
        }
        igUnindent(16.0f);
        
        // Show 65C816-specific registers if available
        if constexpr (Traits.has(CPUCoreFlags::C816_16BIT)) {
            igSeparator();
            igText("65C816 Extended Registers:");
            igIndent(16.0f);
            // Note: These would need additional accessors in the CPU template
            igText("Direct Page (D):     $%04X", 0); // Placeholder
            igText("Data Bank (DB):      $%02X", 0);  // Placeholder
            igText("Program Bank (PB):   $%02X", 0);  // Placeholder
            igUnindent(16.0f);
        }
        
        igUnindent(16.0f);
    }
}

template<const CPUTraits& Traits>
void render_internal_state(fam65xx_t<Traits>* cpu) {
    if (igCollapsingHeader_BoolPtr("Internal State", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Internal registers - use native accessors
        igText("Instruction Register: $%02X", cpu->get(REG_IR));
        igText("Data Latch:          $%02X", cpu->get(REG_DL));
        igText("Address Bus:         $%04X", cpu->get(REG_AB));
        igText("Cycle Index:         %d", cpu->cycle_index);
        
        igSeparator();
        
        // Current opcode information
        igText("Current Opcode Info:");
        igIndent(16.0f);
        // Note: These would need public access or accessor methods
        // For now, we'll show what we can access
        igText("Addressing Mode:     %s", "Available via template analysis");
        igText("Current Operation:   %s", "Available via opcode decode");
        
        // Show processor-specific execution features
        if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
            igText("CMOS Features:       Active");
        }
        if constexpr (Traits.has(CPUCoreFlags::ILLEGAL_OPCODES)) {
            igText("Illegal Opcodes:     Supported");
        }
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }
}

template<const CPUTraits& Traits>
void render_chip_visualization(fam65xx_t<Traits>* cpu) {
    if (igCollapsingHeader_BoolPtr("Chip Visualization", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Calculate available space
        ImVec2 available_size;
        igGetContentRegionAvail(&available_size);
        float chip_width = 200.0f;
        float chip_height = 400.0f;
        
        // Center the chip in available space
        ImVec2 cursor_pos;
        igGetCursorScreenPos(&cursor_pos);
        
        // Get the pin layout for this CPU type to determine actual chip dimensions
        static PinLayout layout = create_cpu_pin_layout<Traits>();
        
        ImVec2 chip_center = {
            cursor_pos.x + available_size.x / 2,
            cursor_pos.y + layout.package.height / 2 + 20
        };
        
        // Reserve space for the chip drawing
        ImVec2 dummy_size = {available_size.x, layout.package.height + 40};
        igDummy(dummy_size);
        
        // Draw the CPU-specific chip visualization
        render_chip_visualization<Traits>(cpu, chip_center);
        
        igSeparator();
        
        // Render pin legend using the generic visualization system
        static std::unique_ptr<ChipVisualization> legend_viz = nullptr;
        if (!legend_viz) {
            legend_viz = std::make_unique<ChipVisualization>(layout);
        }
        legend_viz->render_legend();
        
        igUnindent(16.0f);
        igUnindent(16.0f);
    }
}

template<const CPUTraits& Traits>
void render_processor_features(fam65xx_t<Traits>* cpu) {
    if (igCollapsingHeader_BoolPtr("Processor Features", NULL, 0)) {
        igIndent(16.0f);
        
        igText("Processor: %s", get_processor_name<Traits>());
        igSeparator();
        
        // Show trait-based features
        igText("Core Features:");
        igIndent(16.0f);
        
        if constexpr (Traits.has(CPUCoreFlags::HAS_DECIMAL_MODE)) {
            igText("✓ Decimal Mode (BCD)");
        } else {
            igText("✗ Decimal Mode (BCD)");
        }
        
        if constexpr (Traits.has(CPUCoreFlags::ILLEGAL_OPCODES)) {
            igText("✓ Illegal Opcodes");
        } else {
            igText("✗ Illegal Opcodes");
        }
        
        if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
            igText("✓ CMOS Design");
        } else {
            igText("✗ NMOS Design");
        }
        
        if constexpr (Traits.has(CPUCoreFlags::C816_16BIT)) {
            igText("✓ 16-bit Extensions");
        } else {
            igText("✗ 8-bit Only");
        }
        
        igUnindent(16.0f);
        
        // Processor-specific features
        if constexpr (Traits.has_io_port()) {
            igSeparator();
            igText("I/O Port Features:");
            igIndent(16.0f);
            igText("✓ Memory-mapped I/O at $00/$01");
            igText("✓ Bank switching support");
            igUnindent(16.0f);
        }
        
        if constexpr (Traits.has(CPUCoreFlags::ROCKWELL_BITS)) {
            igSeparator();
            igText("Rockwell Extensions:");
            igIndent(16.0f);
            igText("✓ Bit manipulation (RMB/SMB/BBR/BBS)");
            igUnindent(16.0f);
        }
        
        igUnindent(16.0f);
    }
}

template<const CPUTraits& Traits>
void render_interrupt_state(fam65xx_t<Traits>* cpu) {
    if (igCollapsingHeader_BoolPtr("Interrupt State", NULL, 0)) {
        igIndent(16.0f);
        
        // Note: These would need public accessors or friend functions
        igText("Interrupt System:");
        igIndent(16.0f);
        igText("IRQ Disabled:   %s", (cpu->get(REG_P) & FLAG_I) ? "Yes" : "No");
        
        // Show NMI edge detection for NMOS processors
        if constexpr (Traits.is_nmos()) {
            igText("NMI Edge Detection: Active (NMOS)");
        }
        
        // Show 65C02-specific interrupt features
        if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
            igText("WAI/STP Support:    Available");
        }
        
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }
}

// ============================================================================
// GENERIC CPU TYPE DETECTION AND DISPATCH
// ============================================================================

// Base class for type-erased CPU GUI rendering
class CPUGUIRenderer {
public:
    virtual ~CPUGUIRenderer() = default;
    virtual void render_debug_window(bool* show_window) = 0;
    virtual void render_settings_window(bool* show_window) = 0;
    virtual const char* get_processor_name() const = 0;
};

// Template implementation for specific CPU types
template<const CPUTraits& Traits>
class CPUGUIRendererImpl : public CPUGUIRenderer {
private:
    fam65xx_t<Traits>* cpu;
    
public:
    explicit CPUGUIRendererImpl(fam65xx_t<Traits>* cpu_ptr) : cpu(cpu_ptr) {}
    
    void render_debug_window(bool* show_window) override {
        if (!cpu || !show_window || !*show_window) return;
        
        char window_title[128];
        snprintf(window_title, sizeof(window_title), "%s Debug", get_processor_name());
        
        if (!igBegin(window_title, show_window, 0)) {
            igEnd();
            return;
        }

        igText("%s", get_processor_name());
        igText("MOS Technology 65xx Family Microprocessor");
        igSeparator();
        
        render_cpu_registers<Traits>(cpu);
        igSeparator();
        
        render_internal_state<Traits>(cpu);
        igSeparator();
        
        render_chip_visualization<Traits>(cpu);
        igSeparator();
        
        render_interrupt_state<Traits>(cpu);
        igSeparator();
        
        render_processor_features<Traits>(cpu);

        igEnd();
    }
    
    void render_settings_window(bool* show_window) override {
        if (!cpu || !show_window || !*show_window) return;
        
        char window_title[128];
        snprintf(window_title, sizeof(window_title), "%s Settings", get_processor_name());
        
        if (!igBegin(window_title, show_window, 0)) {
            igEnd();
            return;
        }

        igText("%s Configuration", get_processor_name());
        igSeparator();
        
        igText("Processor Family: MOS Technology 65xx");
        igText("Architecture: 8-bit microprocessor");
        igText("Address Space: 64KB (16-bit addressing)");  
        igText("Data Width: 8 bits");
        
        igSeparator();
        
        // Show processor-specific configuration options
        render_processor_features<Traits>(cpu);

        igEnd();
    }
    
    const char* get_processor_name() const override {
        if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6502)>) {
            return "MOS 6502 (NMOS)";
        } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6510)>) {
            return "MOS 6510 (C64/C128)";
        } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_W65C02S)>) {
            return "WDC 65C02S (CMOS)";
        } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::ROCKWELL_R65C02)>) {
            return "Rockwell R65C02";
        } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_65C816)>) {
            return "WDC 65C816 (16-bit)";
        } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::RICOH_2A03)>) {
            return "RICOH 2A03 (NES)";
        } else {
            return "65xx Family CPU";
        }
    }
};

// Factory function to create appropriate renderer
// This would be called by the specific CPU implementations (mos6502.cpp, etc.)
template<const fam65xx::CPUTraits& Traits>
CPUGUIRenderer* create_cpu_gui_renderer(fam65xx::fam65xx_t<Traits>* cpu) {
    return new CPUGUIRendererImpl<Traits>(cpu);
}

// Global storage for CPU renderers (keyed by chip pointer)
// In a real implementation, this might be part of the chip descriptor
static std::unordered_map<void*, std::unique_ptr<CPUGUIRenderer>> cpu_renderers;

// ============================================================================
// C INTERFACE FUNCTIONS (for chip descriptor callbacks)
// ============================================================================

extern "C" {

void fam65xx_render_debug_window(void* chip, bool* show_window) {
    // Look up the CPU renderer in our registry
    auto it = cpu_renderers.find(chip);
    if (it != cpu_renderers.end()) {
        it->second->render_debug_window(show_window);
    } else {
        // Fallback for unknown CPU types
        if (show_window && *show_window) {
            if (!igBegin("Unknown 65xx CPU Debug", show_window, 0)) {
                igEnd();
                return;
            }
            igText("CPU type not registered for GUI rendering");
            igText("Chip pointer: %p", chip);
            igEnd();
        }
    }
}

void fam65xx_render_settings_window(void* chip, bool* show_window) {
    // Look up the CPU renderer in our registry
    auto it = cpu_renderers.find(chip);
    if (it != cpu_renderers.end()) {
        it->second->render_settings_window(show_window);
        return;
    }
    
    // Fallback for unknown CPU types
    if (!chip || !show_window || !*show_window) return;
    
    if (!igBegin("Unknown 65xx CPU Settings", show_window, 0)) {
        igEnd();
        return;
    }

    igText("65xx Family CPU Configuration");
    igText("CPU type not registered for GUI rendering");
    igSeparator();
    
    // Pin Configuration (static info, doesn't need CPU access)
    if (igCollapsingHeader_BoolPtr("Pin Configuration", NULL, 0)) {
        igIndent(16.0f);
        igText("MOS 65xx DIP-40 Package (40 pins):");
        igSeparator();
        
        igText("Power and Clock:");
        igText("  VCC (8) - +5V Power Supply");
        igText("  VSS (21) - Ground (0V)");
        igText("  φ0 (3) - Phase 0 Clock Input");
        igText("  φ1 (37) - Phase 1 Clock Output");
        igText("  φ2 (39) - Phase 2 Clock Output");
        
        igSeparator();
        
        igText("Address Bus (16 lines):");
        igText("  A0-A15 (9-20, 22-25) - Address Lines");
        
        igSeparator();
        
        igText("Data Bus (8 lines):");
        igText("  D0-D7 (26, 28-33) - Data Lines");
        
        igSeparator();
        
        igText("Control Lines:");
        igText("  R/W̅ (34) - Read/Write");
        igText("  SYNC (7) - Synchronize");
        igText("  RDY (2) - Ready");
        
        igSeparator();
        
        igText("Interrupt Lines:");
        igText("  IRQ̅ (4) - Interrupt Request");
        igText("  NMI̅ (6) - Non-Maskable Interrupt");
        igText("  RES̅ (40) - Reset");
        
        igSeparator();
        
        igText("Special:");
        igText("  SO̅ (38) - Set Overflow");
        igText("  BE (36) - Bus Enable");
        igText("  ML̅ (35) - Memory Lock");
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // CPU Controls (placeholder - would need CPU access for real functionality)
    if (igCollapsingHeader_BoolPtr("CPU Controls", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        ImVec2 button_size = {0, 0};
        if (igButton("Reset CPU", button_size)) {
            // Reset would require bus state - this is just UI placeholder
        }
        
        igSameLine(0, -1);
        
        if (igButton("Trigger NMI", button_size)) {
            // NMI trigger would require pin manipulation
        }
        
        igSameLine(0, -1);
        
        if (igButton("Trigger IRQ", button_size)) {
            // IRQ trigger would require pin manipulation  
        }
        
        igSeparator();
        
        static bool step_mode = false;
        igCheckbox("Single Step Mode", &step_mode);
        
        static bool trace_mode = false;
        igCheckbox("Instruction Trace", &trace_mode);
        
        static bool break_on_brk = true;
        igCheckbox("Break on BRK instruction", &break_on_brk);
        
        igUnindent(16.0f);
    }

    igEnd();
}

} // extern "C"

// ============================================================================
// C++ REGISTRATION API
// ============================================================================

namespace fam65xx {

template<const CPUTraits& Traits>
void register_cpu_for_gui(fam65xx_t<Traits>* cpu) {
    if (cpu) {
        cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<Traits>>(cpu);
    }
}

// Non-template registration functions for different CPU types
void register_mos6502_for_gui(void* cpu) {
    if (cpu) {
        cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<MOS6502>>(reinterpret_cast<fam65xx_t<MOS6502>*>(cpu));
    }
}

void register_nes6502_for_gui(void* cpu) {
    if (cpu) {
        cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<RICOH_2A03>>(reinterpret_cast<fam65xx_t<RICOH_2A03>*>(cpu));
    }
}

void register_rockwell65c02_for_gui(void* cpu) {
    if (cpu) {
        cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<ROCKWELL_R65C02>>(reinterpret_cast<fam65xx_t<ROCKWELL_R65C02>*>(cpu));
    }
}

void register_mos6510_for_gui(void* cpu) {
    if (cpu) {
        cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<MOS6510>>(reinterpret_cast<fam65xx_t<MOS6510>*>(cpu));
    }
}

void register_wdc65c816_for_gui(void* cpu) {
    if (cpu) {
        cpu_renderers[cpu] = std::make_unique<CPUGUIRendererImpl<WDC_65C816>>(reinterpret_cast<fam65xx_t<WDC_65C816>*>(cpu));
    }
}

void unregister_cpu_from_gui(void* cpu) {
    auto it = cpu_renderers.find(cpu);
    if (it != cpu_renderers.end()) {
        cpu_renderers.erase(it);
    }
}

// Simple non-template functions for rendering CPU windows
void render_cpu_debug_window_impl(void* cpu, const char* cpu_name) {
    auto it = cpu_renderers.find(cpu);
    if (it != cpu_renderers.end()) {
        bool show_window = true;
        it->second->render_debug_window(&show_window);
    }
}

void render_cpu_settings_window_impl(void* cpu, const char* cpu_name) {
    auto it = cpu_renderers.find(cpu);
    if (it != cpu_renderers.end()) {
        bool show_window = true;
        it->second->render_settings_window(&show_window);
    }
}

}