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
#include "core/signal/audio_port.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/sound/namco_wsg.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/video/namco_video/namco_video.hpp"
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

inline constexpr auto kPacManManifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "Program ROM"},
    Slot<RAMChip>{.base_addr = 0x4000, .size_bytes = 0x0400, .label = "Video RAM"},
    Slot<RAMChip>{.base_addr = 0x4400, .size_bytes = 0x0400, .label = "Color RAM"},
    Slot<RAMChip>{.base_addr = 0x4C00, .size_bytes = 0x0400, .label = "Work RAM"},
    Slot<namco_wsg_t>{.base_addr = 0x5040, .addr_mask = 0xFFE0, .label = "WSG3"},
    // Ports
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr auto kPengoManifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x8000, .label = "Program ROM"},
    Slot<RAMChip>{.base_addr = 0x8000, .size_bytes = 0x0400, .label = "Video RAM"},
    Slot<RAMChip>{.base_addr = 0x8400, .size_bytes = 0x0400, .label = "Color RAM"},
    Slot<RAMChip>{.base_addr = 0x8C00, .size_bytes = 0x0400, .label = "Work RAM"},
    Slot<namco_wsg_t>{.base_addr = 0x9040, .addr_mask = 0xFFE0, .label = "WSG3"},
    // Ports
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kPacManChipCount = decltype(kPacManManifest)::chip_count;
inline constexpr size_t kPengoChipCount = decltype(kPengoManifest)::chip_count;
// BusTraits — selects the correct manifest per game
template<NamcoGame G> struct NamcoBusTraits;

template<> struct NamcoBusTraits<NamcoGame::PacMan> {
    static constexpr const auto& kManifest = kPacManManifest;
    using Spec = ManifestBusSpec<kPacManManifest, 16, 8>;
};

template<> struct NamcoBusTraits<NamcoGame::Pengo> {
    static constexpr const auto& kManifest = kPengoManifest;
    using Spec = ManifestBusSpec<kPengoManifest, 16, 8>;
};

template<typename BSpec>
struct NamcoBoard : Board<BSpec> {
    using ComponentTuple = decltype(kPacManManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A&   z80  = std::get<0>(components_);
    ROMChip&     rom  = std::get<1>(components_);
    RAMChip&     vram = std::get<2>(components_);
    RAMChip&     cram = std::get<3>(components_);
    RAMChip&     wram = std::get<4>(components_);
    namco_wsg_t& wsg  = std::get<5>(components_);

    // Port aliases
    PortCompositeVideo& video_port = std::get<6>(components_);
    PortAudioMono&      audio_port = std::get<7>(components_);

    template<size_t N>
    NamcoBoard(const ChipManifest<N>& m) : Board<BSpec>(m) {}
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

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Chips (value-typed via Board Chips) ────────────────────────────

    // Audio thread — WSG synthesis runs off the emulation thread
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<namco_wsg_t, true>> wsg_adapter_;
    std::unique_ptr<AudioPort> audio_port_;  // Audio signal output

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
    using MainBoard = NamcoBoard<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    NamcoVideo video_gen_;                            // TTL tile renderer with 90° rotation

    // ── I/O state ────────────────────────────────────────────────────────
    uint8_t in0_        = 0xFF;          // Input port 0 (P1 + coins)
    uint8_t in1_        = 0xFF;          // Input port 1 (P2 + start)
    uint8_t dsw1_       = 0xFF;          // DIP switches
    bool    int_enable_ = false;         // VBLANK interrupt enable
    bool    sound_enable_ = false;       // Sound output enable
    bool    flip_screen_ = false;        // Cocktail flip

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = NAMCO_BUS_DEFAULT_STATE;
    uint32_t scanline_      = 0;
    int audio_sample_rate_  = namco_arcade_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t io_tick(bus_state_t pins);   // Handle I/O region ($5xxx/$9xxx)
    bool load_roms();
    void decode_palette();                   // Build palette from palette PROM
    void render_frame();                     // Decode tilemap into indexed framebuffer
};
