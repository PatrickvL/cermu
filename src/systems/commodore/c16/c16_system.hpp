#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/video/ted/ted7360.hpp"
#include "chip/cpu/fam65xx/mos7501.hpp"
#include "core/chip.hpp"

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
// C264 I/O page chip ($FD00-$FDFF) — PIO2 + debug cart
// ============================================================================
//
// The $FD page on the C264 series contains several I/O registers:
//   $FD00-$FD0F  PIO1 (MOS 6529B) — keyboard column (active-low latched)
//   $FD10-$FD1F  ROM bank select (active ROM socket, Plus/4 only)
//   $FD30-$FD3F  PIO2 (MOS 6529B) — keyboard row select register
//   $FDCF        Debug cart register (VICE convention for test programs)
//
// This chip owns the PIO2 and debug-cart state.  Dispatched via full-page
// MMIO from MemoryBus — no manual io_tick() required.

class c264_io_page_t : public ChipBase {
public:
    c264_io_page_t() : ChipBase(ChipInfo{"C264 I/O Page", "I/O"}) {
        category_ = "I/O";
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint16_t addr = BUS_GET_ADDR(bus);
        uint8_t data = 0xFF;
        if ((addr & 0xFFF0) == 0xFD30) {
            data = pio2_kbd;
        }
        BUS_SET_DATA(bus, data);
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint16_t addr = BUS_GET_ADDR(bus);
        uint8_t data = BUS_GET_DATA(bus);
        if ((addr & 0xFFF0) == 0xFD30) {
            pio2_kbd = data;
        } else if (debug_cart_enabled && addr == 0xFDCF) {
            debug_cart_value = data;
            debug_cart_written = true;
        }
        return bus;
    }

    void reset() override {
        pio2_kbd = 0xFF;
        debug_cart_written = false;
        debug_cart_value = 0;
    }

    uint8_t pio2_kbd          = 0xFF;   // All rows deselected on reset
    bool    debug_cart_enabled  = false;
    bool    debug_cart_written  = false;
    uint8_t debug_cart_value    = 0;
};

// ============================================================================
// C264 chip manifest — declarative memory layout
// ============================================================================
//
// Memory map (CPU view):
//   $0000-$7FFF  RAM (always)
//   $8000-$BFFF  BASIC ROM (read, when ROM enabled) / RAM
//   $C000-$FCFF  KERNAL ROM (read, when ROM enabled) / RAM
//   $FD00-$FDFF  I/O area — full-page MMIO (c264_io_page_t)
//   $FE00-$FEFF  KERNAL ROM (cont.) / RAM
//   $FF00-$FF3F  TED registers — sub-page MMIO via MaskedSubTable
//   $FF40-$FFFF  KERNAL ROM (cont.) / RAM — MaskedSubTable base
//
// ROM banking is controlled by TED latch writes ($FF3E/$FF3F).
// RAM size varies: 16 KB (C16/C116) with mirroring, 64 KB (Plus/4).
//
inline constexpr auto kC264Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0x8000, 16384, 0, "BASIC ROM"},
    Slot<ROMChip>{0xC000, 16384, 0, "KERNAL ROM"},
    // Non-bus chip — factory-created, not address-decoded
    Slot<CSG7501>       {0, 0, 0, "CSG 7501"},
    // MMIO chips — address-decoded by MemoryBus
    Slot<ted7360_t>     {0xFF00, 0, 0xFFC0, "TED 7360"},   // sub-page: (addr & 0xFFC0) == 0xFF00
    Slot<c264_io_page_t>{0xFD00, 0, 0,      "I/O Page"}    // full-page: $FD00-$FDFF
);

namespace c264_slot {
    inline constexpr size_t kRam       = 0;
    inline constexpr size_t kBasicRom  = 1;
    inline constexpr size_t kKernalRom = 2;
    inline constexpr size_t kTed       = 4;
    inline constexpr size_t kIoPage    = 5;
}

struct C264BusTraits {
    static constexpr const auto& kManifest = kC264Chips;
    using Spec = ManifestBusSpec<kC264Chips, 16, 8>;
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

    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Hardware traits (compile-time variant-specific)
    static HardwareTraits create_hardware_traits();

    // --- Test / debug accessors ---
    CSG7501*     cpu()       { return cpu_; }
    ted7360_t*   ted()       { return ted_; }
    RAMChip*  ram()       { return ram_; }
    const CSG7501*    cpu() const { return cpu_; }
    const ted7360_t*  ted() const { return ted_; }
    const RAMChip* ram() const { return ram_; }

    // Debug cart ($FDCF) — VICE convention for Plus4 test programs.
    // When enabled, writes to $FDCF are captured instead of being silently ignored.
    // $00 = test passed, $FF = test failed (matching C64 $D7FF convention).
    void    enable_debug_cart(bool enable) { if (io_) io_->debug_cart_enabled = enable; }
    bool    debug_cart_written() const     { return io_ && io_->debug_cart_written; }
    uint8_t debug_cart_value() const       { return io_ ? io_->debug_cart_value : 0; }
    void    clear_debug_cart()             { if (io_) { io_->debug_cart_written = false; io_->debug_cart_value = 0; } }

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
    ted7360_t* ted_;
    bus_state_t bus_state_;

    // ── Memory bus (declarative manifest + page-pointer dispatch) ────────
    using Bus = MemoryBus<C264BusTraits::Spec>;
    using MainBoard = Board<C264BusTraits::Spec>;
    Bus bus_;
    MainBoard board_{kC264Chips};

    // Convenience chip pointers (owned by board_, accessed via chip_as)
    RAMChip* ram_         = nullptr;  // Up to 64KB RAM (C16/C116 use 16KB, Plus/4 uses 64KB)
    ROMChip* basic_rom_   = nullptr;  // BASIC ROM $8000-$BFFF (16KB)
    ROMChip* kernal_rom_  = nullptr;  // Kernal ROM $C000-$FFFF (16KB)
    c264_io_page_t* io_   = nullptr;  // I/O page chip ($FD00-$FDFF) — MMIO, owned by board_
    size_t  ram_size_ = 16384;           // Cached configured RAM size (updated in apply_configuration)

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
    void setup_ram_mirroring();                 // configure page pointers for current ram_size_
    void update_rom_banking();                  // switch read pages on rom_enabled change
    void setup_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;
    static uint8_t io_port_in(void* user_data);
    static void io_port_out(uint8_t data, void* user_data);
    static uint8_t ted_keyboard_scan(void* user_data, uint8_t column);
    static uint8_t ted_mem_read(void* user_data, uint16_t address);
    static void set_cpu_pc(void* user_data, uint16_t addr);
};

// Convenience type aliases
using C16System   = Commodore264System<C264SeriesVariant::C16>;
using C116System  = Commodore264System<C264SeriesVariant::C116>;
using Plus4System = Commodore264System<C264SeriesVariant::PLUS4>;
