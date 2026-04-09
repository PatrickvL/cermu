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

// ============================================================================
// NES Manifest — chips + removable controller ports (7-pin, 48-pin expansion)
// ============================================================================

inline constexpr auto kNESManifest = make_manifest(
    // Chips
    Slot<RICOH_2A03>{.base_addr = 0x0000, .label = "Ricoh 2A03"},
    Slot<PPU>{.base_addr = 0x2000, .label = "Ricoh 2C02 PPU"},
    // Ports
    Slot<PortControllerNes>{.name = "Controller Port 1", .port_number = 1, .default_device = "nes_gamepad"},
    Slot<PortControllerNes>{.name = "Controller Port 2", .port_number = 2, .default_device = "nes_gamepad"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

// ============================================================================
// Famicom Manifest — same chips, hardwired controllers + 15-pin expansion
// ============================================================================

inline constexpr auto kFCManifest = make_manifest(
    // Chips
    Slot<RICOH_2A03>{.base_addr = 0x0000, .label = "Ricoh 2A03"},
    Slot<PPU>{.base_addr = 0x2000, .label = "Ricoh 2C02 PPU"},
    // Ports
    Slot<PortControllerNes>{.name = "Controller I (hardwired)",              .port_number = 1, .default_device = "nes_gamepad"},
    Slot<PortControllerNes>{.name = "Controller II (hardwired, microphone)", .port_number = 2, .default_device = "nes_gamepad"},
    Slot<PortExpansion>{.name = "Expansion Port (15-pin)"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kNESChipCount = decltype(kNESManifest)::chip_count;
using NESBusSpec = ManifestBusSpec<kNESManifest, 16, 8>;

struct NESBoard : Board<NESBusSpec> {
    using ComponentTuple = decltype(kNESManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases (ports are at indices 2+ but managed by the port framework)
    RICOH_2A03& cpu = std::get<0>(components_);
    PPU&        ppu = std::get<1>(components_);

    NESBoard() : Board(kNESManifest) {}
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
    using MainBoard = NESBoard;
    MainBoard board_;

    // Core components — CPU and PPU accessed via board_.cpu / board_.ppu
    bus_state_t pins_;               // Persistent CPU bus state across ticks
    std::unique_ptr<Cartridge> cartridge_;
    nes_bus::nes_bus_t bus_;                     // Page-pointer bus (replaces MemoryBus)
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
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

    // Boot warp — run at max speed until PPU rendering is enabled.
    // Many NES games spend 2-5 seconds initializing RAM/VRAM before
    // enabling BG/sprite rendering.  Warping through this period gives
    // near-instant boot in the GUI and faster test runs.
    bool boot_warp_ = false;

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

    void* get_video_port_ptr() override { return video_port_.get(); }

    // Boot warp — skip frame sync until PPU renders
    bool is_warping() const override { return boot_warp_; }

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

    // =========================================================================
    // FDS disk data — kept alive for the mapper's duration
    // =========================================================================
    std::vector<uint8_t> fds_disk_data_;

    /** Handle NSF player keyboard shortcuts (subtune selection).
     *  Returns true if the key was consumed (should not be forwarded). */
    bool handle_nsf_player_key(SDL_Keycode key);
};

// Convenience type aliases
using NESSystem     = NintendoSystem<NintendoVariant::NES>;
using FamicomSystem = NintendoSystem<NintendoVariant::FAMICOM>;

} // namespace nes_system
