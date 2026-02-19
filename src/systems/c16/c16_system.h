#ifndef C16_SYSTEM_H
#define C16_SYSTEM_H

#include "../../core/emulated_system.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/keyboard_mapper.h"
#include <cstdint>
#include <memory>

/**
 * C16 System - Commodore 16 / Plus/4 Emulator
 *
 * Direct EmulatedSystem implementation for the Commodore 16 and Plus/4.
 *
 * NOTE: This system currently uses a legacy C struct internally because
 * the actual chip implementations (MOS7501 CPU, TED 7360) don't exist yet.
 * Once those chips are implemented, this can be refactored to use real
 * chip instances like the VIC-20 system.
 */
class C16System : public EmulatedSystem {
public:
    C16System();
    ~C16System() override;
    
    // System identification
    const SystemDescriptor& get_descriptor() const override;
    
    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    
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
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) override;
    void handle_text_input(const char* text) override;
    void release_all_keys() override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    
    // State
    uint32_t get_target_fps() const override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    // Hardware traits
    static HardwareTraits create_hardware_traits();
    static float can_load_file_static(const char* filepath, const uint8_t* data, size_t size);
    static const SystemDescriptor c16_descriptor;

private:
    // Chip instances (TODO: Implement MOS7501 CPU and TED 7360 chips)
    void* mos7501_;              // MOS7501 CPU (TODO: Create proper chip type)
    void* ted_;                  // TED 7360 ($FD00-$FEFF, 4KB) - TODO: Create proper chip type
    commodore_keyboard_t* keyboard_;  // Keyboard matrix (8×8, scanned via TED)
    std::unique_ptr<KeyboardMapper> keyboard_mapper_; // Layered keyboard mapping engine
    
    // Memory arrays (simplified storage like VIC-20)
    uint8_t ram_simple_[65536];  // Up to 64KB RAM (C16 uses 16KB, Plus/4 uses 64KB)
    uint8_t basic_rom_[16384];   // BASIC ROM $8000-$BFFF (16KB)
    uint8_t kernal_rom_[16384];  // Kernal ROM $C000-$FFFF (16KB)
    
    // System state
    uint32_t cycles_per_frame_;
    bool initialized_;
    
    // Helper methods
    bool load_roms();
    uint8_t cpu_read(uint32_t addr);
    void cpu_write(uint32_t addr, uint8_t data);
    
    // Connector port setup (registers C16/Plus4 connector ports with base class)
    void setup_connector_ports();
    
    // Static callbacks for CPU
    static uint8_t cpu_read_callback(void* user_data, uint32_t addr, uint8_t bus_state);
    static void cpu_write_callback(void* user_data, uint32_t addr, uint8_t data);
};

#endif // C16_SYSTEM_H