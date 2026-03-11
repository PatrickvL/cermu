#pragma once

#include "../commodore_system.h"
#include "pet_constants.h"
#include "../../../core/chip_manifest.hpp"
#include "../../../chip/cpu/fam65xx/mos6502.h"
#include "../../../chip/io/pia6820.h"
#include "../../../chip/io/mos6522.h"
#include "../../../chip/video/mc6845/mc6845.h"
#include "../../../chip/memory/memory_chip.h"

#include <cstdint>
#include <string>

#define PET_BUS_DEFAULT_STATE \
    (MOS6502::default_bus_state() | BUS_DATA_MASK)

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

// ============================================================================
// PET chip manifest — declarative memory layout
// ============================================================================
//
// Memory-mapped chips:
//   Slot 0: Main RAM       — 32 KB at $0000
//   Slot 1: Screen RAM     —  1 KB at $8000  (mirrored at $8400 post-apply)
//   Slot 2: BASIC ROM low  —  4 KB at $B000  (901465-23)
//   Slot 3: BASIC ROM mid  —  4 KB at $C000  (901465-20)
//   Slot 4: BASIC ROM high —  4 KB at $D000  (901465-21)
//   Slot 5: Editor ROM     —  2 KB at $E000
//   Slot 6: Kernal ROM     —  4 KB at $F000
//
// I/O at $E800–$E8FF handled separately (PIA1, PIA2, VIA, CRTC).
// Character ROM is NOT bus-mapped.
//
inline constexpr auto kPETChips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 32768},        // Slot 0: Main RAM 32 KB
    Slot<RAMChip>{0x8000,  1024},        // Slot 1: Screen RAM 1 KB
    Slot<ROMChip>{0xB000,  4096},        // Slot 2: BASIC ROM $B000
    Slot<ROMChip>{0xC000,  4096},        // Slot 3: BASIC ROM $C000
    Slot<ROMChip>{0xD000,  4096},        // Slot 4: BASIC ROM $D000
    Slot<ROMChip>{0xE000,  2048},        // Slot 5: Editor ROM
    Slot<ROMChip>{0xF000,  4096}         // Slot 6: Kernal ROM
);

struct PETBusTraits {
    static constexpr const auto& kManifest = kPETChips;
    using Spec = ManifestBusSpec<kPETChips, 16, 8>;
};

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
    // ── Bus ──────────────────────────────────────────────────────────────
    using Bus    = MemoryBus<PETBusTraits::Spec>;
    using BusMem = BusMemory<PETBusTraits::Spec>;
    Bus    bus_;
    BusMem bus_mem_{kPETChips};
    bus_state_t pins_ = PET_BUS_DEFAULT_STATE;

    // ── Memory chips (owned by registered_chips_, managed via BusMemory) ─
    RAMChip* main_ram_chip_      = nullptr;  // 32 KB main RAM
    RAMChip* screen_ram_chip_    = nullptr;  // 1 KB screen RAM
    ROMChip* basic_rom_b_chip_   = nullptr;  // 4 KB BASIC $B000
    ROMChip* basic_rom_c_chip_   = nullptr;  // 4 KB BASIC $C000
    ROMChip* basic_rom_d_chip_   = nullptr;  // 4 KB BASIC $D000
    ROMChip* editor_rom_chip_    = nullptr;  // 2 KB Editor ROM
    ROMChip* kernal_rom_chip_    = nullptr;  // 4 KB Kernal ROM

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
    bool is_system_initialized() const override { return main_ram_chip_ != nullptr && cpu_ != nullptr; }

    // Static callbacks for CommodoreSystem load context
    static uint8_t load_mem_read(void* ctx, uint16_t addr);
    static void    load_mem_write(void* ctx, uint16_t addr, uint8_t val);

    // ROM loading
    bool load_roms();

    // Configure memory map (screen RAM mirror, etc.)
    void configure_memory_map();

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
};
