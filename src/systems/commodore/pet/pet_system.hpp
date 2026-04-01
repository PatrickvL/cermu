#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "systems/commodore/pet/pet_constants.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/io/pia6820.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/video/fam6845/mc6845.hpp"
#include "chip/memory/memory_chip.hpp"

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
//   Slot 1: Screen RAM     —  1 KB at $8000  (mirrored at $8400 via addr_mask)
//   Slot 2: BASIC ROM low  —  4 KB at $B000  (901465-23)
//   Slot 3: BASIC ROM mid  —  4 KB at $C000  (901465-20)
//   Slot 4: BASIC ROM high —  4 KB at $D000  (901465-21)
//   Slot 5: Editor ROM     —  2 KB at $E000
//   Slot 6: Kernal ROM     —  4 KB at $F000
//
// I/O at $E800–$E8FF handled separately (PIA1, PIA2, VIA, CRTC).
// Character ROM is NOT bus-mapped.
//
//                                ctx   type       chip           base    size    mask    ovl  label              rom
#define PET_FOR_EACH_SYSTEM_CHIP(V, ctx) \
    V(ctx, MOS6502,   m6502,        0,            0, 0,      0, "MOS 6502",        nullptr) \
    V(ctx, RAMChip,   main_ram,     0x0000,  32768, 0,      0, "Main RAM",        nullptr) \
    V(ctx, RAMChip,   screen_ram,   0x8000,   2048, 0x03FF, 0, "Screen RAM",      nullptr) \
    V(ctx, ROMChip,   basic_rom_b,  0xB000,   4096, 0,      0, "BASIC ROM $B000", "basic-4.901465-23-20-21.bin@0|901465-23.bin|basic-b.rom") \
    V(ctx, ROMChip,   basic_rom_c,  0xC000,   4096, 0,      0, "BASIC ROM $C000", "basic-4.901465-23-20-21.bin@4096|901465-20.bin|basic-c.rom") \
    V(ctx, ROMChip,   basic_rom_d,  0xD000,   4096, 0,      0, "BASIC ROM $D000", "basic-4.901465-23-20-21.bin@8192|901465-21.bin|basic-d.rom") \
    V(ctx, ROMChip,   editor_rom,   0xE000,   2048, 0,      0, "Editor ROM",      "edit-4-40-n-50Hz.901498-01.bin|edit-4-40-n-60Hz.901499-01.bin|editor.rom|901498-01.bin|901499-01.bin") \
    V(ctx, ROMChip,   kernal_rom,   0xF000,   4096, 0,      0, "Kernal ROM",      "kernal-4.901465-22.bin|kernal4.rom|kernal.rom|901465-22.bin") \
    V(ctx, mc6845_t,  crtc,         0xE880,       0, 0xFFF0, 0, "MC6845 CRTC",     nullptr) \
    V(ctx, mos6520_t, pia1,         0xE810,       0, 0xFFF0, 0, "PIA 1 (Keyboard)", nullptr) \
    V(ctx, mos6520_t, pia2,         0xE820,       0, 0xFFF0, 0, "PIA 2 (IEEE-488)", nullptr) \
    V(ctx, mos6522_t, via,          0xE840,       0, 0xFFF0, 0, "MOS 6522 VIA",    nullptr)

static constexpr size_t kPETChipCount = 0 PET_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kPETChipCount> kPETChips = {{
    PET_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

struct PETBusTraits {
    static constexpr const auto& kManifest = kPETChips;
    using Spec = ManifestBusSpec<kPETChips, 16, 8, 1, true>;
};

// Value-typed chips: all chips are fields via X-macro.
struct PETChipset {
    PET_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
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

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Audio — PET CB2 speaker (simple square wave)
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Bus ──────────────────────────────────────────────────────────────
    using Bus    = MemoryBus<PETBusTraits::Spec>;
    using MainBoard = Board<PETBusTraits::Spec, PETChipset>;
    Bus    bus_;
    MainBoard board_{kPETChips};
    bus_state_t pins_ = PET_BUS_DEFAULT_STATE;

    // ── Memory chips (owned by registered_chips_, managed via Board) ─
    RAMChip* main_ram_chip_      = nullptr;  // 32 KB main RAM
    RAMChip* screen_ram_chip_    = nullptr;  // 1 KB screen RAM
    ROMChip* basic_rom_b_chip_   = nullptr;  // 4 KB BASIC $B000
    ROMChip* basic_rom_c_chip_   = nullptr;  // 4 KB BASIC $C000
    ROMChip* basic_rom_d_chip_   = nullptr;  // 4 KB BASIC $D000
    ROMChip* editor_rom_chip_    = nullptr;  // 2 KB Editor ROM
    ROMChip* kernal_rom_chip_    = nullptr;  // 4 KB Kernal ROM

    // Character ROM (loaded separately, not mapped directly in address space for display)
    uint8_t char_rom_[4096] = {};

    // Chip instances — all owned by board_ (manifest non-bus slots)
    MOS6502*    cpu_  = nullptr;    // MOS 6502 CPU @ 1 MHz
    mos6520_t*  pia1_ = nullptr;    // PIA 1 — keyboard matrix + cassette sense
    mos6520_t*  pia2_ = nullptr;    // PIA 2 — IEEE-488 bus interface
    mos6522_t*  via_  = nullptr;    // VIA — user port, timers, CB2 speaker
    mc6845_t*   crtc_ = nullptr;    // MC6845 CRTC — display timing

    // Display — pixel buffer rendered by CRTC, streamed via video port
    uint8_t pixel_buffer_[pet_constants::DISPLAY_WIDTH * pet_constants::DISPLAY_HEIGHT] = {};
    std::unique_ptr<CompositeVideoPort> video_port_;

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
