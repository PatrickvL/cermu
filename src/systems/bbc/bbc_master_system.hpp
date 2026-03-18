#pragma once
/*
 * bbc_master_system.h — BBC Micro B+ / Master system declarations
 *
 * The BBC Micro Model B+ (1985) and BBC Master (1986) are enhanced
 * versions of the BBC Micro Model B, both using the WDC 65C02 CPU.
 *
 * BBC Micro Model B+ (B+64 / B+128):
 *   CPU:    WDC 65C02 @ 2 MHz
 *   RAM:    64KB (B+64) or 128KB (B+128) — extra RAM as sideways RAM
 *   Video:  Same MC6845 CRTC + Video ULA as Model B
 *   Sound:  SN76489 (same as Model B)
 *   I/O:    2× MOS 6522 VIA (same as Model B)
 *   ROM:    16KB MOS (OS 2.0) + 16 sideways ROM slots
 *   Changes: Shadow screen RAM, extra paging logic
 *
 * BBC Master 128:
 *   CPU:    WDC 65C02 @ 2 MHz
 *   RAM:    128KB (32KB main + 20KB shadow + 16KB filing system +
 *           4KB OS + 64KB sideways RAM in 4×16KB banks)
 *   Video:  MC6845 CRTC + Video ULA (same display hardware)
 *   Sound:  SN76489 (same)
 *   I/O:    2× MOS 6522 VIA (same)
 *           Internal ACIA (MC6850) for serial
 *           RTC (HD146818) — real-time clock
 *   ROM:    128KB MOS (MOS 3.20 — 4×16KB banks) + sideways ROMs
 *   Changes: More I/O, CMOS settings RAM, cartridge slots
 *
 * Both systems share the same chipset as Model B, differing only in
 * CPU (65C02 vs 6502), RAM configuration, and memory banking logic.
 * All new behavior is system-level glue — no new chips required.
 *
 * Memory map (Master 128 — default config):
 *   $0000-$2FFF : Main RAM (pages 0-2)
 *   $3000-$7FFF : Main RAM or Shadow RAM (accent accent via ACCCON register)
 *   $8000-$8FFF : Filing system RAM (4KB) or sideways ROM/RAM
 *   $8000-$BFFF : Sideways ROM/RAM (16KB paged)
 *   $C000-$DFFF : OS ROM (lower 8KB)
 *   $E000-$FFFF : OS ROM (upper 8KB)
 *   $FC00-$FEFF : FRED / JIM / SHEILA I/O (same layout as Model B)
 */

#include "systems/bbc/bbc_micro_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "chip/cpu/fam65xx/wdc65c02.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/video/mc6845/mc6845.hpp"
#include "chip/video/bbc_vidproc/bbc_vidproc.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "core/audio_thread.hpp"
#include "utils/write_only_synth_adapter.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// BBC B+ / Master Variant Template
// ============================================================================

enum class BBCMasterVariant { MODEL_B_PLUS, MASTER_128 };

template<BBCMasterVariant V> struct BBCMasterVariantTraits;

template<> struct BBCMasterVariantTraits<BBCMasterVariant::MODEL_B_PLUS> {
    static constexpr const char* name         = "BBC Micro Model B+";
    static constexpr const char* short_name   = "BBCB+";
    static constexpr const char* description  = "Acorn BBC Micro Model B+ (1985) — 65C02, 64KB RAM, OS 2.0";
    static constexpr const char* data_folder  = "bbc";
    static constexpr uint32_t    ram_size     = 65536;   // 64KB
    static constexpr bool        has_shadow   = true;
    static constexpr bool        has_rtc      = false;
    static constexpr bool        has_cartridge = false;
    static std::vector<const char*> get_aliases() {
        return {"BBCB+", "BBC B+", "BBCBPlus", "Model B+"};
    }
};

template<> struct BBCMasterVariantTraits<BBCMasterVariant::MASTER_128> {
    static constexpr const char* name         = "BBC Master 128";
    static constexpr const char* short_name   = "BBCMaster";
    static constexpr const char* description  = "Acorn BBC Master 128 (1986) — 65C02, 128KB RAM, MOS 3.20";
    static constexpr const char* data_folder  = "bbc";
    static constexpr uint32_t    ram_size     = 131072;  // 128KB
    static constexpr bool        has_shadow   = true;
    static constexpr bool        has_rtc      = true;    // HD146818 RTC
    static constexpr bool        has_cartridge = true;   // 2 cartridge slots
    static std::vector<const char*> get_aliases() {
        return {"BBCMaster", "BBC Master", "Master128", "Master 128"};
    }
};

// ============================================================================
// Bus default state — same as Model B (65C02 is pin-compatible with 6502)
// ============================================================================

#define BBC_MASTER_BUS_DEFAULT_STATE (WDC_65C02::default_bus_state())

// =============================================================================
// BBC B+ / Master chip manifests
// =============================================================================
//
// Model B+ (64KB):
//   Slot 0: RAM — 64KB at $0000 (32KB main + 32KB shadow)
//   Slot 1: Paged ROM — 256KB (16 × 16KB sideways slots)
//   Slot 2: OS ROM — 16KB at $C000
//
// Master 128:
//   Slot 0: RAM — 128KB at $0000 (main + shadow + filing + sideways)
//   Slot 1: Paged ROM — 256KB (16 × 16KB sideways slots)
//   Slot 2: OS ROM — 64KB at $C000 (4 × 16KB MOS banks, selected by ACCCON)
//

inline constexpr auto kBBCBPlusChips = make_chip_manifest(
    Slot<RAMChip>{0x0000,  65536, 0, "RAM"},
    Slot<ROMChip>{0x8000, 262144, 0, "Paged ROM", 0, 16384},
    Slot<ROMChip>{0xC000,  16384, 0, "MOS ROM"}.with_rom("bplus_os.rom|OS20.ROM|os20.rom"),
    // Non-bus chips
    Slot<WDC_65C02>    {0, 0, 0, "WDC 65C02"},
    Slot<mc6845_t>     {0, 0, 0, "MC6845 CRTC"},
    Slot<sn76489_t>    {0, 0, 0, "SN76489 PSG"},
    Slot<mos6522_t>    {0, 0, 0, "System VIA"},
    Slot<mos6522_t>    {0, 0, 0, "User VIA"},
    Slot<bbc_vidproc_t>{0, 0, 0, "Video ULA"}
);

inline constexpr auto kBBCMasterChips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 131072, 0, "RAM", 0, 32768},       // 4 × 32KB banks
    Slot<ROMChip>{0x8000, 262144, 0, "Paged ROM", 0, 16384},
    Slot<ROMChip>{0xC000,  65536, 0, "MOS ROM", 0, 16384}.with_rom("master_mos320.rom|MOS320.ROM|mos3.20.rom"),
    // Non-bus chips
    Slot<WDC_65C02>    {0, 0, 0, "WDC 65C02"},
    Slot<mc6845_t>     {0, 0, 0, "MC6845 CRTC"},
    Slot<sn76489_t>    {0, 0, 0, "SN76489 PSG"},
    Slot<mos6522_t>    {0, 0, 0, "System VIA"},
    Slot<mos6522_t>    {0, 0, 0, "User VIA"},
    Slot<bbc_vidproc_t>{0, 0, 0, "Video ULA"}
);

// BusTraits — selects the correct manifest per variant
template<BBCMasterVariant V> struct BBCMasterBusTraits;

template<> struct BBCMasterBusTraits<BBCMasterVariant::MODEL_B_PLUS> {
    static constexpr const auto& kManifest = kBBCBPlusChips;
    using Spec = ManifestBusSpec<kBBCBPlusChips, 16, 8>;
};

template<> struct BBCMasterBusTraits<BBCMasterVariant::MASTER_128> {
    static constexpr const auto& kManifest = kBBCMasterChips;
    using Spec = ManifestBusSpec<kBBCMasterChips, 16, 8>;
};

// ============================================================================
// BBC B+ / Master System
// ============================================================================

template<BBCMasterVariant V>
class BBCMasterSystem : public System {
    using Traits = BBCMasterVariantTraits<V>;
    using BTraits = BBCMasterBusTraits<V>;

public:
    BBCMasterSystem();
    ~BBCMasterSystem() override;

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

    void get_display_dimensions(int* width, int* height) const override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    WDC_65C02*  cpu_  = nullptr;     // WDC 65C02 — owned by board_
    mc6845_t*   crtc_ = nullptr;     // MC6845 CRTC — owned by board_
    sn76489_t*  psg_  = nullptr;     // SN76489 PSG — owned by board_
    mos6522_t   system_via_;         // System VIA — pre-bound
    mos6522_t   user_via_;           // User VIA — pre-bound

    // Audio thread — SN76489 synthesis runs off the emulation thread
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<sn76489_t>> psg_adapter_;

    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<typename BTraits::Spec>;
    using MainBoard = Board<typename BTraits::Spec>;
    Bus       bus_;
    MainBoard board_{BTraits::kManifest};

    // Memory chip pointers
    RAMChip* ram_chip_        = nullptr;
    ROMChip* paged_rom_chip_  = nullptr;
    ROMChip* os_rom_chip_     = nullptr;
    uint8_t* memory_          = nullptr;    // Direct pointer for rendering

    // Video ULA chip (VIDPROC) — pre-bound
    bbc_vidproc_t vidproc_;

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;

    // ── Paged ROM / Shadow RAM state ─────────────────────────────────────
    uint8_t rom_select_     = 0;
    uint8_t acccon_         = 0;         // ACCCON register (Master: shadow/VDU/filing)
    bool    shadow_active_  = false;     // Shadow screen RAM active

    // ── Keyboard ─────────────────────────────────────────────────────────
    bool    key_matrix_[10][8]{};
    bool    any_key_pressed_ = false;
    uint8_t addressable_latch_ = 0;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_;
    uint32_t    cycles_per_frame_;
    uint32_t    crtc_divider_ = 0;
    bool        system_ready_ = false;

    // ── Internal helpers ─────────────────────────────────────────────────
    void tick_cpu();
    bus_state_t sheila_tick(bus_state_t s);
    void configure_bus_memory_map();
    void update_paged_rom();
    void update_shadow_mapping();        // Apply ACCCON shadow state
    bool load_roms();
    void update_key_matrix(SDL_Keycode key, bool pressed);
    uint8_t scan_keyboard(uint8_t column) const;

    // CRTC display callbacks
    void crtc_display_char(uint16_t ma, uint8_t ra, bool cursor);
    void crtc_vsync();
    void crtc_hsync();

    // VIA callbacks
    static uint8_t sys_via_port_a_read(void* ctx, uint8_t output);
    static uint8_t sys_via_port_b_read(void* ctx, uint8_t output);
};
