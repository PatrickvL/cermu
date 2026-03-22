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
#include "utils/ring_buffer.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "core/audio_thread.hpp"
#include "core/board.hpp"
#include "core/core_chipset.hpp"
#include "core/chip_manifest.hpp"
#include "chip/sound/nes_apu_synth_engine.hpp"

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

#include "chip/cpu/fam65xx/ricoh_2a03.hpp"
#include "core/chip.hpp"
#include "core/system_lines.hpp"
#include "core/system.hpp"
#include "core/formats/nsf_format.hpp"
#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/nes_cartridge.hpp"
#include "chip/video/nes_ppu/nes_ppu.hpp"
#include "systems/nes/bus/nes_bus.hpp"

// NES default bus state — derived from CPU.
// RICOH_2A03 provides: RW, RDY, IRQ, NMI, RES.
#define NES_BUS_DEFAULT_STATE \
    (RICOH_2A03::default_bus_state())

namespace nes_system {

// =============================================================================
// NES chip manifest — non-bus chip declarations for Board typed access.
// Actual memory dispatch uses nes_bus_t (page-pointer bus), not MemoryBus.
// =============================================================================
inline constexpr auto kNESChips = make_chip_manifest(
    Slot<RICOH_2A03>{0, 0, 0, "Ricoh 2A03"},
    Slot<PPU>       {0, 0, 0, "Ricoh 2C02 PPU"}
);

using NESBusSpec = ManifestBusSpec<kNESChips, 16, 8>;

// ── NES Chipset — PPU in the Video slot for auto-palette discovery ──────
struct NESChipset : CoreChipset<RICOH_2A03, PPU> {};

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
    static constexpr const char* data_folder = "nes";
    static std::vector<const char*> get_aliases() { return {"NES"}; }
    static constexpr const char* description =
        "Nintendo Entertainment System (1985)";
};

template<> struct NintendoVariantTraits<NintendoVariant::FAMICOM> {
    static constexpr bool is_famicom    = true;
    static constexpr const char* name   = "Famicom";
    static constexpr const char* full_name = "Nintendo Famicom";
    static constexpr const char* short_id = "FC";
    static constexpr const char* data_folder = "nes";
    static std::vector<const char*> get_aliases() { return {"Famicom", "FC"}; }
    static constexpr const char* description =
        "Nintendo Family Computer (1983) \u2014 expansion audio, hardwired controllers, microphone";
};

// ============================================================================
// MAIN NINTENDO SYSTEM (NES / Famicom)
// ============================================================================

template<NintendoVariant V>
class NintendoSystem : public System {
    using Traits = NintendoVariantTraits<V>;

private:
    // Main board — typed Board with NESChipset for auto-palette discovery.
    // Memory dispatch is handled separately by nes_bus_t (page-pointer bus).
    using MainBoard = Board<NESBusSpec, NESChipset>;
    MainBoard board_{kNESChips};

    // Core components — CPU and PPU accessed via board_.cpu() / board_.video()
    bus_state_t pins_;               // Persistent CPU bus state across ticks
    std::unique_ptr<Cartridge> cartridge_;
    nes_bus::nes_bus_t bus_;                     // Page-pointer bus (replaces MemoryBus)
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output
    std::unique_ptr<AudioPort> audio_port_;            // Audio signal output
    
    // System state
    bool is_pal_;
    uint32_t cycles_per_frame_;
    bool initialized_;

    // OAM DMA controller state
    uint8_t  dma_page_ = 0;
    uint8_t  dma_addr_ = 0;
    uint8_t  dma_data_ = 0;
    bool     dma_transfer_ = false;
    bool     dma_dummy_ = true;

    // DMC DMA cycle stealing during OAM DMA.
    // On real hardware, DMC sample fetches "steal" cycles from an
    // in-progress OAM DMA: 1 halt cycle + 1 read cycle = 2 extra cycles
    // (plus 0-1 alignment wait depending on OAM DMA read/write phase).
    uint8_t  dmc_steal_phase_ = 0;   // 0=none, 1=halt, 2=read

    // Clock dividers (PPU-tick granularity)
    uint32_t system_clock_counter_ = 0;  // Total PPU ticks (saved to state)
    uint8_t  cpu_div_ = 0;               // PPU->CPU countdown (0 = CPU tick this cycle)
    bool     dma_odd_cycle_ = false;     // DMA even/odd toggle
    
    // Audio buffer — lock-free SPSC ring (single-threaded mode)
    AudioRingBuffer audio_ring_buf_{8192};
    uint32_t audio_sample_rate_;
    uint32_t audio_sample_counter_;
    uint32_t audio_sample_period_;      // cached: NTSC=37, PAL=33

    // Audio thread separation — when active, synthesis runs off the emu thread.
    // In single-threaded mode both are null/stopped and the legacy path applies.
    AudioThread audio_thread_;
    std::unique_ptr<NesApuSynthEngine> apu_synth_engine_;
    
    // Timing
    double residual_time_;
    
public:
    NintendoSystem();
    ~NintendoSystem() override;
    
    // System interface - System identification
    const SystemDescriptor& get_descriptor() const override;
    static const SystemDescriptor& static_descriptor();
    
    // System interface - Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    
    // System interface - System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    
    // System interface - Execution
    void tick() override;
    void run_frame() override;
    
    // System interface - File loading
    bool load_file(const char* filepath) override;
    
    // System interface - Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // System interface - GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Window title metadata
    const char* get_mode_label() const override;
    std::string get_subtitle_info() const override;

    // Audio output — drains NES APU sample buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

    // NES-specific public methods
    void eject_cartridge();
    void power_cycle();
    void set_controller_state(int controller, uint8_t state);
    void clear_audio_buffer() { audio_ring_buf_.reset(); audio_sample_counter_ = audio_sample_period_; }
    void set_audio_sample_rate(uint32_t rate);
    bool save_state(const std::string& filename) const;
    bool load_state(const std::string& filename);
    bool is_cartridge_loaded() const { return cartridge_ != nullptr; }

    // Debug / test harness memory access (no side-effects)
    uint8_t peek_memory(uint16_t addr) const;
    void poke_memory(uint16_t addr, uint8_t value);
    uint8_t peek_ppu_memory(uint16_t addr) const;
    uint16_t get_cpu_pc() const;
    void set_cpu_pc(uint16_t addr);
    
private:
    void setup_ports();
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
