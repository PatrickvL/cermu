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

// C++ template interface for registering CPU instances with the GUI system
namespace fam65xx {
    // Register a CPU instance for GUI rendering
    template<const CPUTraits& Traits>
    void register_cpu_for_gui(fam65xx_t<Traits>* cpu);
    
    // Unregister a CPU instance 
    void unregister_cpu_from_gui(void* cpu);
}
#endif

#endif // FAM65XX_GUI_H