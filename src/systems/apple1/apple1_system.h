#pragma once

#include "../../core/emulated_system.h"
#include "../../core/system_lines.h"
#include "../../core/text_terminal.h"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/io/pia6820.h"
#include <cstdint>

// Apple 1 default bus state — initial pin values.
// RW=1 (read mode), active-low IRQ/RES HIGH (inactive).
// Apple 1 has no NMI line connected.
#define APPLE1_BUS_DEFAULT_STATE \
    (BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_RES_BIT))
#include <memory>

/**
 * Apple 1 System Implementation
 * Clean implementation using the new EmulatedSystem architecture (VIC-20 pattern)
 *
 * The Apple 1 was Steve Wozniak's first computer design (1976)
 * Features:
 * - MOS 6502 CPU @ 1 MHz
 * - 4KB-8KB RAM (typically 8KB)
 * - 40x24 character display (via terminal/video card)
 * - Woz Monitor ROM (256 bytes at $FF00-$FFFF)
 * - Optional BASIC ROM
 */
class Apple1System : public EmulatedSystem {
public:
    Apple1System();
    ~Apple1System() override;
    
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
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;

private:
    // Chip instances
    mos6502_t* cpu_;                 // MOS6502 CPU instance @ 1 MHz
    pia6820_t pia_;                  // PIA 6820 for keyboard and display I/O
    TextTerminal* terminal_;         // Text terminal (40x24)
    
    // Apple 1 Memory (simple arrays)
    uint8_t ram_simple_[65536];      // Up to 64KB RAM (typically 8KB at $0000-$1FFF)
    uint8_t monitor_rom_[256];       // Woz Monitor ROM at $FF00-$FFFF
    uint8_t basic_rom_[4096];        // Optional Apple 1 BASIC (4KB at various addresses)
    uint8_t char_rom_[512];          // Signetics 2513 character ROM (64 chars x 8 bytes)
    
    // System state
    uint32_t cycles_per_frame_;
    uint32_t ram_size_;              // Configured RAM size (4KB or 8KB)
    bool has_basic_;                 // Whether BASIC ROM is loaded
    int cursor_col_;                 // Terminal cursor position
    int cursor_row_;
    bus_state_t pins_;               // CPU bus state (persisted across ticks)

    // Helper methods
    void tick_cpu();
    bus_state_t mem_tick(bus_state_t s);

    // Connector port setup (registers Apple 1 connector ports with base class)
    void setup_connector_ports();

    /// Register all Apple 1 chips into registered_chips_ for the Hardware menu.
    void register_apple1_chips();

    // ROM loading
    bool load_roms();

    // PIA callbacks
    static uint8_t pia_keyboard_read(void* user_data);  // Port A read (keyboard)
    static void pia_display_write(void* user_data, uint8_t data);  // Port B write (display)
    
    // Apple 1 keyboard helpers (PIA Port A specific)
    void set_keyboard_data(uint8_t key_code);   // Sets bit 7 strobe + ASCII in bits 0-6
    bool keyboard_ready() const;                 // Checks if bit 7 is set
    void clear_keyboard_strobe();                // Clears bit 7 (called after read)
    
    // Display helpers
    void display_char(uint8_t ch);
    void convert_2513_to_8x8_font(const uint8_t* char_rom, uint8_t* font_8x8);
};