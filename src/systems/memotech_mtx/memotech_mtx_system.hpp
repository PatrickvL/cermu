#pragma once
/*
 * memotech_mtx_system.hpp — Memotech MTX500 / MTX512 Emulated System
 *
 * The Memotech MTX series (1983) are Z80-based home computers
 * with TMS9918A video, AY-3-8910 sound, and Z80 CTC for timing.
 *
 * Templated on MTXVariant for MTX500 (32KB) vs MTX512 (64KB).
 */

#include "systems/memotech_mtx/memotech_mtx_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// MTX Variant Template
// ============================================================================

enum class MTXVariant { MTX500, MTX512 };

template<MTXVariant V> struct MTXVariantTraits;

template<> struct MTXVariantTraits<MTXVariant::MTX500> {
    static constexpr const char* name         = "Memotech MTX500";
    static constexpr const char* short_name   = "MTX500";
    static constexpr const char* description  = "Memotech MTX500 — Z80A @ 4MHz, TMS9918A, AY-3-8910, 32KB RAM (1983)";
    static constexpr const char* data_folder  = "memotech_mtx";
    static constexpr uint32_t    ram_size     = mtx_constants::RAM_SIZE_MTX500;
    static std::vector<const char*> get_aliases() {
        return {"MTX500", "MTX 500"};
    }
};

template<> struct MTXVariantTraits<MTXVariant::MTX512> {
    static constexpr const char* name         = "Memotech MTX512";
    static constexpr const char* short_name   = "MTX512";
    static constexpr const char* description  = "Memotech MTX512 — Z80A @ 4MHz, TMS9918A, AY-3-8910, 64KB RAM (1983)";
    static constexpr const char* data_folder  = "memotech_mtx";
    static constexpr uint32_t    ram_size     = mtx_constants::RAM_SIZE_MTX512;
    static std::vector<const char*> get_aliases() {
        return {"MTX512", "MTX 512"};
    }
};

// ============================================================================
// MTX default bus state
// ============================================================================

#define MTX_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

inline constexpr auto kMTX500Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "OS+BASIC ROM", .rom = {"mtx500.rom|mtx.rom|MTX.ROM"}},
    Slot<TMS9918A>{.base_addr = 0x0001, .label = "TMS9918A"},
    Slot<AY_3_8910>{.base_addr = 0x0003, .label = "AY-3-8910"},
    Slot<z80_ctc_t>{.base_addr = 0x0008, .label = "Z80 CTC"},
    Slot<RAMChip>{.base_addr = 0x4000, .size_bytes = 0x8000, .label = "RAM"},
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port", .port_number = 1, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortRgb>{.name = "Video Out (RGB)", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr auto kMTX512Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "OS+BASIC ROM", .rom = {"mtx512.rom|mtx.rom|MTX.ROM"}},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x10000, .label = "RAM"},
    Slot<TMS9918A>{.base_addr = 0x0001, .label = "TMS9918A"},
    Slot<AY_3_8910>{.base_addr = 0x0003, .label = "AY-3-8910"},
    Slot<z80_ctc_t>{.base_addr = 0x0008, .label = "Z80 CTC"},
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port", .port_number = 1, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortRgb>{.name = "Video Out (RGB)", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kMTX500ChipCount = decltype(kMTX500Manifest)::chip_count;
inline constexpr size_t kMTX512ChipCount = decltype(kMTX512Manifest)::chip_count;
template<MTXVariant V> struct MTXBusTraits;

template<> struct MTXBusTraits<MTXVariant::MTX500> {
    static constexpr const auto& kManifest = kMTX500Manifest;
    using Spec = ManifestBusSpec<kMTX500Manifest, 16, 8>;
};

template<> struct MTXBusTraits<MTXVariant::MTX512> {
    static constexpr const auto& kManifest = kMTX512Manifest;
    using Spec = ManifestBusSpec<kMTX512Manifest, 16, 8>;
};

template<typename BSpec>
struct MTXBoard : Board<BSpec> {
    using ComponentTuple = decltype(kMTX500Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A& z80 = std::get<0>(components_);
    ROMChip&   rom = std::get<1>(components_);
    TMS9918A&  vdp = std::get<2>(components_);
    AY_3_8910& psg = std::get<3>(components_);
    z80_ctc_t& ctc = std::get<4>(components_);
    RAMChip&   ram = std::get<5>(components_);

    // Port aliases
    PortControlDB9& joy1_port     = std::get<6>(components_);
    PortCassette&   cassette_port = std::get<7>(components_);
    PortRgb&        video_port    = std::get<8>(components_);
    PortAudioMono&  audio_port    = std::get<9>(components_);

    template<size_t N>
    MTXBoard(const ChipManifest<N>& m) : Board<BSpec>(m) {}
};
// ============================================================================
// Memotech MTX System
// ============================================================================

template<MTXVariant V>
class MemotechMTXSystem : public System {
    using Traits = MTXVariantTraits<V>;
    using BT     = MTXBusTraits<V>;

public:
    MemotechMTXSystem();
    ~MemotechMTXSystem() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Board + bus (chips live inside board_) ────────────────────────────
    using Bus       = MemoryBus<typename BT::Spec>;
    using PT        = PackingTraits<typename BT::Spec>;
    using MainBoard = MTXBoard<typename BT::Spec>;
    Bus       bus_;
    MainBoard board_{BT::kManifest};

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_    = mtx_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_  = 0;
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[mtx_constants::KEYBOARD_ROWS]{};
    uint8_t keyboard_drive_ = 0;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = MTX_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
