#pragma once
/*
 * acorn_atom_system.h — Acorn Atom system declaration
 *
 * Acorn Atom (1980) — simple 6502-based British home computer.
 *   CPU:    MOS 6502 @ 1 MHz
 *   Video:  Motorola MC6847 VDG (text 32×16, graphics 256×192)
 *   I/O:    Intel 8255 PPI (keyboard matrix scanning)
 *   I/O:    MOS 6522 VIA (cassette, printer, timers)
 *   Memory: 2KB–12KB RAM, 8KB BASIC ROM, 2KB FP ROM, 4KB OS ROM
 *   Video RAM at $8000–$97FF (6KB)
 *
 * Memory map:
 *   $0000–$09FF : Zero page, stack, user RAM (2KB base)
 *   $0A00–$7FFF : Expansion RAM (optional, up to ~30KB)
 *   $8000–$97FF : Video RAM (6KB, directly accessed by MC6847)
 *   $9800–$9FFF : Unused / optional
 *   $A000–$AFFF : Utility ROM area
 *   $B000–$B003 : Intel 8255 PPI
 *   $B800–$B80F : MOS 6522 VIA
 *   $C000–$CFFF : Atom BASIC ROM
 *   $D000–$D7FF : Floating point ROM
 *   $E000–$EFFF : ROM extension area
 *   $F000–$FFFF : OS ROM (Atom monitor)
 */


#include "acorn_atom_constants.h"
#include "../../core/emulated_system.h"
#include "../../core/system_lines.h"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/video/mc6847/mc6847.h"
#include "../../chip/io/i8255.h"
#include "../../chip/io/mos6522.h"
#include <cstdint>
#include <vector>

// Default bus state for the Atom — inherited from MOS 6502 defaults.
#define ATOM_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

class AcornAtomSystem : public EmulatedSystem {
public:
    AcornAtomSystem();
    ~AcornAtomSystem() override;

    // ── EmulatedSystem interface ─────────────────────────────────────────
    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    MOS6502*    cpu_  = nullptr;     // MOS 6502 @ 1 MHz
    mc6847_t    vdg_;                // MC6847 Video Display Generator
    i8255_t     ppi_;                // Intel 8255 PPI (keyboard + cassette ctrl)
    mos6522_t   via_;                // MOS 6522 VIA (timers, cassette, printer)

    // ── Memory ───────────────────────────────────────────────────────────
    std::vector<uint8_t> ram_;       // $0000–$2BFF (up to 11KB)
    std::vector<uint8_t> video_ram_; // $8000–$97FF (6KB)
    std::vector<uint8_t> basic_rom_; // $C000–$CFFF (4KB BASIC)
    std::vector<uint8_t> fp_rom_;    // $D000–$D7FF (2KB floating point)
    std::vector<uint8_t> os_rom_;    // $F000–$FFFF (4KB OS)

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[acorn_atom_constants::FB_WIDTH *
                          acorn_atom_constants::FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[acorn_atom_constants::KEYBOARD_ROWS] = {};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_      = ATOM_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_ = 0;
    uint32_t ram_size_kb_  = 2;          // configurable: 2, 5, 8, 12
    int audio_sample_rate_ = acorn_atom_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t mem_tick(bus_state_t pins);
    bus_state_t io_tick(bus_state_t pins);
    void        render_frame();   // Render one complete video frame to framebuffer_
    bool        load_roms();
};
