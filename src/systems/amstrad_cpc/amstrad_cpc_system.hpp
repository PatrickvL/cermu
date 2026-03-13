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
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/mc6845/mc6845.hpp"
#include "chip/io/i8255.hpp"
#include "chip/sound/ay_3_8910.hpp"
#include "chip/memory/memory_chip.hpp"
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
// Amstrad Gate Array (custom ASIC)
// ============================================================================

struct amstrad_gate_array_t {
    uint8_t  pen_select = 0;                        // Selected pen (0-16, 16=border)
    uint8_t  ink[amstrad_cpc_constants::GA_PEN_COUNT]{};  // Pen->hardware color mapping
    uint8_t  screen_mode = 1;                       // 0, 1, or 2
    bool     lower_rom_enabled = true;              // BIOS ROM at $0000-$3FFF
    bool     upper_rom_enabled = true;              // BASIC ROM at $C000-$FFFF
    uint8_t  ram_config = 0;                        // 6128 RAM banking register
    uint8_t  interrupt_counter = 0;                 // Counts HSYNC, fires IRQ every 52
    bool     interrupt_pending = false;

    void reset() {
        pen_select = 0;
        std::memset(ink, 0, sizeof(ink));
        screen_mode = 1;
        lower_rom_enabled = true;
        upper_rom_enabled = true;
        ram_config = 0;
        interrupt_counter = 0;
        interrupt_pending = false;
    }
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
    Slot<ROMChip>{0x0000, 16384, 0, "Lower ROM"},  // overlay
    Slot<ROMChip>{0xC000, 16384, 0, "Upper ROM"},  // overlay
    // Non-bus chips — factory-created, not address-decoded
    Slot<ZilogZ80A>    {0, 0, 0, "Z80A"},
    Slot<mc6845_t>     {0, 0, 0, "MC6845 CRTC"},
    Slot<i8255_t>      {0, 0, 0, "8255 PPI"},
    Slot<ay_3_8910_t>  {0, 0, 0, "AY-3-8912 PSG"}
);

inline constexpr auto kCPC6128Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 131072, 0, "RAM"},        // 128 KB (8 banks)
    Slot<ROMChip>{0x0000,  16384, 0, "Lower ROM"},  // overlay
    Slot<ROMChip>{0xC000,  16384, 0, "Upper ROM"},  // overlay
    // Non-bus chips — factory-created, not address-decoded
    Slot<ZilogZ80A>    {0, 0, 0, "Z80A"},
    Slot<mc6845_t>     {0, 0, 0, "MC6845 CRTC"},
    Slot<i8255_t>      {0, 0, 0, "8255 PPI"},
    Slot<ay_3_8910_t>  {0, 0, 0, "AY-3-8912 PSG"}
);

namespace cpc_chips {
    inline constexpr size_t kRamSlot      = 0;
    inline constexpr size_t kLowerRomSlot = 1;
    inline constexpr size_t kUpperRomSlot = 2;
    // Non-bus chip slots
    inline constexpr size_t kCpuSlot      = 3;
    inline constexpr size_t kCrtcSlot     = 4;
    inline constexpr size_t kPpiSlot      = 5;
    inline constexpr size_t kAySlot       = 6;
}

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

    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ========================================================================
    // CHIPS
    // ========================================================================

    ZilogZ80A*              cpu_ = nullptr;   // Z80A CPU — owned by board_
    mc6845_t                crtc_;        // MC6845 CRTC — pre-bound into board_
    i8255_t                 ppi_;         // Intel 8255 PPI — pre-bound into board_
    ay_3_8910_t             ay_;          // AY-3-8912 PSG — pre-bound into board_
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
    // DISPLAY
    // ========================================================================

    uint32_t framebuffer_[amstrad_cpc_constants::FB_WIDTH * amstrad_cpc_constants::FB_HEIGHT]{};

    // ========================================================================
    // AUDIO
    // ========================================================================

    uint32_t audio_sample_rate_ = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    std::vector<float> audio_buffer_;

    // ========================================================================
    // HELPERS
    // ========================================================================

    void configure_bus_memory_map();
    void update_banking();           // Remap pages after ROM toggle / 6128 bank switch
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
