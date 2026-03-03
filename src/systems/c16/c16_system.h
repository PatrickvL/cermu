#pragma once

#include "../commodore/commodore_system.h"
#include "../../core/system_lines.h"
#include "../../chip/memory/memory_chip.h"
#include "../../chip/video/ted/ted7360.h"
#include "../../chip/cpu/fam65xx/mos7501.h"

// C264 series (C16/C116/Plus4) default bus state — derived from CPU.
// CSG7501 provides: RW, RDY, IRQ, AEC.  (No NMI — NO_NMI_LINE flag.)
#define C264_BUS_DEFAULT_STATE  CSG7501::default_bus_state()

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
    static constexpr const char* description =
        "Commodore 16 (1984) - 16KB RAM, TED 7360 graphics and sound";
};

template<> struct C264SeriesVariantTraits<C264SeriesVariant::C116> {
    static constexpr bool has_user_port  = false;
    static constexpr size_t default_ram  = 16384;
    static constexpr const char* name    = "C116";
    static constexpr const char* full_name = "Commodore 116";
    static constexpr const char* short_id = "C116";
    static constexpr const char* description =
        "Commodore 116 (1984) - 16KB RAM, TED 7360, chiclet keyboard variant of C16";
};

template<> struct C264SeriesVariantTraits<C264SeriesVariant::PLUS4> {
    static constexpr bool has_user_port  = true;
    static constexpr size_t default_ram  = 65536;
    static constexpr const char* name    = "Plus/4";
    static constexpr const char* full_name = "Commodore Plus/4";
    static constexpr const char* short_id = "PLUS4";
    static constexpr const char* description =
        "Commodore Plus/4 (1984) - 64KB RAM, TED 7360, built-in 3-PLUS-1 software";
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

    // File loading
    bool load_file(const char* filepath) override;

    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Hardware traits (compile-time variant-specific)
    static HardwareTraits create_hardware_traits();

    // File probe — returns confidence + optimal configuration for this TED variant
    static SystemProbeResult probe_file_static(
        const format_descriptor_t* matched_format,
        const char* filepath,
        const uint8_t* data, size_t size);

    // Static descriptor accessor (usable without an instance, e.g. for REGISTER_SYSTEM)
    static const SystemDescriptor& static_descriptor();

private:
    // Chip instances
    CSG7501* cpu_ = nullptr;
    ted7360_t* ted_;
    bus_state_t bus_state_;

    // Memory chips — owned by registered_chips_ (base class), borrowed here
    MemoryChip* ram_         = nullptr;  // Up to 64KB RAM (C16/C116 use 16KB, Plus/4 uses 64KB)
    MemoryChip* basic_rom_   = nullptr;  // BASIC ROM $8000-$BFFF (16KB)
    MemoryChip* kernal_rom_  = nullptr;  // Kernal ROM $C000-$FFFF (16KB)
    size_t  ram_size_ = 16384;           // Cached configured RAM size (updated in apply_configuration)

    // System state
    bool initialized_;

    // Helper methods
    bool load_roms();
    bus_state_t mem_tick(bus_state_t s);
    void setup_connector_ports();
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
