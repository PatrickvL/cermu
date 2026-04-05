#pragma once
/*
 * amstrad_cpc_system.h — Amstrad CPC 464/664/6128 Emulated System
 *
 * The Amstrad CPC (1984-1985) was a popular British home computer series.
 *
 * Architecture:
 *   - Zilog Z80A CPU @ 4 MHz
 *   - Amstrad Gate Array (40018/40226) — mode control, color palette, ROM
 *     banking, interrupt generation, memory paging
 *   - Motorola MC6845 CRTC — programmable display timing
 *   - Intel 8255 PPI — keyboard scanning, AY control, tape, V-sync detect
 *   - GI AY-3-8912 PSG — 3-channel sound (active accent via PPI Port A/C)
 *   - 64KB RAM (464/664) or 128KB RAM (6128)
 *   - Lower ROM: 16KB firmware/BIOS
 *   - Upper ROM: 16KB BASIC (+ optional AMSDOS for disc models)
 *
 * Templated on CPC model for variant-specific behavior (RAM size,
 * disc controller presence, ROM set).
 */

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/audio_port.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/fam6845/mc6845.hpp"
#include "chip/io/i8255.hpp"
#include "chip/sound/ay_psg/ay_3_8912.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/video/amstrad_gate_array/amstrad_gate_array.hpp"
#include "core/audio_thread.hpp"
#include "utils/write_only_synth_adapter.hpp"
#include "systems/amstrad_cpc/amstrad_cpc_constants.hpp"
#include <cstdint>
#include <memory>
#include <vector>

// ============================================================================
// CPC Model Variant
// ============================================================================

enum class CPCModel { CPC464, CPC664, CPC6128 };

template<CPCModel M> struct CPCModelTraits;

template<> struct CPCModelTraits<CPCModel::CPC464> {
    static constexpr const char* name         = "Amstrad CPC 464";
    static constexpr const char* short_name   = "CPC464";
    static constexpr const char* description  = "Amstrad CPC 464 — Z80A, 64KB RAM, tape (1984)";
    static constexpr const char* data_folder  = "amstrad_cpc";
    static constexpr int   ram_size_kb        = 64;
    static constexpr bool  has_disc           = false;
    static std::vector<const char*> get_aliases() {
        return {"CPC464", "CPC", "AmstradCPC"};
    }
};

template<> struct CPCModelTraits<CPCModel::CPC664> {
    static constexpr const char* name         = "Amstrad CPC 664";
    static constexpr const char* short_name   = "CPC664";
    static constexpr const char* description  = "Amstrad CPC 664 — Z80A, 64KB RAM, 3\" disc (1985)";
    static constexpr const char* data_folder  = "amstrad_cpc";
    static constexpr int   ram_size_kb        = 64;
    static constexpr bool  has_disc           = true;
    static std::vector<const char*> get_aliases() { return {"CPC664"}; }
};

template<> struct CPCModelTraits<CPCModel::CPC6128> {
    static constexpr const char* name         = "Amstrad CPC 6128";
    static constexpr const char* short_name   = "CPC6128";
    static constexpr const char* description  = "Amstrad CPC 6128 — Z80A, 128KB RAM, 3\" disc (1985)";
    static constexpr const char* data_folder  = "amstrad_cpc";
    static constexpr int   ram_size_kb        = 128;
    static constexpr bool  has_disc           = true;
    static std::vector<const char*> get_aliases() { return {"CPC6128"}; }
};

// ============================================================================
// CPC default bus state
// ============================================================================

#define CPC_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// Amstrad CPC chip manifests — declarative memory layout
// =============================================================================
//
// CPC 464/664 (64 KB RAM):
//   Slot 0: RAM — 64 KB at $0000 (full address space)
//   Slot 1: Lower ROM — 16 KB at $0000 (BIOS, read overlay when enabled)
//   Slot 2: Upper ROM — 16 KB at $C000 (BASIC, read overlay when enabled)
//
// CPC 6128 (128 KB RAM):
//   Slot 0: RAM — 128 KB at $0000 (8 x 16 KB banks, banked via Gate Array)
//   Slot 1: Lower ROM — 16 KB at $0000 (BIOS, read overlay when enabled)
//   Slot 2: Upper ROM — 16 KB at $C000 (BASIC/AMSDOS, read overlay when enabled)
//
// All I/O is Z80 port-based (IORQ) — no MMIO slots needed.
//
//                                      ctx   type                chip        base    size    mask  ovl  label              rom


inline constexpr auto kCPC464Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.label = "Z80A"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x00010000, .label = "RAM"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "Lower ROM", .overlay_group = 1, .rom = {"cpc464.rom@0|cpc464_os.rom"}},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000, .label = "Upper ROM", .overlay_group = 2, .rom = {"cpc464.rom@16384|cpc464_basic.rom"}},
    Slot<mc6845_t>{.base_addr = 0xBC00, .label = "MC6845 CRTC"},
    Slot<i8255_t>{.base_addr = 0xF400, .label = "8255 PPI"},
    Slot<AY_3_8912>{.label = "AY-3-8912 PSG"},
    Slot<amstrad_gate_array_t>{.base_addr = 0x7F00, .label = "Gate Array"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortRgb>{.name = "Video Out (RGB)", .default_device = "crt_ctm644"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr auto kCPC6128Manifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.label = "Z80A"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x00020000, .label = "RAM", .bank_size = 16384},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x4000, .label = "Lower ROM", .overlay_group = 1, .rom = {"cpc6128.rom@0|cpc6128_os.rom"}},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000, .label = "Upper ROM", .overlay_group = 2, .rom = {"cpc6128.rom@16384|cpc6128_basic.rom"}},
    Slot<mc6845_t>{.base_addr = 0xBC00, .label = "MC6845 CRTC"},
    Slot<i8255_t>{.base_addr = 0xF400, .label = "8255 PPI"},
    Slot<AY_3_8912>{.label = "AY-3-8912 PSG"},
    Slot<amstrad_gate_array_t>{.base_addr = 0x7F00, .label = "Gate Array"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortRgb>{.name = "Video Out (RGB)", .default_device = "crt_ctm644"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kCPC464ChipCount = decltype(kCPC464Manifest)::chip_count;
inline constexpr size_t kCPC6128ChipCount = decltype(kCPC6128Manifest)::chip_count;

// BusTraits — selects the correct manifest per CPC model
template<CPCModel M> struct CPCBusTraits;

template<> struct CPCBusTraits<CPCModel::CPC464> {
    static constexpr const auto& kManifest = kCPC464Manifest;
    using Spec = ManifestBusSpec<kCPC464Manifest, 16, 8>;
};

template<> struct CPCBusTraits<CPCModel::CPC664> {
    static constexpr const auto& kManifest = kCPC464Manifest;  // Same layout as 464
    using Spec = ManifestBusSpec<kCPC464Manifest, 16, 8>;
};

template<> struct CPCBusTraits<CPCModel::CPC6128> {
    static constexpr const auto& kManifest = kCPC6128Manifest;
    using Spec = ManifestBusSpec<kCPC6128Manifest, 16, 8>;
};

template<typename BSpec>
struct CPCBoard : Board<BSpec> {
    using ComponentTuple = decltype(kCPC464Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A&            z80        = std::get<0>(components_);
    RAMChip&              ram        = std::get<1>(components_);
    ROMChip&              lower_rom  = std::get<2>(components_);
    ROMChip&              upper_rom  = std::get<3>(components_);
    mc6845_t&             crtc       = std::get<4>(components_);
    i8255_t&              ppi        = std::get<5>(components_);
    AY_3_8912&            psg        = std::get<6>(components_);
    amstrad_gate_array_t& gate_array = std::get<7>(components_);

    // Port aliases
    PortCassette&  cassette_port  = std::get<8>(components_);
    PortExpansion& expansion_port = std::get<9>(components_);
    PortRgb&       video_port     = std::get<10>(components_);
    PortAudioMono& audio_port     = std::get<11>(components_);

    template<size_t N>
    CPCBoard(const ChipManifest<N>& m) : Board<BSpec>(m) {}
};


// ============================================================================
// Amstrad CPC System
// ============================================================================

template<CPCModel M>
class AmstradCPCSystem : public System {
    using Traits = CPCModelTraits<M>;

public:
    AmstradCPCSystem();
    ~AmstradCPCSystem() override;

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
    // ========================================================================
    // CHIPS (value-typed via Board Chips)
    // ========================================================================

    // ========================================================================
    // MEMORY — owned by registered_chips_, managed via Board
    // ========================================================================

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = CPCBusTraits<M>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = CPCBoard<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;

    // ========================================================================
    // VIDEO
    // ========================================================================

    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output

    // ========================================================================
    // AUDIO
    // ========================================================================

    uint32_t audio_sample_rate_ = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    std::vector<float> audio_buffer_;

    // ── Audio thread — AY synthesis runs off the emu thread ─────────────
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<AY_3_8912, true>> ay_adapter_;
    std::unique_ptr<AudioPort> audio_port_;  // Audio signal output
    uint8_t ay_latch_ = 0;              // Cached latched register (emu thread)

    // ========================================================================
    // HELPERS
    // ========================================================================

    void configure_bus_memory_map();
    void update_banking();           // Remap pages after ROM toggle / 6128 bank switch
    void apply_rom_overlay();        // Load ROM overlay snapshot for current ga state
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
    void render_frame();             // Decode screen RAM into indexed framebuffer

    // Cached chip pointer for hot-path rendering
    RAMChip* ram_chip_ = nullptr;

    // Pre-computed ROM overlay snapshots.
    // Indexed as snapshots_[0][mode] where mode bits: 0=lower ROM, 1=upper ROM.
    // Generated by Board::build_overlay_snapshots() from manifest overlay groups.
    static constexpr size_t kNumOverlayModes = BT::kManifest.overlay_mode_count();
    std::array<std::array<typename Bus::Snapshot, kNumOverlayModes>, 1> snapshots_;
};
