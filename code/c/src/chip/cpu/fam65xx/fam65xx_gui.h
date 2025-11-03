#ifndef FAM65XX_GUI_H
#define FAM65XX_GUI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations for FAM65XX CPU family debug windows
// These are C-compatible wrappers around the C++ template implementation
void fam65xx_render_debug_window(void* chip, bool* show_window);
void fam65xx_render_settings_window(void* chip, bool* show_window);

#ifdef __cplusplus
}

#include <unordered_map>
#include <memory>

// Forward declarations
namespace fam65xx {
    template<const CPUTraits& Traits> class fam65xx_t;
    struct CPUTraits;
    class CPUGUIRenderer;
}

// C++ template interface for registering CPU instances with the GUI system
namespace fam65xx {
    // Register a CPU instance for GUI rendering
    template<const CPUTraits& Traits>
    void register_cpu_for_gui(fam65xx_t<Traits>* cpu);
    
    // Unregister a CPU instance 
    void unregister_cpu_from_gui(void* cpu);
    
    // Non-template registration functions for specific CPU types
    void register_mos6502_for_gui(void* cpu);
    void register_mos6510_for_gui(void* cpu);
    void register_nes6502_for_gui(void* cpu);
    void register_rockwell65c02_for_gui(void* cpu);
    void register_wdc65c816_for_gui(void* cpu);
    

    
    // Non-template function for rendering CPU windows
    void render_cpu_debug_window_impl(void* cpu, const char* cpu_name);
    void render_cpu_settings_window_impl(void* cpu, const char* cpu_name);
}
#endif

#endif // FAM65XX_GUI_H