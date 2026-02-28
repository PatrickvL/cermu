#pragma once
/*
 * nes_system.h - Complete NES System Implementation
 *
 * This file provides a complete Nintendo Entertainment System implementation
 * featuring hardware-accurate components and precise timing.
 *
 * FEATURES:
 * =========
 * - Hardware-accurate NES 6502 CPU with integrated APU
 * - Cycle-accurate PPU (Picture Processing Unit) with proper timing
 * - Memory management unit with accurate mapper support
 * - Cartridge system with multiple mapper types
 * - Controller input with proper timing
 * - Audio/Video output interfaces
 * - Save state support
 * - Region support (NTSC/PAL)
 */

#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include <string>

#include "../../chip/cpu/fam65xx/ricoh_2a03.h"
#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include "../../core/emulated_system.h"
#include "../../core/formats/nsf_format.h"
#include "cartridge/nes_mapper.h"
#include "cartridge/nes_cartridge.h"
#include "ppu/nes_ppu.h"
#include "bus/nes_bus.h"

// NES default bus state — initial pin values before any chip asserts.
// RW=1 (read mode), active-low signals NMI/IRQ/RES start HIGH (inactive).
#define NES_BUS_DEFAULT_STATE \
    (BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_NMI_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_RES_BIT))

// Forward declarations
namespace nes_system {
    class Controller;
    class NsfCartridge;
}

// ============================================================================
// NES SYSTEM CONSTANTS
// ============================================================================

namespace nes_constants {
    // Timing constants
    constexpr uint32_t CPU_FREQ_NTSC = 1789773;  // Hz
    constexpr uint32_t CPU_FREQ_PAL = 1662607;   // Hz
    constexpr uint32_t PPU_FREQ_NTSC = 5369318;  // Hz (3x CPU)
    constexpr uint32_t PPU_FREQ_PAL = 4987821;   // Hz (3x CPU)
    
    // Screen dimensions
    constexpr uint32_t SCREEN_WIDTH = 256;
    constexpr uint32_t SCREEN_HEIGHT = 240;
    constexpr uint32_t TOTAL_SCANLINES_NTSC = 262;
    constexpr uint32_t TOTAL_SCANLINES_PAL = 312;
    
    // Memory layout
    constexpr uint16_t RAM_START = 0x0000;
    constexpr uint16_t RAM_END = 0x07FF;
    constexpr uint16_t RAM_MIRRORS_END = 0x1FFF;
    constexpr uint16_t PPU_REGS_START = 0x2000;
    constexpr uint16_t PPU_REGS_END = 0x2007;
    constexpr uint16_t PPU_MIRRORS_END = 0x3FFF;
    constexpr uint16_t APU_IO_REGS_START = 0x4000;
    constexpr uint16_t APU_IO_REGS_END = 0x4017;
    constexpr uint16_t CARTRIDGE_START = 0x4020;
    constexpr uint16_t CARTRIDGE_END = 0xFFFF;
}

// PPU class now in ppu/nes_ppu.h (included above)
// Cartridge class now in cartridge/nes_cartridge.h (included above)
// Bus struct now in bus/nes_bus.h (included above)
// MemoryBus class removed in Phase 2 — replaced by nes_bus_t + inline dispatch

namespace nes_system {

// ============================================================================
// CONTROLLER INPUT
// ============================================================================

class Controller {
public:
    enum Button {
        RIGHT  = 0x01,
        LEFT   = 0x02,
        DOWN   = 0x04,
        UP     = 0x08,
        START  = 0x10,
        SELECT = 0x20,
        B      = 0x40,
        A      = 0x80
    };
    
private:
    uint8_t controller_state = 0x00;
    uint8_t controller_register = 0x00;
    
public:
    Controller() = default;
    
    void write(uint8_t data) {
        (void)data;
        controller_register = controller_state;
    }
    
    uint8_t read() {
        uint8_t data = (controller_register & 0x80) > 0;
        controller_register <<= 1;
        return data;
    }
    
    void set_button_state(Button button, bool pressed) {
        if (pressed) {
            controller_state |= button;
        } else {
            controller_state &= ~button;
        }
    }
    
    uint8_t get_state() const { return controller_state; }
};

// ============================================================================
// Nintendo system variant (compile-time template parameter)
// ============================================================================

enum class NintendoVariant { NES, FAMICOM };

template<NintendoVariant V> struct NintendoVariantTraits;

template<> struct NintendoVariantTraits<NintendoVariant::NES> {
    static constexpr bool is_famicom    = false;
    static constexpr const char* name   = "NES";
    static constexpr const char* full_name = "Nintendo Entertainment System";
    static constexpr const char* short_id = "NES";
    static constexpr const char* description =
        "Nintendo Entertainment System (1985)";
};

template<> struct NintendoVariantTraits<NintendoVariant::FAMICOM> {
    static constexpr bool is_famicom    = true;
    static constexpr const char* name   = "Famicom";
    static constexpr const char* full_name = "Nintendo Famicom";
    static constexpr const char* short_id = "FC";
    static constexpr const char* description =
        "Nintendo Family Computer (1983) \u2014 expansion audio, hardwired controllers, microphone";
};

// ============================================================================
// MAIN NINTENDO SYSTEM (NES / Famicom)
// ============================================================================

template<NintendoVariant V>
class NintendoSystem : public EmulatedSystem {
    using Traits = NintendoVariantTraits<V>;

private:
    // Core components
    RICOH_2A03* cpu_;
    bus_state_t pins_;  // Persistent CPU bus state across ticks
    std::shared_ptr<PPU> ppu_;
    std::shared_ptr<Cartridge> cartridge_;
    nes_bus::nes_bus_t bus_;                     // Page-pointer bus (replaces MemoryBus)
    std::array<Controller, 2> controllers_;     // Player 1 & 2 controllers
    
    // System state
    bool is_pal_;
    bool system_ready_;
    uint32_t cycles_per_frame_;
    bool initialized_;
    
    // Audio buffer
    std::vector<float> audio_buffer_;
    uint32_t audio_sample_rate_;
    uint32_t audio_samples_per_frame_;
    uint32_t audio_sample_counter_;
    
    // Timing
    double residual_time_;
    
public:
    NintendoSystem();
    ~NintendoSystem() override;
    
    // EmulatedSystem interface - System identification
    const SystemDescriptor& get_descriptor() const override;
    static const SystemDescriptor& static_descriptor();
    
    // EmulatedSystem interface - Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    
    // EmulatedSystem interface - System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    
    // EmulatedSystem interface - Execution
    void tick() override;
    void run_frame() override;
    
    // EmulatedSystem interface - File loading
    bool load_file(const char* filepath) override;
    
    // EmulatedSystem interface - Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
    
    // EmulatedSystem interface - Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // EmulatedSystem interface - GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Window title metadata
    const char* get_mode_label() const override;
    std::string get_subtitle_info() const override;
    
    // EmulatedSystem interface - Emulation control
    void set_speed_multiplier(float multiplier) override;

    // Audio output — drains NES APU sample buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

    // NES-specific public methods
    void eject_cartridge();
    void power_cycle();
    void set_controller_state(int controller, uint8_t state);
    void press_button(int controller, Controller::Button button);
    void release_button(int controller, Controller::Button button);
    const std::vector<uint32_t>& get_screen() const;
    const std::vector<float>& get_audio_buffer() const { return audio_buffer_; }
    void clear_audio_buffer() { audio_buffer_.clear(); audio_sample_counter_ = 0; }
    const std::vector<uint32_t>& get_pattern_table(int table, uint8_t palette) const;
    void set_audio_sample_rate(uint32_t rate);
    bool save_state(const std::string& filename) const;
    bool load_state(const std::string& filename);
    bool is_cartridge_loaded() const { return cartridge_ != nullptr; }
    bool is_system_ready() const override { return system_ready_; }

    // Debug / test harness memory access (read-only, no side-effects)
    uint8_t peek_memory(uint16_t addr) const;
    uint16_t get_cpu_pc() const;
    
private:
    void setup_audio_timing();
    void setup_connector_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    /// Register all NES chips into registered_chips_ for the Hardware menu.
    void register_nes_chips();
    void clock();

    // =========================================================================
    // NSF PLAYER STATE
    // =========================================================================
    // When an NSF file is loaded, we keep a copy of its header and payload
    // so the user can switch subtunes interactively (digits 0-9 for direct
    // selection, left/right cursor keys for prev/next with wrapping).
    // =========================================================================
    bool nsf_player_active_ = false;             ///< True while an NSF file is playing
    nsf_header_t active_nsf_header_{};           ///< Copy of the loaded NSF header
    std::vector<uint8_t> active_nsf_data_;       ///< Copy of original payload bytes
    uint16_t active_nsf_subtune_ = 0;            ///< Current 0-based subtune index
    std::shared_ptr<NsfCartridge> nsf_cartridge_; ///< NSF cartridge for bank/data management

    /** Handle NSF player keyboard shortcuts (subtune selection).
     *  Returns true if the key was consumed (should not be forwarded). */
    bool handle_nsf_player_key(SDL_Keycode key);
};

// Convenience type aliases
using NESSystem     = NintendoSystem<NintendoVariant::NES>;
using FamicomSystem = NintendoSystem<NintendoVariant::FAMICOM>;

} // namespace nes_system
