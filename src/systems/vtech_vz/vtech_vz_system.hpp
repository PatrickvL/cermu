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
#include "core/system_chip_visitors.hpp"
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

// =============================================================================
// VZ chip declarations — variant-specific single source of truth
// =============================================================================
//
// Row: V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
// VZ200: 16 KB ROM at $0000, 2 KB Video RAM at $7000, 16 KB User RAM at $7800
// VZ300: same layout, only ROM filename differs
//

#define VZ200_FOR_EACH_SYSTEM_CHIP(V, ctx)                                                                  \
    V(ctx, ZilogZ80A,  z80,   0x0000,     0, 0, 0, "Z80A",       nullptr)                                   \
    V(ctx, ROMChip,    rom,   0x0000, 16384, 0, 0, "BASIC ROM",  "vz200.rom|BASIC.ROM|laser200.rom")        \
    V(ctx, RAMChip,    vram,  0x7000,  2048, 0, 0, "Video RAM",  nullptr)                                   \
    V(ctx, RAMChip,    ram,   0x7800, 16384, 0, 0, "User RAM",   nullptr)                                   \
    V(ctx, mc6847_t,   vdg,   0x0000,     0, 0, 0, "MC6847 VDG", nullptr)

#define VZ300_FOR_EACH_SYSTEM_CHIP(V, ctx)                                                                  \
    V(ctx, ZilogZ80A,  z80,   0x0000,     0, 0, 0, "Z80A",       nullptr)                                   \
    V(ctx, ROMChip,    rom,   0x0000, 16384, 0, 0, "BASIC ROM",  "vz300.rom|BASIC.ROM|laser310.rom")        \
    V(ctx, RAMChip,    vram,  0x7000,  2048, 0, 0, "Video RAM",  nullptr)                                   \
    V(ctx, RAMChip,    ram,   0x7800, 16384, 0, 0, "User RAM",   nullptr)                                   \
    V(ctx, mc6847_t,   vdg,   0x0000,     0, 0, 0, "MC6847 VDG", nullptr)

static constexpr size_t kVZ200ChipCount = 0 VZ200_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);
static constexpr size_t kVZ300ChipCount = 0 VZ300_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kVZ200ChipCount> kVZ200Chips = ChipManifest<kVZ200ChipCount>{{
    VZ200_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

inline constexpr ChipManifest<kVZ300ChipCount> kVZ300Chips = ChipManifest<kVZ300ChipCount>{{
    VZ300_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

// BusTraits — selects the correct manifest per variant
template<VZVariant V> struct VZBusTraits;

template<> struct VZBusTraits<VZVariant::VZ200> {
    static constexpr const auto& kManifest = kVZ200Chips;
    using Spec = ManifestBusSpec<kVZ200Chips, 16, 8>;
};

template<> struct VZBusTraits<VZVariant::VZ300> {
    static constexpr const auto& kManifest = kVZ300Chips;
    using Spec = ManifestBusSpec<kVZ300Chips, 16, 8>;
};

// ── Chips ──────────────────────────────────────────────────────────────
struct VZChips {
    VZ200_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
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
    using MainBoard = Board<typename BTraits::Spec, VZChips>;
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
