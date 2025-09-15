#include "fam65xx_gui.hpp"
#include "cpu_config.hpp"

namespace fam65xx_cpp {

// C interface wrapper implementations for each CPU configuration

extern "C" {

/**
 * MOS6502 GUI Functions
 */
void mos6502_render_debug_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_6502>*>(cpu);
    CPUDebugGUI<config_6502>::render_debug_window(*cpu_instance, show_window, title ? title : "MOS6502 Debug");
}

void mos6502_render_settings_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_6502>*>(cpu);
    CPUDebugGUI<config_6502>::render_settings_window(*cpu_instance, show_window, title ? title : "MOS6502 Settings");
}

/**
 * MOS6510 GUI Functions
 */
void mos6510_render_debug_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_6510>*>(cpu);
    CPUDebugGUI<config_6510>::render_debug_window(*cpu_instance, show_window, title ? title : "MOS6510 Debug");
}

void mos6510_render_settings_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_6510>*>(cpu);
    CPUDebugGUI<config_6510>::render_settings_window(*cpu_instance, show_window, title ? title : "MOS6510 Settings");
}

/**
 * 65C02 GUI Functions
 */
void mos65c02_render_debug_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_65c02>*>(cpu);
    CPUDebugGUI<config_65c02>::render_debug_window(*cpu_instance, show_window, title ? title : "65C02 Debug");
}

void mos65c02_render_settings_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_65c02>*>(cpu);
    CPUDebugGUI<config_65c02>::render_settings_window(*cpu_instance, show_window, title ? title : "65C02 Settings");
}

/**
 * 65C816 GUI Functions
 */
void mos65c816_render_debug_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_65c816>*>(cpu);
    CPUDebugGUI<config_65c816>::render_debug_window(*cpu_instance, show_window, title ? title : "65C816 Debug");
}

void mos65c816_render_settings_window(void* cpu, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    auto* cpu_instance = static_cast<fam65xx<config_65c816>*>(cpu);
    CPUDebugGUI<config_65c816>::render_settings_window(*cpu_instance, show_window, title ? title : "65C816 Settings");
}

/**
 * Generic CPU GUI dispatch function
 * 
 * This function can be used when the CPU variant is determined at runtime.
 * It checks the CPU configuration and dispatches to the appropriate
 * templated GUI function.
 */
void fam65xx_render_debug_window_generic(void* cpu, int cpu_variant, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    switch (cpu_variant) {
        case 0: // NMOS_6502
            mos6502_render_debug_window(cpu, show_window, title);
            break;
        case 1: // NMOS_6510
            mos6510_render_debug_window(cpu, show_window, title);
            break;
        case 2: // CMOS_65C02
            mos65c02_render_debug_window(cpu, show_window, title);
            break;
        case 3: // WDC_65C816
            mos65c816_render_debug_window(cpu, show_window, title);
            break;
        default:
            // Unknown variant, default to 6502
            mos6502_render_debug_window(cpu, show_window, title);
            break;
    }
}

void fam65xx_render_settings_window_generic(void* cpu, int cpu_variant, bool* show_window, const char* title) {
    if (!cpu || !show_window) return;
    
    switch (cpu_variant) {
        case 0: // NMOS_6502
            mos6502_render_settings_window(cpu, show_window, title);
            break;
        case 1: // NMOS_6510
            mos6510_render_settings_window(cpu, show_window, title);
            break;
        case 2: // CMOS_65C02
            mos65c02_render_settings_window(cpu, show_window, title);
            break;
        case 3: // WDC_65C816
            mos65c816_render_settings_window(cpu, show_window, title);
            break;
        default:
            // Unknown variant, default to 6502
            mos6502_render_settings_window(cpu, show_window, title);
            break;
    }
}

/**
 * Helper function to get CPU variant name as string
 */
const char* fam65xx_get_variant_name(int cpu_variant) {
    switch (cpu_variant) {
        case 0: return "NMOS 6502";
        case 1: return "NMOS 6510";
        case 2: return "CMOS 65C02";
        case 3: return "WDC 65C816";
        default: return "Unknown";
    }
}

/**
 * Helper function to check if a variant supports specific features
 */
bool fam65xx_variant_has_feature(int cpu_variant, int feature) {
    // Feature flags:
    // 0: Decimal mode
    // 1: I/O ports
    // 2: SYNC pin
    // 3: SO pin
    // 4: Illegal opcodes
    // 5: CMOS fixes
    // 6: WAI instruction
    // 7: STP instruction
    // 8: ABORT pin
    
    switch (cpu_variant) {
        case 0: // NMOS_6502
            switch (feature) {
                case 0: return config_6502::has_decimal_mode;
                case 1: return config_6502::has_io_ports;
                case 2: return config_6502::has_sync_pin;
                case 3: return config_6502::has_so_pin;
                case 4: return config_6502::has_illegal_opcodes;
                case 5: return config_6502::has_cmos_fixes;
                case 6: return config_6502::wai_instruction;
                case 7: return config_6502::stp_instruction;
                case 8: return config_6502::has_abort_pin;
                default: return false;
            }
        case 1: // NMOS_6510
            switch (feature) {
                case 0: return config_6510::has_decimal_mode;
                case 1: return config_6510::has_io_ports;
                case 2: return config_6510::has_sync_pin;
                case 3: return config_6510::has_so_pin;
                case 4: return config_6510::has_illegal_opcodes;
                case 5: return config_6510::has_cmos_fixes;
                case 6: return config_6510::wai_instruction;
                case 7: return config_6510::stp_instruction;
                case 8: return config_6510::has_abort_pin;
                default: return false;
            }
        case 2: // CMOS_65C02
            switch (feature) {
                case 0: return config_65c02::has_decimal_mode;
                case 1: return config_65c02::has_io_ports;
                case 2: return config_65c02::has_sync_pin;
                case 3: return config_65c02::has_so_pin;
                case 4: return config_65c02::has_illegal_opcodes;
                case 5: return config_65c02::has_cmos_fixes;
                case 6: return config_65c02::wai_instruction;
                case 7: return config_65c02::stp_instruction;
                case 8: return config_65c02::has_abort_pin;
                default: return false;
            }
        case 3: // WDC_65C816
            switch (feature) {
                case 0: return config_65c816::has_decimal_mode;
                case 1: return config_65c816::has_io_ports;
                case 2: return config_65c816::has_sync_pin;
                case 3: return config_65c816::has_so_pin;
                case 4: return config_65c816::has_illegal_opcodes;
                case 5: return config_65c816::has_cmos_fixes;
                case 6: return config_65c816::wai_instruction;
                case 7: return config_65c816::stp_instruction;
                case 8: return config_65c816::has_abort_pin;
                default: return false;
            }
        default:
            return false;
    }
}

} // extern "C"

} // namespace fam65xx_cpp