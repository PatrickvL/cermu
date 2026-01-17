#pragma once

#include "../../core/emulated_system.h"
#include "../../core/framebuffer_renderer.h"
#include <cstdint>
#include <cstring>

/**
 * CHIP-8 Emulator System
 * Simple interpreter-based system, perfect for validating the multi-system architecture
 *
 * Uses native 1-bit monochrome display with configurable palette
 * Memory efficient: 256 bytes for native display vs 8192 bytes for RGBA
 */
class Chip8System : public IEmulatedSystem {
public:
    Chip8System();
    ~Chip8System() override = default;
    
    // IEmulatedSystem interface
    const SystemDescriptor& get_descriptor() const override;
    
    // Configuration management
    const SystemConfiguration& get_configuration() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    
    // Hardware trait queries
    const HardwareTraits& get_hardware_traits() const override;
    const SystemTiming& get_current_timing() const override;
    const DisplayTraits& get_display_traits() const override;
    const AudioTraits& get_audio_traits() const override;
    
    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    
    // Execution
    void tick() override;
    void run_frame() override;
    
    // File loading
    bool load_file(const char* filepath) override;
    
    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
    
    // Input
    void handle_keyboard_event(int key, bool pressed) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_debug_windows(void* gui_state) override;
    void render_configuration_ui() override;
    
    // State
    uint64_t get_total_cycles() const override;
    uint32_t get_target_fps() const override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;
    float get_speed_multiplier() const override;
    
private:
    // CHIP-8 hardware state
    uint8_t memory_[4096];           // 4KB RAM
    uint8_t V_[16];                  // 16 8-bit registers (V0-VF)
    uint16_t I_;                     // 16-bit index register
    uint16_t PC_;                    // Program counter
    uint16_t stack_[16];             // Stack
    uint8_t SP_;                     // Stack pointer
    uint8_t delay_timer_;            // Delay timer (60Hz)
    uint8_t sound_timer_;            // Sound timer (60Hz)
    
    // Native 1-bit display buffer (256 bytes for 64x32 display)
    // Packed format: 8 pixels per byte, MSB first
    // 64 pixels/row = 8 bytes/row, 32 rows = 256 bytes total
    uint8_t native_display_[64 * 32 / 8];
    
    uint8_t keys_[16];               // 16-key keypad state
    
    // RGBA conversion buffer (provided by GUI, not owned)
    uint32_t* rgba_framebuffer_;
    int rgba_width_;
    int rgba_height_;
    
    // Emulation state
    uint64_t total_cycles_;
    float speed_multiplier_;
    uint32_t cycles_per_frame_;
    bool display_dirty_;             // True when display needs RGBA conversion
    
    // Configuration
    HardwareTraits hardware_traits_;
    SystemConfiguration config_;
    std::vector<PaletteColor> current_palette_;
    
    // Quirks/settings
    bool shift_quirk_;               // Original CHIP-8 shift behavior
    bool load_store_quirk_;          // Original CHIP-8 I register behavior
    
    // Execution
    void execute_instruction(uint16_t opcode);
    void update_timers();
    void render_display();
    
    // Key mapping (SDL keycode to CHIP-8 key)
    int map_sdl_key_to_chip8(int sdl_key);
};