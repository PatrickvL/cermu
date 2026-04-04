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
#include "core/audio_thread.hpp"
#include "utils/write_only_synth_adapter.hpp"
#include <cstdint>
#include <memory>

#define BOMBJACK_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// ============================================================================
// Bomb Jack chip declarations — single source of truth (dual-CPU)
// ============================================================================
//
// Row: V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
// Main CPU: $0000-$7FFF ROM, $8000 RAM, $9xxx video/sprite/palette.
// Sound CPU: $0000-$1FFF ROM, $4000 RAM.
// I/O registers and AY ports handled separately.
//

inline constexpr auto kBombJackMainManifest = make_manifest(
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Main CPU"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 32768, .label = "Program ROM"},
    Slot<RAMChip>{.base_addr = 0x8000, .size_bytes = 4096, .label = "Work RAM"},
    Slot<RAMChip>{.base_addr = 0x9000, .size_bytes = 1024, .label = "FG Tilemap"},
    Slot<RAMChip>{.base_addr = 0x9400, .size_bytes = 1024, .label = "FG Attributes"},
    Slot<RAMChip>{.base_addr = 0x9800, .size_bytes = 256, .label = "Sprite Area"},
    Slot<RAMChip>{.base_addr = 0x9C00, .size_bytes = 256, .label = "Palette RAM"}
);

inline constexpr auto kBombJackSoundManifest = make_manifest(
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Sound CPU"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 8192, .label = "Sound ROM"},
    Slot<RAMChip>{.base_addr = 0x4000, .size_bytes = 1024, .label = "Sound RAM"}
);

// Port manifest — shared between both boards
inline constexpr auto kBombJackPortManifest = make_manifest(
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
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
struct BombJackMainBoard : Board<BombJackMainBusTraits::Spec, NoChips> {
    using ComponentTuple = decltype(kBombJackMainManifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& cpu      = std::get<0>(components_);
    ROMChip&   rom      = std::get<1>(components_);
    RAMChip&   work_ram = std::get<2>(components_);
    RAMChip&   fg_map   = std::get<3>(components_);
    RAMChip&   fg_attr  = std::get<4>(components_);
    RAMChip&   sprites  = std::get<5>(components_);
    RAMChip&   palette  = std::get<6>(components_);

    template<size_t N>
    BombJackMainBoard(const ChipManifest<N>& m) : Board<BombJackMainBusTraits::Spec, NoChips>(m) {}
};

// ── Sound Board ───────────────────────────────────────────────────────────
struct BombJackSoundBoard : Board<BombJackSoundBusTraits::Spec, NoChips> {
    using ComponentTuple = decltype(kBombJackSoundManifest)::component_tuple;
    ComponentTuple components_;

    ZilogZ80A& cpu = std::get<0>(components_);
    ROMChip&   rom = std::get<1>(components_);
    RAMChip&   ram = std::get<2>(components_);

    template<size_t N>
    BombJackSoundBoard(const ChipManifest<N>& m) : Board<BombJackSoundBusTraits::Spec, NoChips>(m) {}
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

private:
    // ── CPUs ─────────────────────────────────────────────────────────────
    ZilogZ80A*  main_cpu_  = nullptr;    // Z80A @ 4 MHz (main) — owned by main_board_
    ZilogZ80A*  sound_cpu_ = nullptr;    // Z80A @ 3 MHz (sound) — owned by sound_board_

    // ── Sound ────────────────────────────────────────────────────────────
    AY_3_8910 ay_[3];                    // 3× AY-3-8910 PSG

    // ── Audio thread — synthesis runs off the emu thread ────────────────
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<AY_3_8910, true>> ay_adapter_[3];
    std::unique_ptr<AudioPort> audio_port_[3];   // Audio signal output (one per AY)
    uint8_t   ay_latch_[3]{};            // Cached latched register per AY (emu thread)
    uint64_t  sound_cycles_ = 0;         // Monotonic sound-CPU cycle counter (3 MHz)

    // Memory chips are auto-created by Board::create_chips() and accessed
    // via board_.chip_as<T>(slot_index).  No manual pointers needed.

    // ── Graphics ROM — NOT bus-mapped (display rendering only) ───────────
    std::vector<uint8_t> char_rom_;      // Character/tile ROM
    std::vector<uint8_t> sprite_rom_;    // Sprite graphics ROM
    std::vector<uint8_t> bg_rom_;        // Background image ROM

    // ── Main bus ─────────────────────────────────────────────────────────
    using MainBus = MemoryBus<BombJackMainBusTraits::Spec>;
    MainBus main_bus_;
    BombJackMainBoard main_board_{kBombJackMainManifest};

    // ── Sound bus ────────────────────────────────────────────────────────
    using SoundBus = MemoryBus<BombJackSoundBusTraits::Spec>;
    SoundBus sound_bus_;
    BombJackSoundBoard sound_board_{kBombJackSoundManifest};

    // ── Inter-CPU communication ──────────────────────────────────────────
    uint8_t sound_latch_ = 0;           // Main → Sound command latch
    bool    sound_nmi_   = false;       // NMI to sound CPU on latch write

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    BombJackVideo video_gen_;                         // TTL foreground tile renderer
    uint8_t bg_image_select_ = 0;       // Active background (0–4)

    // ── Inputs ───────────────────────────────────────────────────────────
    uint8_t input_p1_     = 0xFF;       // Player 1 (active low)
    uint8_t input_p2_     = 0xFF;       // Player 2 (active low)
    uint8_t input_system_ = 0xFF;       // Coin/start (active low)
    uint8_t dsw1_         = 0xFF;       // DIP switch bank 1
    uint8_t dsw2_         = 0xFF;       // DIP switch bank 2

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

    // Chip pointers (cached for rendering hot path)
    RAMChip* fg_tilemap_chip_ = nullptr;
    RAMChip* fg_attr_chip_    = nullptr;
    RAMChip* palette_ram_chip_ = nullptr;
};
