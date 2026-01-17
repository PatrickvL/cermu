#pragma once

#include "../../core/emulated_system.h"
#include "c64.h"
#include "c64_config.h"

/**
 * C64 System Wrapper
 * Adapts the existing C64 system to the EmulatedSystem interface
 *
 * FUTURE REFACTORING PLAN:
 * ========================
 * This wrapper should eventually be merged into a single unified C64System class.
 * The current design has two layers:
 *
 * 1. C64SystemWrapper (C++ class, EmulatedSystem interface)
 * 2. c64_t struct (C-style, internal implementation)
 *
 * MERGER STRATEGY:
 * ----------------
 * The unified C64System class should:
 *
 * 1. **Direct Member Integration**: Convert c64_t members to C64System class members
 *    - system_8bit_t system → keep as compatibility layer or remove
 *    - c64_bus_t bus → make it a direct member (not embedded)
 *    - All chip pointers (vicii, sid, cia1, cia2, keyboard, etc.) → direct members
 *    - total_cycles → already in base class as total_cycles_
 *
 * 2. **Method Conversion**: Convert C functions to C64System methods
 *    - c64_system_tick() → void tick() override
 *    - c64_system_reset() → void reset() override
 *    - c64_system_create() → constructor logic
 *    - c64_system_destroy() → destructor logic
 *    - c64_set_framebuffer() → void set_framebuffer() override
 *
 * 3. **Bus Integration**: The c64_bus_t should become a nested class or direct member
 *    - Keep bus cycle-accurate interface
 *    - Maintain unified memory buffer architecture
 *    - Preserve PLA banking system
 *
 * 4. **Chip Management**: Use smart pointers for chip ownership
 *    - std::unique_ptr<vicii_t> vicii_;
 *    - std::unique_ptr<mos6581_t> sid_;
 *    - etc.
 *
 * 5. **Configuration**: Merge c64_config_t into SystemConfiguration
 *    - Map VIC standard to timing region
 *    - Map ROM paths to configuration system
 *
 * BENEFITS OF MERGER:
 * -------------------
 * - Single unified type (no wrapper indirection)
 * - Consistent with CHIP-8System design pattern
 * - Better C++ resource management (RAII)
 * - Cleaner API without dual-layer access
 * - Eliminates c64_ pointer indirection
 *
 * CHALLENGES:
 * -----------
 * - Large codebase (~850 lines in c64.cpp to convert)
 * - Complex bus architecture with cycle-accurate timing
 * - Many C-style functions to convert to methods
 * - Extensive chip interaction code
 * - Need to maintain compatibility with existing test harnesses
 *
 * MIGRATION PATH:
 * ---------------
 * 1. Keep wrapper working (current state)
 * 2. Gradually move functionality into wrapper
 * 3. Convert C functions to static methods
 * 4. Make c64_t members direct C64System members
 * 5. Remove wrapper layer entirely
 * 6. Rename C64SystemWrapper → C64System
 */
class C64SystemWrapper : public EmulatedSystem {
public:
    C64SystemWrapper();
    ~C64SystemWrapper() override;
    
    // EmulatedSystem interface - system-specific overrides
    const SystemDescriptor& get_descriptor() const override;
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    void tick() override;
    void run_frame() override;
    bool load_file(const char* filepath) override;
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
    void handle_keyboard_event(int key, bool pressed) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // GUI forwarding methods - delegate to old C64 GUI code (imgui_interface.cpp)
    void render_system_menu_items() override;      // Forwards to gui_render_c64_system_menu_items()
    void render_debug_windows(void* gui_state) override;  // Forwards to chip debug system
    
    uint32_t get_target_fps() const override;
    void set_speed_multiplier(float multiplier) override;
    
    // Configuration interface
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    void render_configuration_ui() override;
    
    // Note: The following methods are now provided by EmulatedSystem base class:
    // - get_configuration() - returns config_
    // - get_hardware_traits() - returns hardware_traits_
    // - get_current_timing() - returns hardware_traits_.timing
    // - get_display_traits() - returns hardware_traits_.display
    // - get_audio_traits() - returns hardware_traits_.audio
    // - get_total_cycles() - returns total_cycles_
    // - get_speed_multiplier() - returns speed_multiplier_
    
    // Get the underlying C64 system (for compatibility with existing GUI code)
    c64_t* get_c64_system() { return c64_; }
    
private:
    c64_t* c64_;
    uint32_t cycles_per_frame_;
    c64_config_t c64_config_;  // Renamed to avoid conflict with base class config_
    void* gui_state_;  // Opaque pointer to gui_state_t (persistent GUI state)
    
    // Note: hardware_traits_, config_ (SystemConfiguration), speed_multiplier_,
    // total_cycles_ are now stored in EmulatedSystem base class
};