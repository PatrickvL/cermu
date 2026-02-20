#ifndef C16_SYSTEM_H
#define C16_SYSTEM_H

#include "../../core/emulated_system.h"
#include "../../core/system_lines.h"
#include "../../chip/cpu/fam65xx/mos7501.h"
#include "../../chip/video/ted/ted7360.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/keyboard_mapper.h"
#include <cstdint>
#include <memory>

/**
 * C16 System - Commodore 16 / Plus/4 Emulator
 *
 * Direct EmulatedSystem implementation for the Commodore 16 and Plus/4.
 * The Plus/4 is treated as a variant of the C16 (like Famicom is to NES):
 * same CPU (MOS 7501), same video/sound chip (TED 7360), same BASIC 3.5,
 * but the Plus/4 has 64KB RAM, a User Port, and built-in "3-PLUS-1"
 * productivity software in additional ROM.
 *
 * NOTE: This system currently uses a legacy C struct internally because
 * the actual chip implementations (MOS7501 CPU, TED 7360) don't exist yet.
 * Once those chips are implemented, this can be refactored to use real
 * chip instances like the VIC-20 system.
 */
class C16System : public EmulatedSystem {
public:
    /// Variant identifier for the TED-based system family
    enum class Variant { C16, C116, PLUS4 };
    
    C16System(Variant variant = Variant::C16);
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
    static HardwareTraits create_hardware_traits(bool is_plus4);
    static float can_load_file_static(const char* filepath, const uint8_t* data, size_t size);

private:
    // Variant flag
    Variant variant_;
    bool is_plus4_;
    const char* system_name_;   // "C16", "C116", or "Plus/4" — used for logging
    // Chip instances
    mos7501_t* cpu_;              // MOS 7501/8501 CPU
    ted7360_t* ted_;             // TED 7360 (video, sound, I/O, timers)
    bus_state_t bus_state_;       // Current bus state for CPU cycle
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
    bus_state_t mem_tick(bus_state_t s);  // Service CPU memory bus access
    
    // Connector port setup (registers C16/Plus4 connector ports with base class)
    void setup_connector_ports();
    
    // Static callbacks for CPU
    static uint8_t cpu_read_callback(void* user_data, uint32_t addr, uint8_t bus_state);
    static void cpu_write_callback(void* user_data, uint32_t addr, uint8_t data);
    
    // MOS 7501 I/O port callbacks (cassette motor, serial bus, etc.)
    static uint8_t io_port_in(void* user_data);
    static void io_port_out(uint8_t data, void* user_data);
    
    // TED keyboard scan callback
    static uint8_t ted_keyboard_scan(void* user_data, uint8_t column);
    
    // TED memory read callback (for TED's own character/bitmap/screen fetches)
    static uint8_t ted_mem_read(void* user_data, uint16_t address);
    
    // Commodore load helper: set CPU PC
    static void set_cpu_pc(void* user_data, uint16_t addr);
};

#endif // C16_SYSTEM_H