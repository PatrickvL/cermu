#pragma once
/*
 * bombjack_system.h — Bomb Jack arcade system declaration
 *
 * Tehkan Bomb Jack (1984) — dual Z80 arcade board.
 *   Main CPU:   Z80A @ 4 MHz (gameplay, video)
 *   Sound CPU:  Z80A @ 3 MHz (driven by sound latch commands)
 *   Sound:      3× AY-3-8910 PSG
 *   Video:      256×224, background + tilemap + sprites
 *   Inputs:     2 players (joystick + 2 buttons), coins, DIP switches
 */


#include "systems/arcade/bombjack/bombjack_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/typed_manifest.hpp"
#include "core/typed_port.hpp"
#include "core/signal/audio_port.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/video/bombjack_video/bombjack_video.hpp"
#include "core/dip_switch.hpp"
#include "core/audio_thread.hpp"
#include "utils/write_only_synth_adapter.hpp"
#include <cstdint>
#include <memory>

#define BOMBJACK_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// ============================================================================
// Bomb Jack manifests — dual-board, dual-CPU arcade hardware
// ============================================================================
//
// Physical topology (Tehkan 1984):
//   Top board:  Main Z80A @ 4 MHz, all video TTL, SRAM, EPROMs 01–08 (suffix t),
//               resistor ladder DACs, 18-pin edge connector, 6-pin RGB header.
//   Bottom board: Sound Z80A @ 3.072 MHz, 3× AY-3-8910 @ 1.536 MHz,
//               EPROMs 09–13 (main CPU program ROMs, suffix b),
//               EPROMs 14–16 (sprite ROMs, suffix b, accessed via ribbon cable).
//   Inter-board ribbon cable bridges the two buses.
//
// No custom ICs — all standard catalogue parts (Z80, AY-3-8910, 74LS TTL).
// Only the NMI pin is connected on the main Z80; INT is permanently inactive.
//
// Manifests split by CPU bus domain (main vs sound), not physical board.
// Both cabinet connectors (video + speaker) are on the top board.
//

inline constexpr auto kBombJackMainManifest = make_manifest(
    // ── Chips ──────────────────────────────────────────────────────────
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Main CPU"},                      // [0]
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 32768, .label = "Program ROM"},  // [1]  EPROMs 09–12
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 8192, .label = "Program ROM 2"}, // [2]  EPROM 13
    Slot<RAMChip>{.base_addr = 0x8000, .size_bytes = 4096, .label = "Work RAM"},      // [3]
    Slot<RAMChip>{.base_addr = 0x9000, .size_bytes = 1024, .label = "FG Tilemap"},    // [4]
    Slot<RAMChip>{.base_addr = 0x9400, .size_bytes = 1024, .label = "FG Attributes"}, // [5]
    Slot<RAMChip>{.base_addr = 0x9800, .size_bytes = 256, .label = "Sprite RAM"},     // [6]
    Slot<RAMChip>{.base_addr = 0x9C00, .size_bytes = 256, .label = "Palette RAM"},    // [7]
    Slot<DipSwitchBankComponent>{.descriptor = &bombjack_constants::kBjDSW1, .label = "DSW1"},  // [8]
    Slot<DipSwitchBankComponent>{.descriptor = &bombjack_constants::kBjDSW2, .label = "DSW2"},  // [9]
    // ── Ports (top board connectors) ──────────────────────────────────
    Slot<PortRgb>{.name = "Video Out", .default_device = "crt_arcade"},              // 6-pin RGB header
    Slot<PortAudioMono>{.name = "Audio Out"}                                          // 18-pin edge (speaker)
);

inline constexpr auto kBombJackSoundManifest = make_manifest(
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Sound CPU"},                      // Z80A @ 3.072 MHz
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 8192, .label = "Sound ROM"},     // EPROM 01
    Slot<RAMChip>{.base_addr = 0x4000, .size_bytes = 1024, .label = "Sound RAM"},
    Slot<AY_3_8910>{.label = "PSG 1"},                                                // @ 1.536 MHz
    Slot<AY_3_8910>{.label = "PSG 2"},
    Slot<AY_3_8910>{.label = "PSG 3"}
);

inline constexpr size_t kBJMainChipCount  = decltype(kBombJackMainManifest)::chip_count;
inline constexpr size_t kBJSoundChipCount = decltype(kBombJackSoundManifest)::chip_count;

// ── Bus traits — one per CPU ─────────────────────────────────────────────

struct BombJackMainBusTraits {
    static constexpr const auto& kManifest = kBombJackMainManifest;
    using Spec = ManifestBusSpec<kBombJackMainManifest, 16, 8>;
};

struct BombJackSoundBusTraits {
    static constexpr const auto& kManifest = kBombJackSoundManifest;
    using Spec = ManifestBusSpec<kBombJackSoundManifest, 16, 8>;
};

// ── Main Board ────────────────────────────────────────────────────────────
struct BombJackMainBoard : Board<BombJackMainBusTraits::Spec> {
    using ComponentTuple = decltype(kBombJackMainManifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& cpu      = std::get<0>(components_);
    ROMChip&   rom      = std::get<1>(components_);
    ROMChip&   rom2     = std::get<2>(components_);
    RAMChip&   work_ram = std::get<3>(components_);
    RAMChip&   fg_map   = std::get<4>(components_);
    RAMChip&   fg_attr  = std::get<5>(components_);
    RAMChip&   sprites  = std::get<6>(components_);
    RAMChip&   palette  = std::get<7>(components_);
    DipSwitchBankComponent& dsw1 = std::get<8>(components_);
    DipSwitchBankComponent& dsw2 = std::get<9>(components_);

    // Port aliases (top board connectors)
    PortRgb&       video_out  = std::get<10>(components_);
    PortAudioMono& audio_out  = std::get<11>(components_);

    template<size_t N>
    BombJackMainBoard(const ChipManifest<N>& m) : Board<BombJackMainBusTraits::Spec>(m) {}
};

// ── Sound Board ───────────────────────────────────────────────────────────
struct BombJackSoundBoard : Board<BombJackSoundBusTraits::Spec> {
    using ComponentTuple = decltype(kBombJackSoundManifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& cpu = std::get<0>(components_);
    ROMChip&   rom = std::get<1>(components_);
    RAMChip&   ram = std::get<2>(components_);
    AY_3_8910& ay0 = std::get<3>(components_);
    AY_3_8910& ay1 = std::get<4>(components_);
    AY_3_8910& ay2 = std::get<5>(components_);

    template<size_t N>
    BombJackSoundBoard(const ChipManifest<N>& m) : Board<BombJackSoundBusTraits::Spec>(m) {}
};

class BombJackSystem : public System {
public:
    BombJackSystem();
    ~BombJackSystem() override;

    // ── System interface ─────────────────────────────────────────
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

    bool load_file(const char* filepath) override;

    // ── ROM set support ─────────────────────────────────────────────
    std::vector<const RomSetDescriptor*> get_rom_set_descriptors() const override;
    bool load_rom_set(const RomSetMatch& match) override;
private:
    // ── Audio thread — synthesis runs off the emu thread ────────────────
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<AY_3_8910, true>> ay_adapter_[3];
    std::unique_ptr<AudioPort> audio_port_[3];   // Audio signal output (one per AY)
    uint8_t   ay_latch_[3]{};            // Cached latched register per AY (emu thread)
    uint64_t  sound_cycles_ = 0;         // Monotonic sound-CPU cycle counter (3 MHz)

    // Memory chips are auto-created by Board::create_chips() and accessed
    // via board_.chip_as<T>(slot_index).  No manual pointers needed.

    // ── Graphics ROM — NOT bus-mapped (display rendering only) ───────────
    std::vector<uint8_t> char_rom_;      // Character/tile ROM (3 planes, 12KB)
    std::vector<uint8_t> sprite_rom_;    // Sprite graphics ROM (3 planes, 24KB)
    std::vector<uint8_t> bg_tile_rom_;   // Background tile ROM (3 planes, 24KB)
    std::vector<uint8_t> bg_map_rom_;    // Background map ROM (4KB)

    // ── Main bus ─────────────────────────────────────────────────────────
    using MainBus = MemoryBus<BombJackMainBusTraits::Spec>;
    MainBus main_bus_;
    BombJackMainBoard main_board_{kBombJackMainManifest};

    // ── Sound bus ────────────────────────────────────────────────────────
    using SoundBus = MemoryBus<BombJackSoundBusTraits::Spec>;
    SoundBus sound_bus_;
    BombJackSoundBoard sound_board_{kBombJackSoundManifest};

    // ── Sound — AY refs into sound_board_ (manifest slots, always valid) ──
    struct {
        AY_3_8910* p[3];
        AY_3_8910& operator[](int i)       { return *p[i]; }
        const AY_3_8910& operator[](int i) const { return *p[i]; }
    } ay_ = {{ &sound_board_.ay0, &sound_board_.ay1, &sound_board_.ay2 }};

    // ── Inter-CPU communication ──────────────────────────────────────────
    uint8_t sound_latch_ = 0;           // Main → Sound command latch
    bool    sound_nmi_   = false;       // NMI to sound CPU on latch write

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    BombJackVideo video_gen_;                         // TTL video renderer
    uint8_t bg_image_select_ = 0;       // Active background (0–4)
    uint8_t nmi_mask_        = 0;       // NMI enable (0 = disabled)

    // ── Inputs ───────────────────────────────────────────────────────────
    uint8_t input_p1_     = 0xFF;       // Player 1 (active low)
    uint8_t input_p2_     = 0xFF;       // Player 2 (active low)
    uint8_t input_system_ = 0xFF;       // Coin/start (active low)

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t main_pins_  = BOMBJACK_BUS_DEFAULT_STATE;
    bus_state_t sound_pins_ = BOMBJACK_BUS_DEFAULT_STATE;
    int audio_sample_rate_  = bombjack_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t main_io_tick(bus_state_t pins);   // $B000+ I/O registers
    bus_state_t sound_io_tick(bus_state_t pins);   // AY-3-8910 port I/O
    bool load_roms();
    void decode_palette();                         // Rebuild palette from palette RAM
    void render_frame();                           // Decode FG tilemap into indexed framebuffer
};
