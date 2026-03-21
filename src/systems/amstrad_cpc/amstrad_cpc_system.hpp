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
#include "chip/video/mc6845/mc6845.hpp"
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
inline constexpr auto kCPC464Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0x0000, 16384, 0, "Lower ROM", 0, 0, 1},  // overlay group 1
    Slot<ROMChip>{0xC000, 16384, 0, "Upper ROM", 0, 0, 2},  // overlay group 2
    // Non-bus chips — factory-created or pre-bound, not address-decoded
    Slot<ZilogZ80A>           {0, 0, 0, "Z80A"},
    Slot<mc6845_t>            {0, 0, 0, "MC6845 CRTC"},
    Slot<i8255_t>             {0, 0, 0, "8255 PPI"},
    Slot<AY_3_8912>           {0, 0, 0, "AY-3-8912 PSG"},
    Slot<amstrad_gate_array_t>{0, 0, 0, "Gate Array"}
);

inline constexpr auto kCPC6128Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 131072, 0, "RAM", 0, 16384},          // 128 KB (8 × 16 KB banks)
    Slot<ROMChip>{0x0000,  16384, 0, "Lower ROM", 0, 0, 1},    // overlay group 1
    Slot<ROMChip>{0xC000,  16384, 0, "Upper ROM", 0, 0, 2},    // overlay group 2
    // Non-bus chips — factory-created or pre-bound, not address-decoded
    Slot<ZilogZ80A>           {0, 0, 0, "Z80A"},
    Slot<mc6845_t>            {0, 0, 0, "MC6845 CRTC"},
    Slot<i8255_t>             {0, 0, 0, "8255 PPI"},
    Slot<AY_3_8912>           {0, 0, 0, "AY-3-8912 PSG"},
    Slot<amstrad_gate_array_t>{0, 0, 0, "Gate Array"}
);


// BusTraits — selects the correct manifest per CPC model
template<CPCModel M> struct CPCBusTraits;

template<> struct CPCBusTraits<CPCModel::CPC464> {
    static constexpr const auto& kManifest = kCPC464Chips;
    using Spec = ManifestBusSpec<kCPC464Chips, 16, 8>;
};

template<> struct CPCBusTraits<CPCModel::CPC664> {
    static constexpr const auto& kManifest = kCPC464Chips;  // Same layout as 464
    using Spec = ManifestBusSpec<kCPC464Chips, 16, 8>;
};

template<> struct CPCBusTraits<CPCModel::CPC6128> {
    static constexpr const auto& kManifest = kCPC6128Chips;
    using Spec = ManifestBusSpec<kCPC6128Chips, 16, 8>;
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

    bool load_file(const char* filepath) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ========================================================================
    // CHIPS
    // ========================================================================

    ZilogZ80A*              cpu_ = nullptr;   // Z80A CPU — owned by board_
    mc6845_t                crtc_;        // MC6845 CRTC — pre-bound into board_
    i8255_t                 ppi_;         // Intel 8255 PPI — pre-bound into board_
    AY_3_8912               ay_;          // AY-3-8912 PSG — pre-bound into board_
    amstrad_gate_array_t    gate_array_;  // Amstrad custom gate array

    // ========================================================================
    // MEMORY — owned by registered_chips_, managed via Board
    // ========================================================================

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = CPCBusTraits<M>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;
    bool        system_ready_ = false;

    // ========================================================================
    // DISPLAY — IndexedFrameBuffer owns palette + RGBA fallback.
    // Gate Array chip writes scanlines; display_ handles GPU routing.
    // ========================================================================

    IndexedFrameBuffer display_;
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output

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
