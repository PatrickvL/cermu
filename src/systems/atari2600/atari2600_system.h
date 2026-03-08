#pragma once
/*
 * atari2600_system.h — Atari 2600 (VCS) Emulated System
 *
 * The Atari 2600 (1977) was the first widely successful home video game
 * console. It features:
 *   - MOS 6507 CPU (6502 core, 13-bit address bus, no IRQ pin)
 *   - TIA chip (video + audio generation, collision detection)
 *   - PIA 6532 RIOT (128 bytes RAM, I/O ports, timer)
 *   - ROM cartridge slot (2KB–32KB+ with bank switching)
 *   - Two DB-9 joystick ports (active-low, same pinout as Commodore)
 *   - Console switches: Reset, Select, Color/B&W, Difficulty A/B
 *
 * Clock: TIA runs at 3.579545 MHz (NTSC); CPU divides by 3 ≈ 1.19 MHz.
 * Display: 160×~192 visible pixels, 128-color NTSC palette.
 */

#include "../../core/emulated_system.h"
#include "../../chip/cpu/fam65xx/mos6507.h"
#include "../../chip/video/tia/tia.h"
#include "../../chip/io/pia6532.h"
#include "mappers/a2600_mapper.h"
#include "atari2600_constants.h"
#include <cstdint>
#include <memory>
#include <vector>

// Atari 2600 default bus state — derived from MOS6507 CPU.
#define ATARI2600_BUS_DEFAULT_STATE (MOS6507::default_bus_state())

class Atari2600System : public EmulatedSystem {
public:
    Atari2600System();
    ~Atari2600System() override;

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

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    // System ready state
    bool is_system_ready() const override { return system_ready_; }

private:
    // ========================================================================
    // CHIPS
    // ========================================================================

    MOS6507*    cpu_ = nullptr;     // MOS 6507 CPU (6502, 13-bit address bus)
    tia_t       tia_;               // TIA — Television Interface Adapter
    pia6532_t   riot_;              // PIA 6532 RIOT — RAM, I/O, Timer

    // ========================================================================
    // CARTRIDGE ROM
    // ========================================================================

    std::vector<uint8_t> cart_rom_;         // Cartridge ROM data
    uint32_t cart_size_ = 0;                // Actual ROM size in bytes
    std::unique_ptr<A2600Mapper> mapper_;   // Bank-switching mapper
    bool mapper_snoop_ = false;             // Cached: mapper needs bus_snoop() calls

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;                   // CPU bus state (persisted across ticks)
    bool system_ready_ = false;          // True when cartridge is loaded
    uint32_t cycles_per_frame_;          // CPU cycles per video frame

    // Console switch state (directly mapped to RIOT Port B)
    uint8_t console_switches_ = 0xFF;   // All switches default high (not pressed)

    // Joystick state (directly mapped to RIOT Port A)
    // Each nibble controls one joystick: P0=upper nibble, P1=lower nibble
    // Bits: 4=P0-UP, 5=P0-DOWN, 6=P0-LEFT, 7=P0-RIGHT (active-low)
    //       0=P1-UP, 1=P1-DOWN, 2=P1-LEFT, 3=P1-RIGHT (active-low)
    uint8_t joystick_state_ = 0xFF;     // All directions released (active-low)

    // ========================================================================
    // FRAME DETECTION
    // ========================================================================

    bool    frame_complete_ = false;
    bool    in_vsync_ = false;           // Tracks VSYNC transitions for frame boundary
    int     vsync_scanline_ = 0;         // Scanline where VSYNC started

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    void tick_cpu();
    bus_state_t mem_tick(bus_state_t s);

    // Connector port setup
    void setup_connector_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    // Read joystick signals from connector ports into RIOT/TIA
    void update_joystick_state();


};
