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
// Bomb Jack chip manifests — declarative memory layout (dual-CPU)
// ============================================================================
//
// Main CPU ($0000-$7FFF ROM, $8000 RAM, $9xxx video/sprite/palette, $Bxxx I/O):
//   Slot 0: Program ROM    — 32 KB at $0000  (read-only)
//   Slot 1: Work RAM       —  4 KB at $8000
//   Slot 2: FG tilemap     —  1 KB at $9000
//   Slot 3: FG attributes  —  1 KB at $9400
//   Slot 4: Sprite area    — 256 bytes at $9800  (sprite RAM at offset $20)
//   Slot 5: Palette RAM    — 256 bytes at $9C00
//
// Sound CPU ($0000-$1FFF ROM, $4000 RAM, $6000 latch via manual dispatch):
//   Slot 0: Sound ROM  — 8 KB at $0000  (read-only)
//   Slot 1: Sound RAM  — 1 KB at $4000
//
// I/O registers ($B000+ main, $6000 sound latch) handled separately due to
// side effects (NMI clear, overlapping read/write semantics).
// AY-3-8910 ports use Z80 IORQ, not memory-mapped.
// Graphics ROMs (char, sprite, bg) are NOT bus-mapped.
//
inline constexpr auto kBombJackMainChips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "Program ROM"},
    Slot<RAMChip>{0x8000,  4096, 0, "Work RAM"},
    Slot<RAMChip>{0x9000,  1024, 0, "FG Tilemap"},
    Slot<RAMChip>{0x9400,  1024, 0, "FG Attributes"},
    Slot<RAMChip>{0x9800,   256, 0, "Sprite Area"},
    Slot<RAMChip>{0x9C00,   256, 0, "Palette RAM"},
    // Non-bus chip — factory-created, not address-decoded
    Slot<ZilogZ80A>{0, 0, 0, "Main CPU"}
);

inline constexpr auto kBombJackSoundChips = make_chip_manifest(
    Slot<ROMChip>{0x0000,  8192, 0, "Sound ROM"},
    Slot<RAMChip>{0x4000,  1024, 0, "Sound RAM"},
    // Non-bus chip — factory-created, not address-decoded
    Slot<ZilogZ80A>{0, 0, 0, "Sound CPU"}
);


// ── Bus traits — one per CPU ─────────────────────────────────────────────

struct BombJackMainBusTraits {
    static constexpr const auto& kManifest = kBombJackMainChips;
    using Spec = ManifestBusSpec<kBombJackMainChips, 16, 8>;
};

struct BombJackSoundBusTraits {
    static constexpr const auto& kManifest = kBombJackSoundChips;
    using Spec = ManifestBusSpec<kBombJackSoundChips, 16, 8>;
};

// Value-typed chips: one Z80A per board, no video/sound/IO in slots.
using BombJackMainChipset  = CoreChips<ZilogZ80A>;
using BombJackSoundChipset = CoreChips<ZilogZ80A>;

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
    using MainBoard = Board<BombJackMainBusTraits::Spec, BombJackMainChipset>;
    MainBus main_bus_;
    MainBoard main_board_{kBombJackMainChips};

    // ── Sound bus ────────────────────────────────────────────────────────
    using SoundBus = MemoryBus<BombJackSoundBusTraits::Spec>;
    using SoundBoard = Board<BombJackSoundBusTraits::Spec, BombJackSoundChipset>;
    SoundBus sound_bus_;
    SoundBoard sound_board_{kBombJackSoundChips};

    // ── Inter-CPU communication ──────────────────────────────────────────
    uint8_t sound_latch_ = 0;           // Main → Sound command latch
    bool    sound_nmi_   = false;       // NMI to sound CPU on latch write

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output
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
