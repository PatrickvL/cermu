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
class Chip8System : public EmulatedSystem {
public:
    Chip8System();
    ~Chip8System() override = default;
    
    // EmulatedSystem interface - only implement what's required
    const SystemDescriptor& get_descriptor() const override;
    
    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    
    // System lifecycle
    void reset() override;
    
    // Execution
    void tick() override;
    void run_frame() override;
    
    // File loading
    bool load_file(const char* filepath) override;
    
    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    
    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    
    // State
    uint32_t get_target_fps() const override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    // Auto-detect speed profile from ROM size
    SystemConfiguration detect_optimal_configuration(
        const char* filepath, const uint8_t* data, size_t size) override;

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
    
    // Note: rgba_framebuffer_, rgba_width_, rgba_height_ now in base class
    // Note: total_cycles_, speed_multiplier_ now in base class
    
    // CHIP-8 specific state
    uint32_t cycles_per_frame_;
    bool display_dirty_;             // True when display needs RGBA conversion
    
    // Note: hardware_traits_, config_, current_palette_ now in base class
    
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