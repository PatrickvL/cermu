#pragma once

#include "../../core/emulated_system.h"
#include "c64.h"
#include "c64_config.h"

/**
 * C64 System Wrapper
 * Adapts the existing C64 system to the EmulatedSystem interface
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
    void render_system_menu_items() override;
    void render_debug_windows(void* gui_state) override;
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
    
    // Note: hardware_traits_, config_ (SystemConfiguration), speed_multiplier_,
    // total_cycles_ are now stored in EmulatedSystem base class
};