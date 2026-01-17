    #pragma once

#include "../../core/emulated_system.h"
#include "c64.h"
#include "c64_config.h"

/**
 * C64 System Wrapper
 * Adapts the existing C64 system to the IEmulatedSystem interface
 */
class C64SystemWrapper : public IEmulatedSystem {
public:
    C64SystemWrapper();
    ~C64SystemWrapper() override;
    
    // IEmulatedSystem interface
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
    uint64_t get_total_cycles() const override;
    uint32_t get_target_fps() const override;
    void set_speed_multiplier(float multiplier) override;
    float get_speed_multiplier() const override;
    
    // Hardware traits interface
    const HardwareTraits& get_hardware_traits() const override;
    const SystemTiming& get_current_timing() const override;
    const DisplayTraits& get_display_traits() const override;
    const AudioTraits& get_audio_traits() const override;
    
    // Configuration interface
    const SystemConfiguration& get_configuration() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    void render_configuration_ui() override;
    
    // Get the underlying C64 system (for compatibility with existing GUI code)
    c64_t* get_c64_system() { return c64_; }
    
private:
    c64_t* c64_;
    float speed_multiplier_;
    uint32_t cycles_per_frame_;
    c64_config_t config_;
    
    // Hardware traits (initialized in constructor)
    HardwareTraits hardware_traits_;
    SystemConfiguration system_config_;
};