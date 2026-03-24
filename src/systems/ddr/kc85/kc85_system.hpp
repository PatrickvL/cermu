#pragma once
/*
 * kc85_system.h — KC 85/2, /3, /4 system declaration
 *
 * Three variants, templated:
 *   KC85_2 (HC 900) — 16 KB RAM, no BASIC ROM, CAOS 2.2 (1984)
 *   KC85_3          — 16 KB RAM, built-in BASIC ROM, CAOS 3.1 (1986)
 *   KC85_4          — 64 KB RAM, extended video (2 planes), CAOS 4.2 (1989)
 */


#include "systems/ddr/kc85/kc85_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/core_chips.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"

#include "chip/cpu/z80/u880.hpp"
#include "chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/io/kc85_module_system.hpp"
#include "chip/input/keyboard_encoder.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/video/kc85_video/kc85_video.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <vector>

#define KC85_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class KC85Variant { KC85_2, KC85_3, KC85_4 };

// ============================================================================
// KC 85 chip manifests — declarative memory layout
// ============================================================================
//
// KC85/2 (3 slots):
//   Slot 0: RAM       — 16 KB at $0000         (always mapped)
//   Slot 1: IRM       — 16 KB at $8000         (pixel + color interleaved)
//   Slot 2: CAOS ROM  —  8 KB at $E000         (OS ROM)
//
// KC85/3 (4 slots):
//   Slot 0: RAM       — 16 KB at $0000         (always mapped)
//   Slot 1: IRM       — 16 KB at $8000         (pixel + color interleaved)
//   Slot 2: BASIC ROM —  8 KB at $C000
//   Slot 3: CAOS ROM  —  8 KB at $E000
//
// KC85/4 (5 bus slots):
//   Slot 0: RAM        — 32 KB at $0000        ($0000-$7FFF, base; $4000+ via port $86)
//   Slot 1: IRM        — 64 KB at $8000        (4 banks: pixel0/pixel1/color0/color1)
//   Slot 2: BASIC ROM  —  8 KB at $C000        (overlay group 1)
//   Slot 3: CAOS-C ROM —  4 KB at $C000        (overlay group 3, port $86 bit 7)
//   Slot 4: CAOS ROM   —  8 KB at $E000        (overlay group 2)
//
// KC85/4 IRM bank layout (64 KB, 256 pages):
//   Pages   0- 63: Pixel RAM plane 0
//   Pages  64-127: Pixel RAM plane 1
//   Pages 128-191: Color RAM plane 0
//   Pages 192-255: Color RAM plane 1
//
// Banking (all variants):
//   IRM at $8000-$BFFF: enabled/disabled via PIO B bit 2
//   BASIC ROM at $C000-$DFFF: enabled/disabled via PIO B bit 6 (KC85/3,/4)
//   CAOS ROM at $E000-$FFFF: enabled/disabled via PIO B bit 0
//   KC85/4 IRM bank: selected via port $84 bits 0-1
//
inline constexpr auto kKC852Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 16384, 0, "RAM"},
    Slot<RAMChip>{0x8000, 16384, 0, "IRM",      0, 0, 1},  // overlay group 1
    Slot<ROMChip>{0xE000,  8192, 0, "CAOS ROM", 0, 0, 2}.with_rom("caos.rom|CAOS.ROM"),  // overlay group 2
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>                {0, 0, 0, "U880"},
    Slot<z80_pio_t>           {0, 0, 0, "U855 PIO #1"},
    Slot<z80_pio_t>           {0, 0, 0, "U855 PIO #2"},
    Slot<z80_ctc_t>           {0, 0, 0, "U857 CTC"},
    Slot<kc85_module_system_t>{0, 0, 0, "Module System"}
);

inline constexpr auto kKC853Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 16384, 0, "RAM"},
    Slot<RAMChip>{0x8000, 16384, 0, "IRM",       0, 0, 1},  // overlay group 1
    Slot<ROMChip>{0xC000,  8192, 0, "BASIC ROM", 0, 0, 2}.with_rom("basic.rom|BASIC.ROM"),  // overlay group 2
    Slot<ROMChip>{0xE000,  8192, 0, "CAOS ROM",  0, 0, 3}.with_rom("caos.rom|CAOS.ROM"),  // overlay group 3
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>                {0, 0, 0, "U880"},
    Slot<z80_pio_t>           {0, 0, 0, "U855 PIO #1"},
    Slot<z80_pio_t>           {0, 0, 0, "U855 PIO #2"},
    Slot<z80_ctc_t>           {0, 0, 0, "U857 CTC"},
    Slot<kc85_module_system_t>{0, 0, 0, "Module System"}
);

inline constexpr auto kKC854Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 32768, 0, "RAM"},
    Slot<RAMChip>{0x8000, 65536, 0, "IRM", 0, 16384, 0, 16384}, // 4 × 16 KB banks, 1 visible
    Slot<ROMChip>{0xC000,  8192, 0, "BASIC ROM",  0, 0, 1}.with_rom("basic.rom|BASIC.ROM"),   // overlay group 1
    Slot<ROMChip>{0xC000,  4096, 0, "CAOS-C ROM", 0, 0, 3}.with_rom("caos_c.rom|CAOS_C.ROM", true),  // overlay group 3, optional
    Slot<ROMChip>{0xE000,  8192, 0, "CAOS ROM",   0, 0, 2}.with_rom("caos.rom|CAOS.ROM"),   // overlay group 2
    // Non-bus chips — factory-created, not address-decoded
    Slot<U880>                {0, 0, 0, "U880"},
    Slot<z80_pio_t>           {0, 0, 0, "U855 PIO #1"},
    Slot<z80_pio_t>           {0, 0, 0, "U855 PIO #2"},
    Slot<z80_ctc_t>           {0, 0, 0, "U857 CTC"},
    Slot<kc85_module_system_t>{0, 0, 0, "Module System"}
);

// ── Variant traits ───────────────────────────────────────────────────────
template<KC85Variant V> struct KC85VariantTraits;

template<> struct KC85VariantTraits<KC85Variant::KC85_2> {
    static constexpr const char* name            = "KC 85/2";
    static constexpr const char* short_name      = "KC85/2";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/2 (HC 900) — U880 @ 1.77MHz, 16KB RAM, CAOS 2.2 (1984)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
    static constexpr bool        has_extended_video = false;
    static constexpr const char* caos_version    = "2.2";
};

template<> struct KC85VariantTraits<KC85Variant::KC85_3> {
    static constexpr const char* name            = "KC 85/3";
    static constexpr const char* short_name      = "KC85/3";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/3 — U880 @ 1.77MHz, 16KB RAM, BASIC, CAOS 3.1 (1986)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = true;
    static constexpr bool        has_extended_video = false;
    static constexpr const char* caos_version    = "3.1";
};

template<> struct KC85VariantTraits<KC85Variant::KC85_4> {
    static constexpr const char* name            = "KC 85/4";
    static constexpr const char* short_name      = "KC85/4";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/4 — U880 @ 1.77MHz, 64KB RAM, dual-plane video, CAOS 4.2 (1989)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_64K;
    static constexpr bool        has_basic_rom   = true;
    static constexpr bool        has_extended_video = true;
    static constexpr const char* caos_version    = "4.2";
};

// BusTraits — selects the correct manifest per variant
template<KC85Variant V> struct KC85BusTraits;

template<> struct KC85BusTraits<KC85Variant::KC85_2> {
    static constexpr const auto& kManifest = kKC852Chips;
    using Spec = ManifestBusSpec<kKC852Chips, 16, 8>;
};

template<> struct KC85BusTraits<KC85Variant::KC85_3> {
    static constexpr const auto& kManifest = kKC853Chips;
    using Spec = ManifestBusSpec<kKC853Chips, 16, 8>;
};

template<> struct KC85BusTraits<KC85Variant::KC85_4> {
    static constexpr const auto& kManifest = kKC854Chips;
    using Spec = ManifestBusSpec<kKC854Chips, 16, 8>;
};

// ── Chips ──────────────────────────────────────────────────────────────
struct KC85Chipset : CoreChips<U880, NoChip, NoChip, z80_pio_t> {
    z80_pio_t pio2;     // U855 PIO (module system)
    z80_ctc_t ctc;      // U857 CTC (timing + sound + tape)

    template<typename BoardT>
    void bind_extras(BoardT& board) {
        board.bind_chip(board.template find_index<z80_pio_t>(1), &pio2);
        board.bind_chip(board.template find_index<z80_ctc_t>(), &ctc);
    }

    template<typename BoardT>
    void register_extras(BoardT& board) {
        board.register_component(&pio2);
        board.register_component(&ctc);
    }
};

// ── Keyboard emulation modes ─────────────────────────────────────────────────
enum class KC85KeyboardMode {
    MEMORY_INJECT,   // Patch keycodes directly into CAOS OS variables (fast, default)
    SERIAL_PIO,      // Emulate serial keyboard encoder via PIO-B interrupts (accurate, TODO)
};

// ── System ───────────────────────────────────────────────────────────────
template<KC85Variant V>
class KC85System : public System {
    using Traits = KC85VariantTraits<V>;
public:
    KC85System();
    ~KC85System() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_configuration_ui() override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Chips (value-typed via Board Chips) ────────────────────────────
    kc85_module_system_t* modules_ = nullptr; // Expansion module slot controller — owned by board_

    // ── Memory — owned by Board, accessed via chip_as<>() ────────────
    ROMChip* basic_rom_chip_ = nullptr;   // KC85/3, /4 only
    ROMChip* caos_rom_chip_  = nullptr;
    RAMChip* irm_chip_       = nullptr;   // Image RAM (video memory)

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = KC85BusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec, KC85Chipset>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output
    KC85VideoGenerator video_gen_;                    // Video generation circuitry

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[kc85_constants::KEYBOARD_ROWS] = {};
    uint8_t cur_key_code_   = 0;     // Most recently pressed KC85 key code
    uint32_t key_sticky_count_ = 0;  // Frames remaining for key "hold"
    KC85KeyboardMode kbd_mode_ = KC85KeyboardMode::MEMORY_INJECT;
    KeyboardEncoder kbd_encoder_;    // U807 serial keyboard encoder (SERIAL_PIO mode)

    // Reverse KTAB: keycode → scancode lookup (built from CAOS ROM at runtime)
    uint8_t reverse_ktab_[256] = {};
    bool    ktab_valid_ = false;

    // ── Banking state ────────────────────────────────────────────────────
    uint8_t bank_ctrl_     = 0;      // Port $84 value (KC85/4 only)
    uint8_t bank_ctrl2_    = 0;      // Port $86 value (KC85/4 only)
    bool    irm_enabled_   = false;  // PIO B bit 2: video RAM access enabled
    bool    caos_rom_on_   = true;   // PIO B bit 0: CAOS ROM enabled
    bool    basic_rom_on_  = false;  // PIO B bit 6: BASIC ROM enabled
    uint8_t active_plane_  = 0;      // Display plane (KC85/4: 0 or 1)
    bool    blink_flag_    = true;   // Toggled by CTC CH2 zero-count; drives foreground blink

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = KC85_BUS_DEFAULT_STATE;
    int audio_sample_rate_  = kc85_constants::DEFAULT_SAMPLE_RATE;

    // ── Video timing counters ────────────────────────────────────────────
    // h_count_ ticks every CPU cycle; v_count_ increments each scanline.
    // CTC ch2 external trigger fires on vertical sync (v_count_ wrap).
    uint32_t h_count_ = 0;
    uint32_t v_count_ = 0;

    // ── Audio (CTC-driven square-wave beepers) ───────────────────────────
    // CTC channel 0 → beeper 1. CTC channel 1 → beeper 2.
    // Each zero-count toggles the respective beeper output.
    bool     beeper1_state_        = false;
    bool     beeper2_state_        = false;
    float    audio_volume_         = 0.5f;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_  = 0;   // CPU ticks per audio sample
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;  // Audio signal output

    // ── Internal helpers ─────────────────────────────────────────────────
    void        configure_bus_memory_map();   // Initial banking setup after apply()
    void        update_bank_state();          // Remap pages on PIO B / port write
    void        apply_banking();              // Load overlay snapshot + manual IRM
    bus_state_t io_tick(bus_state_t pins);
    bool        load_roms();
    void        render_frame();               // Decode IRM into indexed framebuffer
    void        handle_keyboard();            // Patch keycode into CAOS via IX register
    void        build_reverse_ktab();         // Build keycode→scancode from CAOS KTAB

    // Pre-computed overlay snapshots.
    // KC85/2: 4 modes (IRM×CAOS), KC85/3: 8 modes (IRM×BASIC×CAOS),
    // KC85/4: 8 modes (BASIC×CAOS-E×CAOS-C, IRM+RAM4 stay manual).
    static constexpr size_t kNumOverlayModes = BT::kManifest.overlay_mode_count();
    std::array<std::array<typename Bus::Snapshot, kNumOverlayModes>, 1> snapshots_;
};
