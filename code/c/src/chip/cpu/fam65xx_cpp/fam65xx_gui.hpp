#ifndef FAM65XX_GUI_HPP
#define FAM65XX_GUI_HPP

#include <stdint.h>
#include <stdbool.h>
#include "fam65xx.hpp"
#include "cpu_config.hpp"
#include "cpu_defs.hpp"

// Forward declarations for cimgui
extern "C" {
    // ImGui C interface functions (will be included via cimgui.h when compiled)
    void igText(const char* fmt, ...);
    bool igBegin(const char* name, bool* p_open, int flags);
    void igEnd(void);
    bool igButton(const char* label, float size_x, float size_y);
    void igSeparator(void);
    bool igTreeNode_Str(const char* label);
    void igTreePop(void);
    void igSameLine(float offset_from_start_x, float spacing);
    void igTextColored(float col_r, float col_g, float col_b, float col_a, const char* fmt, ...);
    bool igCheckbox(const char* label, bool* v);
    bool igSliderInt(const char* label, int* v, int v_min, int v_max, const char* format);
    void igColumns(int count, const char* id, bool border);
    void igNextColumn(void);
}

namespace fam65xx_cpp {

/**
 * Templated GUI system for 65xx family CPUs
 *
 * This provides a unified GUI interface that works with all CPU configurations:
 * - config_6502 (original NMOS with decimal mode)
 * - config_6510 (C64 CPU with I/O ports)
 * - config_65c02 (CMOS with additional instructions)
 * - config_65c816 (16-bit extended processor)
 *
 * The template system allows configuration-specific features to be displayed
 * while maintaining a consistent interface.
 */
template<typename Config>
class CPUDebugGUI {
public:
    using cpu_type = fam65xx<Config>;
    
    /**
     * Render the main CPU debug window
     * @param cpu CPU instance
     * @param show_window Pointer to window visibility flag
     * @param window_title Title for the debug window
     */
    static void render_debug_window(const cpu_type& cpu,
                                   bool* show_window,
                                   const char* window_title) {
        if (!*show_window) return;
        
        if (igBegin(window_title, show_window, 0)) {
            render_cpu_registers(cpu);
            igSeparator();
            render_cpu_flags(cpu);
            igSeparator();
            render_execution_info(cpu);
            
            // Configuration-specific sections
            if constexpr (Config::has_io_ports) {
                igSeparator();
                render_io_ports(cpu);
            }
            
            if constexpr (Config::wai_instruction || Config::stp_instruction) {
                igSeparator();
                render_cmos_features(cpu);
            }
            
            if constexpr (Config::cpu_variant == CpuVariant::WDC_65C816) {
                igSeparator();
                render_65c816_features(cpu);
            }
            
            igSeparator();
            render_interrupt_status(cpu);
            
            igSeparator();
            render_state_flags(cpu);
        }
        igEnd();
    }
    
    /**
     * Render CPU settings/configuration window
     * @param cpu CPU instance (for settings that can be changed)
     * @param show_window Pointer to window visibility flag
     * @param window_title Title for the settings window
     */
    static void render_settings_window(cpu_type& cpu,
                                      bool* show_window,
                                      const char* window_title) {
        if (!*show_window) return;
        
        if (igBegin(window_title, show_window, 0)) {
            render_configuration_info();
            igSeparator();
            render_debug_controls(cpu);
            
            // Configuration-specific settings
            if constexpr (Config::has_io_ports) {
                igSeparator();
                render_io_settings(cpu);
            }
            
            if constexpr (Config::wai_instruction || Config::stp_instruction) {
                igSeparator();
                render_cmos_settings(cpu);
            }
            
            if constexpr (Config::cpu_variant == CpuVariant::WDC_65C816) {
                igSeparator();
                render_65c816_settings(cpu);
            }
        }
        igEnd();
    }

private:
    // Core register display (common to all configurations)
    static void render_cpu_registers(const cpu_type& cpu) {
        if (igTreeNode_Str("CPU Registers")) {
            igColumns(2, "RegColumns", true);
            
            igText("PC:"); igNextColumn();
            igText("$%04X", cpu.get_pc()); igNextColumn();
            
            igText("A:"); igNextColumn();
            igText("$%02X", cpu.get_a()); igNextColumn();
            
            igText("X:"); igNextColumn();
            igText("$%02X", cpu.get_x()); igNextColumn();
            
            igText("Y:"); igNextColumn();
            igText("$%02X", cpu.get_y()); igNextColumn();
            
            igText("SP:"); igNextColumn();
            igText("$%02X", cpu.get_s()); igNextColumn();
            
            igColumns(1, nullptr, false);
            igTreePop();
        }
    }
    
    // Status flags display (common to all configurations)
    static void render_cpu_flags(const cpu_type& cpu) {
        if (igTreeNode_Str("Status Flags")) {
            igColumns(4, "FlagColumns", true);
            
            uint8_t p = cpu.get_p();
            
            // Use colors to indicate flag states
            float green_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
            float red_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
            
            bool n_flag = (p & P_NEGATIVE) != 0;
            igTextColored(n_flag ? green_color[0] : red_color[0],
                         n_flag ? green_color[1] : red_color[1],
                         n_flag ? green_color[2] : red_color[2],
                         n_flag ? green_color[3] : red_color[3],
                         "N:%d", n_flag ? 1 : 0);
            igNextColumn();
            
            bool v_flag = (p & P_OVERFLOW) != 0;
            igTextColored(v_flag ? green_color[0] : red_color[0],
                         v_flag ? green_color[1] : red_color[1],
                         v_flag ? green_color[2] : red_color[2],
                         v_flag ? green_color[3] : red_color[3],
                         "V:%d", v_flag ? 1 : 0);
            igNextColumn();
            
            bool d_flag = (p & P_DECIMAL) != 0;
            igTextColored(d_flag ? green_color[0] : red_color[0],
                         d_flag ? green_color[1] : red_color[1],
                         d_flag ? green_color[2] : red_color[2],
                         d_flag ? green_color[3] : red_color[3],
                         "D:%d", d_flag ? 1 : 0);
            igNextColumn();
            
            bool i_flag = (p & P_IRQ_DIS) != 0;
            igTextColored(i_flag ? green_color[0] : red_color[0],
                         i_flag ? green_color[1] : red_color[1],
                         i_flag ? green_color[2] : red_color[2],
                         i_flag ? green_color[3] : red_color[3],
                         "I:%d", i_flag ? 1 : 0);
            igNextColumn();
            
            bool z_flag = (p & P_ZERO) != 0;
            igTextColored(z_flag ? green_color[0] : red_color[0],
                         z_flag ? green_color[1] : red_color[1],
                         z_flag ? green_color[2] : red_color[2],
                         z_flag ? green_color[3] : red_color[3],
                         "Z:%d", z_flag ? 1 : 0);
            igNextColumn();
            
            bool c_flag = (p & P_CARRY) != 0;
            igTextColored(c_flag ? green_color[0] : red_color[0],
                         c_flag ? green_color[1] : red_color[1],
                         c_flag ? green_color[2] : red_color[2],
                         c_flag ? green_color[3] : red_color[3],
                         "C:%d", c_flag ? 1 : 0);
            igNextColumn();
            
            igColumns(1, nullptr, false);
            igTreePop();
        }
    }
    
    // Execution information (common to all configurations)
    static void render_execution_info(const cpu_type& cpu) {
        if (igTreeNode_Str("Execution Info")) {
            igText("Current Opcode: $%04X", cpu.get_opcode());
            igText("Cycle Step: %d", cpu.get_cycle_step());
            igText("Address: $%04X", cpu.get_address());
            igText("R/W: %s", cpu.get_rw() ? "READ" : "WRITE");
            if (!cpu.get_rw()) {
                igText("Write Data: $%02X", cpu.get_write_data());
            }
            igTreePop();
        }
    }
    
    // State flags display
    static void render_state_flags(const cpu_type& cpu) {
        if (igTreeNode_Str("Internal State Flags")) {
            uint32_t state = cpu.get_state_flags();
            
            igColumns(2, "StateColumns", true);
            
            igText("Reset Pending:"); igNextColumn();
            igText("%s", (state & STATE_RESET_PENDING) ? "YES" : "NO"); igNextColumn();
            
            igText("Sync Next:"); igNextColumn();
            igText("%s", (state & STATE_SYNC_NEXT) ? "YES" : "NO"); igNextColumn();
            
            igText("Jam State:"); igNextColumn();
            igText("%s", (state & STATE_JAM_STATE) ? "YES" : "NO"); igNextColumn();
            
            igText("Page Crossed:"); igNextColumn();
            igText("%s", (state & STATE_PAGE_CROSSED) ? "YES" : "NO"); igNextColumn();
            
            igText("Branch Taken:"); igNextColumn();
            igText("%s", (state & STATE_BRANCH_TAKEN) ? "YES" : "NO"); igNextColumn();
            
            igText("RDY Wait:"); igNextColumn();
            igText("%s", (state & STATE_RDY_WAIT) ? "YES" : "NO"); igNextColumn();
            
            igText("Interrupt Sequence:"); igNextColumn();
            igText("%s", (state & STATE_INTERRUPT_SEQUENCE) ? "YES" : "NO"); igNextColumn();
            
            if constexpr (Config::wai_instruction) {
                igText("WAI Mode:"); igNextColumn();
                igText("%s", (state & STATE_WAI_MODE) ? "YES" : "NO"); igNextColumn();
            }
            
            if constexpr (Config::stp_instruction) {
                igText("STP Mode:"); igNextColumn();
                igText("%s", (state & STATE_STP_MODE) ? "YES" : "NO"); igNextColumn();
            }
            
            igColumns(1, nullptr, false);
            igTreePop();
        }
    }
    
    // Interrupt status (common to all configurations)
    static void render_interrupt_status(const cpu_type& cpu) {
        if (igTreeNode_Str("Interrupt Status")) {
            uint32_t state = cpu.get_state_flags();
            
            igColumns(2, "IntColumns", true);
            
            igText("NMI Edge:"); igNextColumn();
            igText("%s", (state & STATE_NMI_EDGE) ? "YES" : "NO"); igNextColumn();
            
            igText("NMI Pending:"); igNextColumn();
            igText("%s", (state & STATE_NMI_PENDING) ? "YES" : "NO"); igNextColumn();
            
            igText("IRQ Line:"); igNextColumn();
            igText("%s", (state & STATE_IRQ_LINE) ? "ACTIVE" : "INACTIVE"); igNextColumn();
            
            igText("IRQ Pending:"); igNextColumn();
            igText("%s", (state & STATE_IRQ_PENDING) ? "YES" : "NO"); igNextColumn();
            
            igText("IRQ Masked:"); igNextColumn();
            igText("%s", cpu.is_irq_masked() ? "YES" : "NO"); igNextColumn();
            
            if constexpr (Config::has_abort_pin) {
                igText("ABORT Pending:"); igNextColumn();
                igText("%s", (state & STATE_ABORT_PENDING) ? "YES" : "NO"); igNextColumn();
                
                igText("COP Pending:"); igNextColumn();
                igText("%s", (state & STATE_COP_PENDING) ? "YES" : "NO"); igNextColumn();
            }
            
            igColumns(1, nullptr, false);
            igTreePop();
        }
    }
    
    // MOS6510-specific I/O ports (only for configurations with I/O ports)
    static void render_io_ports(const cpu_type& cpu) {
        if constexpr (Config::has_io_ports) {
            if (igTreeNode_Str("I/O Ports (MOS6510)")) {
                igColumns(2, "IOColumns", true);
                
                // Note: The actual I/O port values would need to be exposed
                // through the CPU interface. For now, show placeholder.
                igText("Port $00 Data:"); igNextColumn();
                igText("N/A (not exposed)"); igNextColumn();
                
                igText("Port $01 DDR:"); igNextColumn();
                igText("N/A (not exposed)"); igNextColumn();
                
                igColumns(1, nullptr, false);
                igTreePop();
            }
        }
    }
    
    // CMOS-specific features
    static void render_cmos_features(const cpu_type& cpu) {
        if constexpr (Config::wai_instruction || Config::stp_instruction) {
            if (igTreeNode_Str("CMOS Features")) {
                uint32_t state = cpu.get_state_flags();
                
                if constexpr (Config::wai_instruction) {
                    igText("WAI State: %s", (state & STATE_WAI_MODE) ? "WAITING" : "NORMAL");
                }
                
                if constexpr (Config::stp_instruction) {
                    igText("STP State: %s", (state & STATE_STP_MODE) ? "STOPPED" : "NORMAL");
                }
                
                igTreePop();
            }
        }
    }
    
    // 65C816-specific 16-bit features
    static void render_65c816_features(const cpu_type& cpu) {
        if constexpr (Config::cpu_variant == CpuVariant::WDC_65C816) {
            if (igTreeNode_Str("16-bit Features (65C816)")) {
                igColumns(2, "C816Columns", true);
                
                uint8_t p = cpu.get_p();
                
                // Note: 65C816 specific registers would need to be exposed
                // through the CPU interface. For now, show what we can.
                igText("M Flag:"); igNextColumn();
                igText("%s", (p & 0x20) ? "8-bit" : "16-bit"); igNextColumn();
                
                igText("X Flag:"); igNextColumn();
                igText("%s", (p & 0x10) ? "8-bit" : "16-bit"); igNextColumn();
                
                igColumns(1, nullptr, false);
                igTreePop();
            }
        }
    }
    
    // Configuration information display
    static void render_configuration_info() {
        if (igTreeNode_Str("CPU Configuration")) {
            igText("Variant: %s", get_variant_name());
            igText("Decimal Mode: %s", Config::has_decimal_mode ? "YES" : "NO");
            igText("I/O Ports: %s", Config::has_io_ports ? "YES" : "NO");
            igText("SYNC Pin: %s", Config::has_sync_pin ? "YES" : "NO");
            igText("SO Pin: %s", Config::has_so_pin ? "YES" : "NO");
            igText("AEC Pin: %s", Config::has_aec_pin ? "YES" : "NO");
            igText("BE Pin: %s", Config::has_be_pin ? "YES" : "NO");
            igText("ABORT Pin: %s", Config::has_abort_pin ? "YES" : "NO");
            igText("VP Pin: %s", Config::has_vp_pin ? "YES" : "NO");
            igText("ML Pin: %s", Config::has_ml_pin ? "YES" : "NO");
            igText("Illegal Opcodes: %s", Config::has_illegal_opcodes ? "YES" : "NO");
            igText("CMOS Fixes: %s", Config::has_cmos_fixes ? "YES" : "NO");
            igText("Address Lines: %d", Config::address_lines);
            igText("RDY Affects Writes: %s", Config::rdy_affects_writes ? "YES" : "NO");
            igText("WAI Instruction: %s", Config::wai_instruction ? "YES" : "NO");
            igText("STP Instruction: %s", Config::stp_instruction ? "YES" : "NO");
            igTreePop();
        }
    }
    
    // Debug controls
    static void render_debug_controls(cpu_type& cpu) {
        if (igTreeNode_Str("Debug Controls")) {
            if (igButton("Reset CPU", 100, 0)) {
                cpu.reset();
            }
            
            igSameLine(0, 10);
            if (igButton("Trigger NMI", 100, 0)) {
                cpu.nmi();
            }
            
            igSameLine(0, 10);
            if (igButton("Assert IRQ", 100, 0)) {
                cpu.irq(false); // IRQ active low
            }
            
            if (igButton("Clear IRQ", 100, 0)) {
                cpu.irq(true); // IRQ inactive high
            }
            
            if constexpr (Config::has_abort_pin) {
                igSameLine(0, 10);
                if (igButton("Trigger ABORT", 100, 0)) {
                    cpu.abort_pin(false); // ABORT active low
                }
            }
            
            igTreePop();
        }
    }
    
    // Configuration-specific settings
    static void render_io_settings(cpu_type& cpu) {
        if constexpr (Config::has_io_ports) {
            if (igTreeNode_Str("I/O Port Settings")) {
                igText("I/O port configuration would go here");
                igText("(Requires extended CPU interface)");
                igTreePop();
            }
        }
    }
    
    static void render_cmos_settings(cpu_type& cpu) {
        if constexpr (Config::wai_instruction || Config::stp_instruction) {
            if (igTreeNode_Str("CMOS Settings")) {
                igText("CMOS-specific settings would go here");
                igText("(WAI/STP control, etc.)");
                igTreePop();
            }
        }
    }
    
    static void render_65c816_settings(cpu_type& cpu) {
        if constexpr (Config::cpu_variant == CpuVariant::WDC_65C816) {
            if (igTreeNode_Str("65C816 Settings")) {
                igText("65C816-specific settings would go here");
                igText("(Memory banking, 16-bit mode, etc.)");
                igTreePop();
            }
        }
    }
    
    // Helper function to get variant name
    static const char* get_variant_name() {
        switch (Config::cpu_variant) {
            case CpuVariant::NMOS_6502: return "NMOS 6502";
            case CpuVariant::NMOS_6510: return "NMOS 6510";
            case CpuVariant::CMOS_65C02: return "CMOS 65C02";
            case CpuVariant::WDC_65C816: return "WDC 65C816";
            default: return "Unknown";
        }
    }
};

// C interface wrappers for each CPU configuration
extern "C" {
    // MOS6502 GUI functions
    void mos6502_render_debug_window(void* cpu, bool* show_window, const char* title);
    void mos6502_render_settings_window(void* cpu, bool* show_window, const char* title);
    
    // MOS6510 GUI functions
    void mos6510_render_debug_window(void* cpu, bool* show_window, const char* title);
    void mos6510_render_settings_window(void* cpu, bool* show_window, const char* title);
    
    // 65C02 GUI functions
    void mos65c02_render_debug_window(void* cpu, bool* show_window, const char* title);
    void mos65c02_render_settings_window(void* cpu, bool* show_window, const char* title);
    
    // 65C816 GUI functions
    void mos65c816_render_debug_window(void* cpu, bool* show_window, const char* title);
    void mos65c816_render_settings_window(void* cpu, bool* show_window, const char* title);
}

} // namespace fam65xx_cpp

#endif // FAM65XX_GUI_HPP