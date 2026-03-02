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
#include <memory>
#include <vector>
#include <string>

// ============================================================================
// NES SYSTEM CONSTANTS
// ============================================================================

namespace nes_constants {
    // Timing constants
    constexpr uint32_t CPU_FREQ_NTSC = 1789773;  // Hz
    constexpr uint32_t CPU_FREQ_PAL = 1662607;   // Hz
    constexpr uint32_t CYCLES_PER_FRAME_NTSC = 29829;   // CPU_FREQ_NTSC / 60
    constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;        // Hz
    
    // Screen / scanline timing
    constexpr int32_t  TOTAL_SCANLINES_NTSC = 262;
    constexpr int32_t  TOTAL_SCANLINES_PAL = 312;
    constexpr int32_t  VBLANK_SCANLINE = 241;           // first VBlank scanline
    constexpr uint32_t DOTS_PER_SCANLINE = 341;          // PPU dots per scanline

    // CPU / bus sizing
    constexpr uint32_t CPU_RAM_SIZE = 2048;               // 2KB internal RAM

    // iNES format sizes
    constexpr uint32_t INES_CHR_BANK_SIZE = 8192;         // 8KB CHR-ROM/RAM bank
    constexpr uint32_t INES_PRG_RAM_DEFAULT = 8192;       // default 8KB PRG-RAM
}

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

// Forward declarations — none needed; all NES types included above.

// PPU class now in ppu/nes_ppu.h (included above)
// Cartridge class now in cartridge/nes_cartridge.h (included above)
// Bus struct now in bus/nes_bus.h (included above)
// MemoryBus class removed in Phase 2 — replaced by nes_bus_t + inline dispatch

namespace nes_system {

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
    bus_state_t pins_;               // Persistent CPU bus state across ticks
    std::shared_ptr<PPU> ppu_;
    std::shared_ptr<Cartridge> cartridge_;
    nes_bus::nes_bus_t bus_;                     // Page-pointer bus (replaces MemoryBus)
    
    // System state
    bool is_pal_;
    bool system_ready_;
    uint32_t cycles_per_frame_;
    bool initialized_;

    // OAM DMA controller state
    uint8_t  dma_page_ = 0;
    uint8_t  dma_addr_ = 0;
    uint8_t  dma_data_ = 0;
    bool     dma_transfer_ = false;
    bool     dma_dummy_ = true;

    // Clock dividers (PPU-tick granularity)
    uint32_t system_clock_counter_ = 0;  // Total PPU ticks (saved to state)
    uint8_t  cpu_div_ = 0;               // PPU->CPU countdown (0 = CPU tick this cycle)
    bool     dma_odd_cycle_ = false;     // DMA even/odd toggle
    
    // Audio buffer
    std::vector<float> audio_buffer_;
    uint32_t audio_sample_rate_;
    uint32_t audio_sample_counter_;
    uint32_t audio_sample_period_;      // cached: NTSC=37, PAL=33
    
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
    const std::vector<uint32_t>& get_screen() const;
    const std::vector<float>& get_audio_buffer() const { return audio_buffer_; }
    void clear_audio_buffer() { audio_buffer_.clear(); audio_sample_counter_ = audio_sample_period_; }
    void set_audio_sample_rate(uint32_t rate);
    bool save_state(const std::string& filename) const;
    bool load_state(const std::string& filename);
    bool is_cartridge_loaded() const { return cartridge_ != nullptr; }
    bool is_system_ready() const override { return system_ready_; }

    // Debug / test harness memory access (no side-effects)
    uint8_t peek_memory(uint16_t addr) const;
    void poke_memory(uint16_t addr, uint8_t value);
    uint8_t peek_ppu_memory(uint16_t addr) const;
    uint16_t get_cpu_pc() const;
    void set_cpu_pc(uint16_t addr);
    
private:
    void setup_connector_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    /// Register all NES chips into registered_chips_ for the Hardware menu.
    void register_nes_chips();

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
    bool nsf_bankswitched_ = false;              ///< True if the loaded NSF uses bank switching

    /** Handle NSF player keyboard shortcuts (subtune selection).
     *  Returns true if the key was consumed (should not be forwarded). */
    bool handle_nsf_player_key(SDL_Keycode key);
};

// Convenience type aliases
using NESSystem     = NintendoSystem<NintendoVariant::NES>;
using FamicomSystem = NintendoSystem<NintendoVariant::FAMICOM>;

} // namespace nes_system
