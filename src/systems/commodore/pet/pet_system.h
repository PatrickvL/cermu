#pragma once

#include "../commodore_system.h"
#include "pet_bus.h"
#include "pet_constants.h"
#include "../../../chip/cpu/fam65xx/mos6502.h"
#include "../../../chip/io/pia6820.h"
#include "../../../chip/io/mos6522.h"
#include "../../../chip/video/mc6845/mc6845.h"

#include <cstdint>
#include <string>

/**
 * Commodore PET System Implementation
 *
 * The PET (Personal Electronic Transactor) was Commodore's first complete
 * personal computer, introduced in 1977.  All-in-one unit with integrated
 * keyboard, monitor, and cassette drive.
 *
 * Features:
 * - MOS 6502 CPU @ 1 MHz
 * - 8KB–32KB RAM (model-dependent; default: 32KB)
 * - 40×25 character display (monochrome green phosphor)
 * - MC6845 CRTC for display timing
 * - 2× PIA 6820 (keyboard + IEEE-488) + 1× MOS 6522 VIA (timers + CB2 speaker)
 * - Built-in keyboard (graphics or business variant)
 *
 * Memory Map (PET 4032 — 32KB, BASIC 4.0):
 * $0000–$7FFF  32KB  RAM
 * $8000–$83FF  1KB   Screen RAM (40×25 = 1000 chars used)
 * $8400–$87FF  1KB   Screen RAM mirror
 * $8800–$8FFF  2KB   Unmapped
 * $9000–$9FFF  4KB   Unmapped
 * $A000–$AFFF  4KB   Expansion ROM socket 1
 * $B000–$BFFF  4KB   Expansion ROM socket 2
 * $C000–$DFFF  8KB   BASIC 4.0 ROM
 * $E000–$E7FF  2KB   Editor ROM (40-col normal keyboard variant)
 * $E800–$E8FF  256B  I/O (PIA1, PIA2, VIA, CRTC — mirrored within page)
 * $E900–$EFFF  ~2KB  Unmapped (expansion I/O)
 * $F000–$FFFF  4KB   KERNAL ROM
 */
class PETSystem : public CommodoreSystem {
public:
    PETSystem();
    ~PETSystem() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration management
    bool apply_configuration() override;

    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Audio — PET CB2 speaker (simple square wave)
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

private:
    pet_bus_t bus_;

    // Unified 64KB memory buffer — flat, no banking
    uint8_t* memory_ = nullptr;

    // Character ROM (loaded separately, not mapped directly in address space for display)
    uint8_t char_rom_[4096] = {};

    // Chip instances
    MOS6502*    cpu_  = nullptr;    // MOS 6502 CPU @ 1 MHz
    pia6820_t*  pia1_ = nullptr;    // PIA 1 — keyboard matrix + cassette sense
    pia6820_t*  pia2_ = nullptr;    // PIA 2 — IEEE-488 bus interface
    mos6522_t*  via_  = nullptr;    // VIA — user port, timers, CB2 speaker
    mc6845_t*   crtc_ = nullptr;    // MC6845 CRTC — display timing

    // Display state
    uint32_t screen_pixel_x_ = 0;   // Current pixel X position in framebuffer
    uint32_t screen_pixel_y_ = 0;   // Current pixel Y position in framebuffer

    // Audio state — CB2 square wave speaker
    bool     speaker_state_ = false;    // Current CB2 output level
    float    audio_buffer_[4096] = {};  // Ring buffer for audio samples
    uint32_t audio_write_pos_ = 0;     // Write position in ring buffer
    uint32_t audio_read_pos_ = 0;      // Read position in ring buffer
    int      audio_sample_rate_ = pet_constants::AUDIO_SAMPLE_RATE;
    uint32_t audio_cycle_counter_ = 0; // Cycles since last audio sample
    uint32_t audio_cycles_per_sample_ = 0;  // CPU cycles per audio sample

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    void inject_keys(const char* str) override;
    bool is_system_initialized() const override { return memory_ != nullptr && cpu_ != nullptr; }

    // ROM loading
    bool load_roms();

    // Memory access for CPU
    bus_state_t mem_tick(bus_state_t s);

    // I/O dispatch ($E800-$E8FF)
    uint8_t io_read(uint16_t addr);
    void io_write(uint16_t addr, uint8_t data);

    // PIA1 callbacks — keyboard matrix scanning
    static uint8_t pia1_port_a_read(void* user_data);
    static void    pia1_port_a_write(void* user_data, uint8_t data);
    static uint8_t pia1_port_b_read(void* user_data);
    static void    pia1_port_b_write(void* user_data, uint8_t data);

    // VIA CB2 callback — speaker output
    static void via_cb2_output(void* user_data, bool state);

    // CRTC display callback — renders one character cell
    void crtc_display_char(uint16_t ma, uint8_t ra, bool cursor);
    void crtc_vsync();
    void crtc_hsync();

    // Keyboard row select state (written by PIA1 Port A output)
    uint8_t keyboard_row_select_ = 0;

    /// Register all PET chips into registered_chips_ for the Hardware menu.
    void register_pet_chips();
};
