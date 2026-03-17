#pragma once
/*
 * namco_arcade_system.h — Namco Pac-Man / Pengo arcade system declaration
 *
 * Templated on NamcoGame enum — Pac-Man and Pengo share the same board
 * with minor ROM layout and memory map differences.
 */


#include "systems/arcade/namco/namco_arcade_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/sound/namco_wsg.hpp"
#include "chip/memory/memory_chip.hpp"
#include "core/audio_thread.hpp"
#include "utils/write_only_synth_adapter.hpp"
#include <cstdint>
#include <memory>
#include <vector>

#define NAMCO_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// ── Game variants ────────────────────────────────────────────────────────
enum class NamcoGame { PacMan, Pengo };

template<NamcoGame G> struct NamcoGameTraits;

template<> struct NamcoGameTraits<NamcoGame::PacMan> {
    static constexpr const char* name            = "Pac-Man";
    static constexpr const char* short_name      = "PacMan";
    static constexpr const char* description     = "Namco Pac-Man — Z80A @ 3.072MHz, WSG3 sound, 224×288 (1980)";
    static constexpr uint32_t    rom_size        = namco_arcade_constants::PACMAN_ROM_SIZE;
    static constexpr uint32_t    char_rom_size   = namco_arcade_constants::PACMAN_CHAR_ROM_SIZE;
    static constexpr bool        encrypted_roms  = false;
    static constexpr int         wsg_voices      = 3;
    // Memory layout — Pac-Man starts at $4000
    static constexpr uint16_t    vram_base       = 0x4000;
    static constexpr uint16_t    cram_base       = 0x4400;
    static constexpr uint16_t    wram_base       = 0x4C00;
    static constexpr uint16_t    io_base         = 0x5000;
};

template<> struct NamcoGameTraits<NamcoGame::Pengo> {
    static constexpr const char* name            = "Pengo";
    static constexpr const char* short_name      = "Pengo";
    static constexpr const char* description     = "Sega/Coreland Pengo — Z80A @ 3.072MHz, WSG3 sound, 224×288 (1982)";
    static constexpr uint32_t    rom_size        = namco_arcade_constants::PENGO_ROM_SIZE;
    static constexpr uint32_t    char_rom_size   = namco_arcade_constants::PENGO_CHAR_ROM_SIZE;
    static constexpr bool        encrypted_roms  = true;
    static constexpr int         wsg_voices      = 3;
    // Memory layout — Pengo starts at $8000
    static constexpr uint16_t    vram_base       = 0x8000;
    static constexpr uint16_t    cram_base       = 0x8400;
    static constexpr uint16_t    wram_base       = 0x8C00;
    static constexpr uint16_t    io_base         = 0x9000;
};

// ============================================================================
// Namco chip manifests — declarative memory layout
// ============================================================================
//
// Pac-Man ($0000-$3FFF ROM, $4000/$4400/$4C00 work areas, $5xxx I/O):
//   Slot 0: ROM       — 16 KB at $0000     (read-only)
//   Slot 1: Video RAM —  1 KB at $4000
//   Slot 2: Color RAM —  1 KB at $4400
//   Slot 3: Work RAM  —  1 KB at $4C00
//
// Pengo ($0000-$7FFF ROM, $8000/$8400/$8C00 work areas, $9xxx I/O):
//   Slot 0: ROM       — 32 KB at $0000     (read-only)
//   Slot 1: Video RAM —  1 KB at $8000
//   Slot 2: Color RAM —  1 KB at $8400
//   Slot 3: Work RAM  —  1 KB at $8C00
//
// I/O registers at $5000/$9000 are memory-mapped but handled separately
// (asymmetric read/write behavior: reads → input ports, writes → control regs).
// Graphics ROMs (char, sprite, palette, waveform) are NOT bus-mapped.
//
inline constexpr auto kPacManChips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 16384, 0, "Program ROM"},
    Slot<RAMChip>{0x4000,  1024, 0, "Video RAM"},
    Slot<RAMChip>{0x4400,  1024, 0, "Color RAM"},
    Slot<RAMChip>{0x4C00,  1024, 0, "Work RAM"},
    // Non-bus chip — factory-created, not address-decoded
    Slot<ZilogZ80A>{0, 0, 0, "Z80A"}
);

inline constexpr auto kPengoChips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "Program ROM"},
    Slot<RAMChip>{0x8000,  1024, 0, "Video RAM"},
    Slot<RAMChip>{0x8400,  1024, 0, "Color RAM"},
    Slot<RAMChip>{0x8C00,  1024, 0, "Work RAM"},
    // Non-bus chip — factory-created, not address-decoded
    Slot<ZilogZ80A>{0, 0, 0, "Z80A"}
);

// BusTraits — selects the correct manifest per game
template<NamcoGame G> struct NamcoBusTraits;

template<> struct NamcoBusTraits<NamcoGame::PacMan> {
    static constexpr const auto& kManifest = kPacManChips;
    using Spec = ManifestBusSpec<kPacManChips, 16, 8>;
};

template<> struct NamcoBusTraits<NamcoGame::Pengo> {
    static constexpr const auto& kManifest = kPengoChips;
    using Spec = ManifestBusSpec<kPengoChips, 16, 8>;
};

// ── System ───────────────────────────────────────────────────────────────
template<NamcoGame G>
class NamcoArcadeSystem : public System {
    using Traits = NamcoGameTraits<G>;
public:
    NamcoArcadeSystem();
    ~NamcoArcadeSystem() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    void get_display_dimensions(int* width, int* height) const override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    ZilogZ80A*       cpu_ = nullptr;     // Z80A @ 3.072 MHz — owned by board_
    namco_wsg_t      wsg_;               // Namco WSG3 wavetable sound

    // Audio thread — WSG synthesis runs off the emulation thread
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<namco_wsg_t, true>> wsg_adapter_;

    // ── Graphics ROM — NOT bus-mapped (display rendering only) ───────────
    std::vector<uint8_t> char_rom_;
    std::vector<uint8_t> sprite_rom_;
    std::vector<uint8_t> palette_prom_;
    std::vector<uint8_t> colortable_prom_;
    std::vector<uint8_t> waveform_rom_;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = NamcoBusTraits<G>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;

    // ── Memory chips (cached for hot-path rendering) ────────────
    RAMChip* vram_chip_ = nullptr;  // Video RAM (tile indices)
    RAMChip* cram_chip_ = nullptr;  // Color RAM (palette attributes)

    // ── I/O state ────────────────────────────────────────────────────────
    uint8_t in0_        = 0xFF;          // Input port 0 (P1 + coins)
    uint8_t in1_        = 0xFF;          // Input port 1 (P2 + start)
    uint8_t dsw1_       = 0xFF;          // DIP switches
    bool    int_enable_ = false;         // VBLANK interrupt enable
    bool    sound_enable_ = false;       // Sound output enable
    bool    flip_screen_ = false;        // Cocktail flip

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = NAMCO_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint32_t scanline_      = 0;
    int audio_sample_rate_  = namco_arcade_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t io_tick(bus_state_t pins);   // Handle I/O region ($5xxx/$9xxx)
    bool load_roms();
    void decode_palette();                   // Build palette from palette PROM
    void render_frame();                     // Decode tilemap into indexed framebuffer
};
