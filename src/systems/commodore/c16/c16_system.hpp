#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/video/ted/ted7360.hpp"
#include "chip/cpu/fam65xx/mos7501.hpp"
#include "chip/io/mos6529.hpp"
#include <memory>

// C264 series (C16/C116/Plus4) default bus state — derived from CPU.
// CSG7501 provides: RW, RDY, IRQ, AEC.  (No NMI — NO_NMI_LINE flag.)
#define C264_BUS_DEFAULT_STATE  CSG7501::default_bus_state()

#include "core/formats/format_handler.hpp"
#include <string>
#include <cstdint>

// ============================================================================
// TED-based system variant (compile-time template parameter)
// ============================================================================

enum class C264SeriesVariant { C16, C116, PLUS4 };

// ============================================================================
// Compile-time variant traits
// ============================================================================

template<C264SeriesVariant V> struct C264SeriesVariantTraits;

template<> struct C264SeriesVariantTraits<C264SeriesVariant::C16> {
    static constexpr bool has_user_port  = false;
    static constexpr size_t default_ram  = 16384;
    static constexpr const char* name    = "C16";
    static constexpr const char* full_name = "Commodore 16";
    static constexpr const char* short_id = "C16";
    static constexpr const char* data_folder = "c16";
    static std::vector<const char*> get_aliases() { return {"C16"}; }
    static constexpr const char* description =
        "Commodore 16 (1984) - 16KB RAM, TED 7360 graphics and sound";
};

template<> struct C264SeriesVariantTraits<C264SeriesVariant::C116> {
    static constexpr bool has_user_port  = false;
    static constexpr size_t default_ram  = 16384;
    static constexpr const char* name    = "C116";
    static constexpr const char* full_name = "Commodore 116";
    static constexpr const char* short_id = "C116";
    static constexpr const char* data_folder = "c16";
    static std::vector<const char*> get_aliases() { return {"C116"}; }
    static constexpr const char* description =
        "Commodore 116 (1984) - 16KB RAM, TED 7360, chiclet keyboard variant of C16";
};

template<> struct C264SeriesVariantTraits<C264SeriesVariant::PLUS4> {
    static constexpr bool has_user_port  = true;
    static constexpr size_t default_ram  = 65536;
    static constexpr const char* name    = "Plus/4";
    static constexpr const char* full_name = "Commodore Plus/4";
    static constexpr const char* short_id = "PLUS4";
    static constexpr const char* data_folder = "c16";
    static std::vector<const char*> get_aliases() { return {"Plus4", "Plus/4", "Plus-4"}; }
    static constexpr const char* description =
        "Commodore Plus/4 (1984) - 64KB RAM, TED 7360, built-in 3-PLUS-1 software";
};

// ============================================================================
// C264 ROM Bank Select ($FDD0-$FDDF) — address-decoded latch
// ============================================================================
//
// Writing to $FDDx selects the active ROM bank pair.  The written data byte
// is irrelevant — only the address bits A0-A3 matter:
//   A1:A0 = low ROM bank  (0=BASIC, 1=Function LO, 2=Cartridge LO)
//   A3:A2 = high ROM bank (0=KERNAL, 1=Function HI, 2=Cartridge HI)
//
// Reads return open bus.  On write, fires an optional callback so the system
// can update banking immediately (no per-tick polling).

using rom_bank_change_fn = void (*)(void* user_data);

class c264_rom_bank_select_t : public ChipBase {
public:
    c264_rom_bank_select_t()
        : ChipBase(ChipInfo{"ROM Bank Select", "ROM Bank"}) {
        category_ = "Logic";
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        return bus;   // write-only; reads return open bus
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t nibble = BUS_GET_ADDR(bus) & 0x0F;
        uint8_t new_low  = nibble & 0x03;
        uint8_t new_high = (nibble >> 2) & 0x03;
        if (new_low != low_bank || new_high != high_bank) {
            low_bank  = new_low;
            high_bank = new_high;
            if (on_change) on_change(on_change_user_data);
        }
        return bus;
    }

    void reset() override {
        low_bank  = 0;   // BASIC
        high_bank = 0;   // KERNAL
        // on_change / on_change_user_data are preserved across reset
    }

    uint8_t low_bank  = 0;     // 0=BASIC, 1=Function LO, 2=Cartridge LO
    uint8_t high_bank = 0;     // 0=KERNAL, 1=Function HI, 2=Cartridge HI
    rom_bank_change_fn on_change = nullptr;
    void* on_change_user_data    = nullptr;
};

// ============================================================================
// C264 chip manifest — declarative memory layout
// ============================================================================
//
// Memory map (CPU view):
//   $0000-$7FFF  RAM (always)
//   $8000-$BFFF  BASIC ROM (read, when ROM enabled) / RAM
//   $C000-$FCFF  KERNAL ROM (read, when ROM enabled) / RAM
//   $FD00-$FDFF  I/O area — MaskedSubTable with individual chip MMIO:
//                  $FD10-$FD1F  PIO1 (MOS 6529B) — user port / tape sense
//                  $FD30-$FD3F  PIO2 (MOS 6529B) — keyboard row select
//                  $FDD0-$FDDF  ROM bank select — address-decoded latch
//                  Other $FD addresses → open bus (no chip)
//   $FE00-$FEFF  KERNAL ROM (cont.) / RAM
//   $FF00-$FF3F  TED registers — MaskedSubTable sub-page MMIO
//   $FF40-$FFFF  KERNAL ROM (cont.) / RAM — MaskedSubTable base
//
// ROM banking is controlled by TED latch writes ($FF3E/$FF3F).
// ROM bank pair selected by writes to $FDD0-$FDDF.
// RAM size varies: 16 KB (C16/C116) with mirroring, 64 KB (Plus/4).
//
inline constexpr auto kC264Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM",        0, 65536, 0, 0, {}},
    Slot<ROMChip>{0x8000, 16384, 0, "BASIC ROM",  0, 16384, 1, 0, {}}.with_rom("basic.318006-01.bin|basic.rom|318006-01.bin"),   // overlay group 1
    Slot<ROMChip>{0xC000, 16384, 0, "KERNAL ROM", 0, 16384, 1, 0, {}}.with_rom("kernal.318004-05.bin|kernal.rom|318004-05.bin"), // overlay group 1
    // Non-bus chip — factory-created, not address-decoded
    Slot<CSG7501>               {0, 0, 0, "CSG 7501",         0, 0, 0, 0, {}},
    // MMIO chips — address-decoded by MemoryBus via MaskedSubTables
    Slot<ted7360_t>             {0xFF00, 0, 0xFFC0, "TED 7360",        0, 0, 0, 0, {}},  // page $FF sub-table 0
    Slot<mos6529_t>             {0xFD10, 0, 0xFFF0, "MOS 6529B PIO1",  0, 0, 0, 0, {}},  // page $FD sub-table 1
    Slot<mos6529_t>             {0xFD30, 0, 0xFFF0, "MOS 6529B PIO2",  0, 0, 0, 0, {}},  // page $FD sub-table 1
    Slot<c264_rom_bank_select_t>{0xFDD0, 0, 0xFFF0, "ROM Bank Select", 0, 0, 0, 0, {}}   // page $FD sub-table 1
);

// MaskedSubTable indices (determined by manifest slot order during apply())
namespace c264_sub {
    inline constexpr size_t kTedPage = 0;    // page $FF — TED registers
    inline constexpr size_t kIoPage  = 1;    // page $FD — PIO1, PIO2, ROM bank select
}

struct C264BusTraits {
    static constexpr const auto& kManifest = kC264Chips;
    using Spec = ManifestBusSpec<kC264Chips, 16, 8, 2>;  // 2 viewers: CPU + TED video
};

// Viewer IDs for the C264 bus
namespace c264_viewer {
    inline constexpr size_t kCpu      = 0;  // CPU memory map (controlled by rom_enabled)
    inline constexpr size_t kTedVideo = 1;  // TED video fetches (controlled by video_romsel)
}

// Value-typed chips: CPU + TED (video) + PIO1 + PIO2 + ROM bank select.
// TED is default-constructed and configured via init(desc) in initialize().
struct C264Chipset : CoreChips<CSG7501, ted7360_t, NoChip, mos6529_t> {
    mos6529_t               pio2;
    c264_rom_bank_select_t  rom_bank;

    template<typename Board> void bind_extras(Board& board) {
        board.bind_chip(board.template find_index<mos6529_t>(1),             &pio2);
        board.bind_chip(board.template find_index<c264_rom_bank_select_t>(), &rom_bank);
    }
    template<typename Board> void register_extras(Board& board) {
        board.register_component(&pio2);
        board.register_component(&rom_bank);
    }
};

// ============================================================================
// Commodore264System — Commodore 264 Series Emulator (C16, C116, Plus/4)
// ============================================================================
/**
 * Template-based system for the Commodore 264 series (TED 7360 family).
 *
 * The "264 series" was Commodore's internal project codename for the
 * budget computer line released in 1984–1985.  All three machines share
 * the same CPU (MOS 7501), video/sound/IO chip (TED 7360), memory map,
 * and BASIC 3.5 ROM.  The only hardware-level differences are:
 *   - C16:    16 KB RAM, full-travel keyboard, no User Port
 *   - C116:   16 KB RAM, chiclet (rubber) keyboard, no User Port
 *   - Plus/4: 64 KB RAM, full-travel keyboard, User Port, built-in
 *             "3-PLUS-1" productivity ROM
 *
 * These differences are captured by C264SeriesVariantTraits<V> and selected at
 * compile time via the template parameter.
 *
 * Inherits from CommodoreSystem which provides shared Commodore 8-bit
 * infrastructure (keyboard mapper, configuration, speed control).
 */
template<C264SeriesVariant V>
class Commodore264System : public CommodoreSystem {
    using Traits = C264SeriesVariantTraits<V>;

public:
    Commodore264System();
    ~Commodore264System() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration management
    bool apply_configuration() override;

    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    void* get_video_port_ptr() override { return video_port_.get(); }

    // Hardware traits (compile-time variant-specific)
    static HardwareTraits create_hardware_traits();

    // --- Test / debug accessors ---
    CSG7501*     cpu()       { return cpu_; }
    ted7360_t*   ted()       { return &board_.video(); }
    RAMChip*  ram()       { return ram_; }
    const CSG7501*    cpu() const { return cpu_; }
    const ted7360_t*  ted() const { return &board_.video(); }
    const RAMChip* ram() const { return ram_; }

    // Debug cart ($FDCF) — VICE convention for Plus4 test programs.
    // When enabled, writes to $FDCF are captured instead of being silently ignored.
    // $00 = test passed, $FF = test failed (matching C64 $D7FF convention).
    // Not real hardware — captured in mem_tick() post-processing.
    void    enable_debug_cart(bool enable) { debug_cart_enabled_ = enable; }
    bool    debug_cart_written() const     { return debug_cart_written_; }
    uint8_t debug_cart_value() const       { return debug_cart_value_; }
    void    clear_debug_cart()             { debug_cart_written_ = false; debug_cart_value_ = 0; }

    // File probe — returns confidence + optimal configuration for this TED variant
    static SystemProbeResult probe_file_static(
        const format_descriptor_t* matched_format,
        const char* filepath,
        const uint8_t* data, size_t size);

    // Static descriptor accessor (usable without an instance, e.g. for REGISTER_SYSTEM)
    static const SystemDescriptor& static_descriptor();

private:
    // Chip instances
    CSG7501* cpu_ = nullptr;          // MOS 7501/8501 CPU — owned by board_
    ted7360_t* ted_ = nullptr;        // Convenience pointer: &board_.video()
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output
    std::unique_ptr<AudioPort> audio_port_;            // Audio signal output
    bus_state_t bus_state_;

    // ── Memory bus (declarative manifest + page-pointer dispatch) ────────
    using Bus = MemoryBus<C264BusTraits::Spec>;
    using MainBoard = Board<C264BusTraits::Spec, C264Chipset>;
    Bus bus_;
    MainBoard board_{kC264Chips};

    // Convenience chip pointers (owned by board_, accessed via chip_as)
    RAMChip* ram_         = nullptr;  // Up to 64KB RAM (C16/C116 use 16KB, Plus/4 uses 64KB)
    ROMChip* basic_rom_   = nullptr;  // BASIC ROM $8000-$BFFF (16KB)
    ROMChip* kernal_rom_  = nullptr;  // Kernal ROM $C000-$FFFF (16KB)
    mos6529_t* pio1_      = nullptr;  // MOS 6529B PIO1 ($FD10) — user port + tape sense
    mos6529_t* pio2_      = nullptr;  // MOS 6529B PIO2 ($FD30) — keyboard row select
    c264_rom_bank_select_t* rom_bank_ = nullptr;  // ROM bank select ($FDD0)
    size_t  ram_size_ = 16384;           // Cached configured RAM size (updated in apply_configuration)

    // Debug cart state (VICE test convention, not real hardware)
    bool    debug_cart_enabled_ = false;
    bool    debug_cart_written_ = false;
    uint8_t debug_cart_value_   = 0;

    // System state
    bool initialized_;

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    void inject_keys(const char* str) override;
    bool is_system_initialized() const override { return initialized_; }
    int get_iec_port_index() const override { return 2; }   // IEC Serial Bus
    int get_cassette_port_index() const override { return 3; }  // Cassette Port

    // Helper methods
    bool load_roms();
    bus_state_t mem_tick(bus_state_t s);
    void setup_ram_mirroring();                 // configure chip_info_ mask for current ram_size_
    void build_banking_snapshots();             // populate overlay snapshots via generic builder
    void apply_cpu_banking();                   // load CPU viewer snapshot for current rom_enabled
    void apply_ted_video_banking();             // load TED viewer snapshot for current video_romsel
    void setup_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;
    static uint8_t io_port_in(void* user_data);
    static void io_port_out(uint8_t data, void* user_data);
    static uint8_t ted_keyboard_scan(void* user_data, uint8_t column);
    static uint8_t ted_mem_read(void* user_data, uint16_t address);
    static void ted_banking_changed(void* user_data, uint8_t changes);
    static void set_cpu_pc(void* user_data, uint16_t addr);

    // Pre-computed banking mode snapshots for instant mode switching.
    // Indexed as snapshots_[viewer][mode]: mode 0 = RAM only, mode 1 = ROM overlaid.
    // Generated by Board::build_overlay_snapshots() from manifest overlay groups.
    static constexpr size_t kNumViewers = 2;
    static constexpr size_t kNumModes   = kC264Chips.overlay_mode_count();  // 2
    std::array<std::array<Bus::Snapshot, kNumModes>, kNumViewers> snapshots_;
};

// Convenience type aliases
using C16System   = Commodore264System<C264SeriesVariant::C16>;
using C116System  = Commodore264System<C264SeriesVariant::C116>;
using Plus4System = Commodore264System<C264SeriesVariant::PLUS4>;
