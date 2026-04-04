#pragma once
/*
 * sega_sg1000_system.hpp — Sega SG-1000 / SC-3000 Emulated System
 *
 * The SG-1000 (1983) was Sega's first home console.
 * The SC-3000 was the computer variant with a full keyboard.
 * Both share the same Z80 + TMS9918A + SN76489 chipset.
 *
 * Templated on SG1000Variant to share code.
 */

#include "systems/sega/sg1000/sega_sg1000_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// SG-1000 Variant Template
// ============================================================================

enum class SG1000Variant { SG1000, SC3000 };

template<SG1000Variant V> struct SG1000VariantTraits;

template<> struct SG1000VariantTraits<SG1000Variant::SG1000> {
    static constexpr const char* name         = "Sega SG-1000";
    static constexpr const char* short_name   = "SG-1000";
    static constexpr const char* description  = "Sega SG-1000 — Z80A, TMS9918A, SN76489 (1983)";
    static constexpr const char* data_folder  = "sega_sg1000";
    static constexpr uint32_t    ram_size     = sg1000_constants::RAM_SIZE_SG1000;
    static constexpr bool        has_keyboard = false;
    static std::vector<const char*> get_aliases() {
        return {"SG1000", "SG-1000"};
    }
};

template<> struct SG1000VariantTraits<SG1000Variant::SC3000> {
    static constexpr const char* name         = "Sega SC-3000";
    static constexpr const char* short_name   = "SC-3000";
    static constexpr const char* description  = "Sega SC-3000 — Z80A, TMS9918A, SN76489, keyboard (1983)";
    static constexpr const char* data_folder  = "sega_sg1000";
    static constexpr uint32_t    ram_size     = sg1000_constants::RAM_SIZE_SC3000;
    static constexpr bool        has_keyboard = true;
    static std::vector<const char*> get_aliases() {
        return {"SC3000", "SC-3000"};
    }
};

// ============================================================================
// SG-1000 default bus state
// ============================================================================

#define SG1000_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

inline constexpr auto kSG1000Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x8000, .label = "Cart ROM"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x10000, .label = "RAM"},
    Slot<sn76489_t>{.base_addr = 0x007E, .label = "SN76489"},
    Slot<TMS9918A>{.base_addr = 0x00BE, .label = "TMS9918A"},
    // Ports
    Slot<PortControlDB9>{.name = "Controller Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Controller Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kSG1000ChipCount = decltype(kSG1000Manifest)::chip_count;
using SG1000BusSpec = ManifestBusSpec<kSG1000Manifest, 16, 8>;

struct SG1000Board : Board<SG1000BusSpec, NoChips> {
    using ComponentTuple = decltype(kSG1000Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A& z80  = std::get<0>(components_);
    ROMChip&   cart = std::get<1>(components_);
    RAMChip&   ram  = std::get<2>(components_);
    sn76489_t& psg  = std::get<3>(components_);
    TMS9918A&  vdp  = std::get<4>(components_);

    // Port aliases
    PortControlDB9&     ctrl1_port     = std::get<5>(components_);
    PortControlDB9&     ctrl2_port     = std::get<6>(components_);
    PortExpansion&      cartridge_port = std::get<7>(components_);
    PortCompositeVideo& video_port     = std::get<8>(components_);
    PortAudioMono&      audio_port     = std::get<9>(components_);

    SG1000Board() : Board(kSG1000Manifest) {}
};
// ============================================================================
// Sega SG-1000 / SC-3000 System
// ============================================================================

template<SG1000Variant V>
class SegaSG1000System : public System {
    using Traits = SG1000VariantTraits<V>;

public:
    SegaSG1000System();
    ~SegaSG1000System() override;

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
    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<SG1000BusSpec>;
    using PT        = PackingTraits<SG1000BusSpec>;
    using MainBoard = SG1000Board;
    Bus       bus_;
    MainBoard board_;

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_ = sg1000_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ── Joystick ─────────────────────────────────────────────────────────
    uint8_t joypad_state_ = 0xFF;  // Active-low: Up/Down/Left/Right/TL/TR

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = SG1000_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
};
