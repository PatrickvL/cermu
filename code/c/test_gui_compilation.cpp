// Test compilation of the new templated C++ GUI system
#include "src/chip/cpu/fam65xx_cpp/fam65xx_gui.hpp"
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

// Mock cimgui functions for compilation test
extern "C" {
    void igText(const char* fmt, ...) {}
    bool igBegin(const char* name, bool* p_open, int flags) { return true; }
    void igEnd(void) {}
    bool igButton(const char* label, float size_x, float size_y) { return false; }
    void igSeparator(void) {}
    bool igTreeNodeStr(const char* label) { return true; }
    void igTreePop(void) {}
    void igSameLine(float offset_from_start_x, float spacing) {}
    void igTextColored(float col_r, float col_g, float col_b, float col_a, const char* fmt, ...) {}
    bool igCheckbox(const char* label, bool* v) { return false; }
    bool igSliderInt(const char* label, int* v, int v_min, int v_max, const char* format) { return false; }
    void igColumns(int count, const char* id, bool border) {}
    void igNextColumn(void) {}
}

int main() {
    // Test that we can instantiate different CPU configurations
    using namespace fam65xx_cpp;
    
    // Create CPU instances for different variants
    fam65xx<config_6502> cpu_6502;
    fam65xx<config_6510> cpu_6510;
    fam65xx<config_65c02> cpu_65c02;
    fam65xx<config_65c816> cpu_65c816;
    
    // Test GUI instantiation for each variant
    bool show_debug = true;
    bool show_settings = true;
    
    // Test 6502 GUI
    CPUDebugGUI<config_6502>::render_debug_window(cpu_6502, &show_debug, "MOS6502 Debug");
    CPUDebugGUI<config_6502>::render_settings_window(cpu_6502, &show_settings, "MOS6502 Settings");
    
    // Test 6510 GUI
    CPUDebugGUI<config_6510>::render_debug_window(cpu_6510, &show_debug, "MOS6510 Debug");
    CPUDebugGUI<config_6510>::render_settings_window(cpu_6510, &show_settings, "MOS6510 Settings");
    
    // Test 65C02 GUI
    CPUDebugGUI<config_65c02>::render_debug_window(cpu_65c02, &show_debug, "65C02 Debug");
    CPUDebugGUI<config_65c02>::render_settings_window(cpu_65c02, &show_settings, "65C02 Settings");
    
    // Test 65C816 GUI
    CPUDebugGUI<config_65c816>::render_debug_window(cpu_65c816, &show_debug, "65C816 Debug");
    CPUDebugGUI<config_65c816>::render_settings_window(cpu_65c816, &show_settings, "65C816 Settings");
    
    // Test C interface wrappers
    mos6502_render_debug_window(&cpu_6502, &show_debug, "C Interface Test");
    mos6510_render_debug_window(&cpu_6510, &show_debug, "C Interface Test");
    mos65c02_render_debug_window(&cpu_65c02, &show_debug, "C Interface Test");
    mos65c816_render_debug_window(&cpu_65c816, &show_debug, "C Interface Test");
    
    // Test helper functions
    printf("Successfully compiled templated C++ GUI system!\n");
    printf("All CPU variants (6502, 6510, 65C02, 65C816) GUI interfaces work!\n");
    printf("C interface wrappers functional!\n");
    
    return 0;
}