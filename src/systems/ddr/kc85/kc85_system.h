#pragma once
/*
 * kc85_system.h — KC 85/2, /3, /4 system declaration
 *
 * Three variants, templated:
 *   KC85_2 (HC 900) — 16 KB RAM, no BASIC ROM, CAOS 2.2 (1984)
 *   KC85_3          — 16 KB RAM, built-in BASIC ROM, CAOS 3.1 (1986)
 *   KC85_4          — 64 KB RAM, extended video (2 planes), CAOS 4.2 (1989)
 */


#include "kc85_constants.h"
#include "../../../core/system.h"
#include "../../../core/system_lines.h"
#include "../../../core/chip_manifest.hpp"
#include "../../../chip/cpu/z80/u880.h"
#include "../../../chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "../../../chip/io/z80_pio.h"
#include "../../../chip/io/z80_ctc.h"
#include "../../../chip/io/kc85_module_system.h"
#include "../../../chip/memory/memory_chip.h"
#include <cstdint>
#include <vector>

#define KC85_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class KC85Variant { KC85_2, KC85_3, KC85_4 };

template<KC85Variant V> struct KC85VariantTraits;

template<> struct KC85VariantTraits<KC85Variant::KC85_2> {
    static constexpr const char* name            = "KC 85/2";
    static constexpr const char* short_name      = "KC85/2";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/2 (HC 900) — U880 @ 1.77MHz, 16KB RAM, CAOS 2.2 (1984)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
    static constexpr bool        has_extended_video = false;
    static constexpr const char* caos_version    = "2.2";
    // Slot indices into kKC852Chips
    static constexpr size_t kCaosRomSlot         = 2;
};

template<> struct KC85VariantTraits<KC85Variant::KC85_3> {
    static constexpr const char* name            = "KC 85/3";
    static constexpr const char* short_name      = "KC85/3";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/3 — U880 @ 1.77MHz, 16KB RAM, BASIC, CAOS 3.1 (1986)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = true;
    static constexpr bool        has_extended_video = false;
    static constexpr const char* caos_version    = "3.1";
    // Slot indices into kKC853Chips
    static constexpr size_t kBasicRomSlot        = 2;
    static constexpr size_t kCaosRomSlot         = 3;
};

template<> struct KC85VariantTraits<KC85Variant::KC85_4> {
    static constexpr const char* name            = "KC 85/4";
    static constexpr const char* short_name      = "KC85/4";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/4 — U880 @ 1.77MHz, 64KB RAM, dual-plane video, CAOS 4.2 (1989)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_64K;
    static constexpr bool        has_basic_rom   = true;
    static constexpr bool        has_extended_video = true;
    static constexpr const char* caos_version    = "4.2";
    // Slot indices into kKC854Chips
    static constexpr size_t kBasicRomSlot        = 2;
    static constexpr size_t kCaosRomSlot         = 3;
};

// ============================================================================
// KC 85 chip manifests — declarative memory layout
// ============================================================================
//
// KC85/2 (3 slots):
//   Slot 0: RAM       — 16 KB at $0000         (always mapped)
//   Slot 1: IRM       — 16 KB at $8000         (pixel + color interleaved)
//   Slot 2: CAOS ROM  —  8 KB at $E000         (OS ROM)
//
// KC85/3 (4 slots):
//   Slot 0: RAM       — 16 KB at $0000         (always mapped)
//   Slot 1: IRM       — 16 KB at $8000         (pixel + color interleaved)
//   Slot 2: BASIC ROM —  8 KB at $C000
//   Slot 3: CAOS ROM  —  8 KB at $E000
//
// KC85/4 (4 slots):
//   Slot 0: RAM       — 32 KB at $0000         ($0000-$7FFF, always mapped)
//   Slot 1: IRM       — 64 KB at $8000         (4 banks: pixel0/pixel1/color0/color1)
//   Slot 2: BASIC ROM —  8 KB at $C000
//   Slot 3: CAOS ROM  —  8 KB at $E000
//
// KC85/4 IRM bank layout (64 KB, 256 pages):
//   Pages   0- 63: Pixel RAM plane 0
//   Pages  64-127: Pixel RAM plane 1
//   Pages 128-191: Color RAM plane 0
//   Pages 192-255: Color RAM plane 1
//
// Banking (all variants):
//   IRM at $8000-$BFFF: enabled/disabled via PIO B bit 2
//   BASIC ROM at $C000-$DFFF: enabled/disabled via PIO B bit 6 (KC85/3,/4)
//   CAOS ROM at $E000-$FFFF: enabled/disabled via PIO B bit 0
//   KC85/4 IRM bank: selected via port $84 bits 0-1
//
inline constexpr auto kKC852Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 16384, 0, "RAM"},
    Slot<RAMChip>{0x8000, 16384, 0, "IRM"},
    Slot<ROMChip>{0xE000,  8192, 0, "CAOS ROM"}
);

inline constexpr auto kKC853Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 16384, 0, "RAM"},
    Slot<RAMChip>{0x8000, 16384, 0, "IRM"},
    Slot<ROMChip>{0xC000,  8192, 0, "BASIC ROM"},
    Slot<ROMChip>{0xE000,  8192, 0, "CAOS ROM"}
);

inline constexpr auto kKC854Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 32768, 0, "RAM"},
    Slot<RAMChip>{0x8000, 65536, 0, "IRM"},
    Slot<ROMChip>{0xC000,  8192, 0, "BASIC ROM"},
    Slot<ROMChip>{0xE000,  8192, 0, "CAOS ROM"}
);

// BusTraits — selects the correct manifest per variant
template<KC85Variant V> struct KC85BusTraits;

template<> struct KC85BusTraits<KC85Variant::KC85_2> {
    static constexpr const auto& kManifest = kKC852Chips;
    using Spec = ManifestBusSpec<kKC852Chips, 16, 8>;
};

template<> struct KC85BusTraits<KC85Variant::KC85_3> {
    static constexpr const auto& kManifest = kKC853Chips;
    using Spec = ManifestBusSpec<kKC853Chips, 16, 8>;
};

template<> struct KC85BusTraits<KC85Variant::KC85_4> {
    static constexpr const auto& kManifest = kKC854Chips;
    using Spec = ManifestBusSpec<kKC854Chips, 16, 8>;
};

// ── System ───────────────────────────────────────────────────────────────
template<KC85Variant V>
class KC85System : public System {
    using Traits = KC85VariantTraits<V>;
public:
    KC85System();
    ~KC85System() override;

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
    // ── Chips ────────────────────────────────────────────────────────────
    U880*                cpu_  = nullptr;     // U880 @ 1.7734 MHz
    z80_pio_t            pio1_;               // U855 PIO (system + keyboard)
    z80_pio_t            pio2_;               // U855 PIO (module system)
    z80_ctc_t            ctc_;                // U857 CTC (timing + sound + tape)
    kc85_module_system_t modules_;            // Expansion module slot controller

    // ── Memory — owned by BusMemory, accessed via chip_as<>() ────────────
    ROMChip* basic_rom_chip_ = nullptr;   // KC85/3, /4 only
    ROMChip* caos_rom_chip_  = nullptr;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = KC85BusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using Mem = BusMemory<typename BT::Spec>;
    Bus bus_;
    Mem bus_mem_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[kc85_constants::FB_WIDTH *
                          kc85_constants::FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[kc85_constants::KEYBOARD_ROWS] = {};

    // ── Banking state ────────────────────────────────────────────────────
    uint8_t bank_ctrl_     = 0;      // Port $84 value (KC85/4 only)
    uint8_t bank_ctrl2_    = 0;      // Port $86 value (KC85/4 only)
    bool    irm_enabled_   = false;  // PIO B bit 2: video RAM access enabled
    bool    caos_rom_on_   = true;   // PIO B bit 0: CAOS ROM enabled
    bool    basic_rom_on_  = false;  // PIO B bit 6: BASIC ROM enabled
    uint8_t active_plane_  = 0;      // Display plane (KC85/4: 0 or 1)

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = KC85_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = kc85_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    void        configure_bus_memory_map();   // Initial banking setup after apply()
    void        update_bank_state();          // Remap pages on PIO B / port write
    bus_state_t io_tick(bus_state_t pins);
    bool        load_roms();
};
