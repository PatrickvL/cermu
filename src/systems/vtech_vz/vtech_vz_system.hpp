#pragma once
/*
 * vtech_vz_system.h — VTech VZ200 / VZ300 / Laser 200 system declaration
 *
 * VTech Laser 200 / VZ200 / VZ300 family (1983-1985):
 *   CPU:    Zilog Z80A @ 3.58 MHz (NTSC color burst crystal)
 *   Video:  Motorola MC6847 VDG (text 32×16, graphics up to 256×192)
 *   Sound:  1-bit speaker (Z80 port-driven, no sound chip)
 *   I/O:    Memory-mapped keyboard/mode registers + Z80 port I/O
 *   Memory: VZ200: 8KB + 16KB ROM;  VZ300: 16KB + 16KB ROM
 *
 * The VZ family is architecturally very similar to the Acorn Atom —
 * both use the MC6847 VDG for video, though the VZ uses Z80 instead of
 * 6502 and has no VIA or PPI on the base board.
 *
 * Laser 210 / Fellow models are functionally identical to VZ200.
 * Laser 310 / Fellow II models are functionally identical to VZ300.
 */

#include "systems/vtech_vz/vtech_vz_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/mc6847/mc6847.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>

// ============================================================================
// VZ Variant Template
// ============================================================================

enum class VZVariant { VZ200, VZ300 };

template<VZVariant V> struct VZVariantTraits;

template<> struct VZVariantTraits<VZVariant::VZ200> {
    static constexpr const char* name         = "VTech VZ200";
    static constexpr const char* short_name   = "VZ200";
    static constexpr const char* description  = "VTech VZ200 / Laser 200 (1983) — Z80A @ 3.58MHz, MC6847, 8KB RAM";
    static constexpr const char* data_folder  = "vtech_vz";
    static constexpr uint32_t    ram_size     = vtech_vz_constants::RAM_SIZE_VZ200;
    static std::vector<const char*> get_aliases() {
        return {"VZ200", "Laser200", "Laser 200", "Salora Fellow", "Dick Smith VZ200"};
    }
};

template<> struct VZVariantTraits<VZVariant::VZ300> {
    static constexpr const char* name         = "VTech VZ300";
    static constexpr const char* short_name   = "VZ300";
    static constexpr const char* description  = "VTech VZ300 / Laser 310 (1985) — Z80A @ 3.58MHz, MC6847, 16KB RAM";
    static constexpr const char* data_folder  = "vtech_vz";
    static constexpr uint32_t    ram_size     = vtech_vz_constants::RAM_SIZE_VZ300;
    static std::vector<const char*> get_aliases() {
        return {"VZ300", "Laser310", "Laser 310", "Salora Fellow II", "Dick Smith VZ300"};
    }
};

// ============================================================================
// VZ default bus state
// ============================================================================

#define VZ_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

inline constexpr auto kVZ200Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "BASIC ROM", .rom = {"vz200.rom|BASIC.ROM|laser200.rom"}},
    Slot<RAMChip>{.base_addr = 0x7000, .size_bytes = 0x0800, .label = "Video RAM"},
    Slot<RAMChip>{.base_addr = 0x7800, .size_bytes = 0x4000, .label = "User RAM"},
    Slot<mc6847_t>{.base_addr = 0x0000, .label = "MC6847 VDG"},
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port", .port_number = 1, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"}
);

inline constexpr auto kVZ300Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "BASIC ROM", .rom = {"vz300.rom|BASIC.ROM|laser310.rom"}},
    Slot<RAMChip>{.base_addr = 0x7000, .size_bytes = 0x0800, .label = "Video RAM"},
    Slot<RAMChip>{.base_addr = 0x7800, .size_bytes = 0x4000, .label = "User RAM"},
    Slot<mc6847_t>{.base_addr = 0x0000, .label = "MC6847 VDG"},
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port", .port_number = 1, .default_device = "joystick"},
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"}
);

inline constexpr size_t kVZ200ChipCount = decltype(kVZ200Manifest)::chip_count;
inline constexpr size_t kVZ300ChipCount = decltype(kVZ300Manifest)::chip_count;
// BusTraits — selects the correct manifest per variant
template<VZVariant V> struct VZBusTraits;

template<> struct VZBusTraits<VZVariant::VZ200> {
    static constexpr const auto& kManifest = kVZ200Manifest;
    using Spec = ManifestBusSpec<kVZ200Manifest, 16, 8>;
};

template<> struct VZBusTraits<VZVariant::VZ300> {
    static constexpr const auto& kManifest = kVZ300Manifest;
    using Spec = ManifestBusSpec<kVZ300Manifest, 16, 8>;
};

template<typename BSpec>
struct VZBoard : Board<BSpec> {
    using ComponentTuple = decltype(kVZ200Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A& z80  = std::get<0>(components_);
    ROMChip&   rom  = std::get<1>(components_);
    RAMChip&   vram = std::get<2>(components_);
    RAMChip&   ram  = std::get<3>(components_);
    mc6847_t&  vdg  = std::get<4>(components_);

    // Port aliases
    PortControlDB9&     joy_port      = std::get<5>(components_);
    PortCassette&       cassette_port = std::get<6>(components_);
    PortCompositeVideo& video_port    = std::get<7>(components_);

    template<size_t N>
    VZBoard(const ChipManifest<N>& m) : Board<BSpec>(m) {}
};


// ============================================================================
// VTech VZ System
// ============================================================================

template<VZVariant V>
class VTechVZSystem : public System {
    using Traits = VZVariantTraits<V>;
    using BTraits = VZBusTraits<V>;

public:
    VTechVZSystem();
    ~VTechVZSystem() override;

    // ── System interface ─────────────────────────────────────────
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
    // ── Chips (value-typed via Board Chips) ────────────────────────────

    // ── Memory chips — post-init pointers ────────────────────────────────
    uint8_t* video_ram_ptr_ = nullptr;

    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<typename BTraits::Spec>;
    using MainBoard = VZBoard<typename BTraits::Spec>;
    Bus       bus_;
    MainBoard board_{BTraits::kManifest};

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[vtech_vz_constants::KEYBOARD_ROWS] = {};

    // ── Speaker ──────────────────────────────────────────────────────────
    bool spkr_state_ = false;
    int  audio_sample_rate_ = vtech_vz_constants::DEFAULT_SAMPLE_RATE;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_      = VZ_BUS_DEFAULT_STATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bool load_roms();
    void io_tick(bus_state_t& bus);   // Z80 port I/O dispatch
};
