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

#include "../../chip/cpu/fam65xx/ricoh_2a03.h"
#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include "../../core/emulated_system.h"
#include "../../core/formats/nsf_format.h"

// NES default bus state — initial pin values before any chip asserts.
// RW=1 (read mode), active-low signals NMI/IRQ/RES start HIGH (inactive).
#define NES_BUS_DEFAULT_STATE \
    (BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_NMI_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_RES_BIT))

// Forward declarations
namespace nes_system {
    class PPU;
    class Cartridge;
    class Controller;
    class MemoryBus;
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

// ============================================================================
// PPU (Picture Processing Unit)
// ============================================================================

namespace nes_system {

class PPU : public ChipBase {
public:
    // PPU registers
    struct Registers {
        uint8_t ctrl;       // $2000 - PPUCTRL
        uint8_t mask;       // $2001 - PPUMASK
        uint8_t status;     // $2002 - PPUSTATUS
        uint8_t oam_addr;   // $2003 - OAMADDR
        uint8_t oam_data;   // $2004 - OAMDATA
        uint8_t scroll;     // $2005 - PPUSCROLL
        uint8_t addr;       // $2006 - PPUADDR
        uint8_t data;       // $2007 - PPUDATA
    } regs = {};
    
    // PPU memory
    std::vector<uint8_t> vram;      // 2KB VRAM
    std::vector<uint8_t> oam;       // 256 bytes OAM (Object Attribute Memory)
    std::vector<uint8_t> palette;   // 32 bytes palette RAM
    
    // Internal state
    struct InternalState {
        uint16_t v = 0;       // Current VRAM address (15 bits)
        uint16_t t = 0;       // Temporary VRAM address (15 bits)
        uint8_t x = 0;        // Fine X scroll (3 bits)
        bool w = false;       // First or second write toggle
        uint8_t fine_y = 0;   // Fine Y scroll
        
        // Background rendering
        uint16_t nt_addr = 0;     // Nametable address
        uint8_t nt_byte = 0;      // Nametable byte
        uint8_t at_byte = 0;      // Attribute table byte
        uint8_t bg_lo_byte = 0;   // Background pattern table low
        uint8_t bg_hi_byte = 0;   // Background pattern table high
        
        // Background shift registers
        uint16_t bg_shifter_pattern_lo = 0;
        uint16_t bg_shifter_pattern_hi = 0;
        uint16_t bg_shifter_attrib_lo = 0;
        uint16_t bg_shifter_attrib_hi = 0;
        
        // Sprite rendering
        struct Sprite {
            uint8_t y = 0;
            uint8_t tile_id = 0;
            uint8_t attributes = 0;
            uint8_t x = 0;
        };
        
        std::vector<Sprite> sprite_scanline;  // Sprites for current scanline
        uint8_t sprite_shifter_pattern_lo[8] = {};
        uint8_t sprite_shifter_pattern_hi[8] = {};
        
        bool sprite_zero_hit_possible = false;
        bool sprite_zero_being_rendered = false;
    } internal = {};
    
    // Timing
    int16_t scanline = -1;    // Current scanline (-1 to 260)
    uint16_t cycle = 0;       // Current cycle (0 to 340)
    uint64_t frame_count = 0; // Frame counter
    bool frame_complete = false;
    bool nmi = false;
    
    // Open bus data latch — PPU data bus retains last value
    uint8_t ppu_data_bus_ = 0;
    
    // Region
    bool is_pal = false;
    
    // Frame buffer (RGB888)
    std::vector<uint32_t> screen;
    
    // Pattern tables (for debugging)
    std::vector<uint32_t> pattern_table[2];
    
public:
    PPU(bool pal = false) : is_pal(pal) {
        info_ = ChipInfo{pal ? "RP2C07" : "RP2C02", "Ricoh"};
        // Initialize PPU memory
        vram.resize(2048, 0);
        oam.resize(256, 0);
        palette.resize(32, 0);
        screen.resize(nes_constants::SCREEN_WIDTH * nes_constants::SCREEN_HEIGHT, 0);
        pattern_table[0].resize(128 * 128, 0);
        pattern_table[1].resize(128 * 128, 0);
        internal.sprite_scanline.resize(8);
        
        reset();
    }
    
    void reset() {
        regs = {};
        internal = {};
        scanline = -1;
        cycle = 0;
        frame_count = 0;
        frame_complete = false;
        nmi = false;
        
        // Clear memory
        std::fill(vram.begin(), vram.end(), 0);
        std::fill(oam.begin(), oam.end(), 0);
        std::fill(palette.begin(), palette.end(), 0);
        std::fill(screen.begin(), screen.end(), 0);
    }
    
    // CPU bus interface — the PPU is a bus device; it samples A0-A2, R/W
    // and drives/samples D0-D7 via the shared bus_state_t.  Open-bus behavior
    // emerges naturally because the data lines retain their last value.
    bus_state_t cpu_bus_tick(bus_state_t bus);

    // Read-only peek for debug/GUI (no side-effects on PPU state)
    uint8_t cpu_peek(uint16_t addr) const;
    
    // PPU memory access
    uint8_t ppu_read(uint16_t addr, bool read_only = false);
    void ppu_write(uint16_t addr, uint8_t data);
    
    // Main PPU tick - called 3 times per CPU cycle
    void clock();
    
    // Connect cartridge for CHR data access
    void connect_cartridge(std::shared_ptr<Cartridge> cartridge);
    
    // Get frame buffer
    const std::vector<uint32_t>& get_screen() const { return screen; }
    
    // Get pattern tables (for debugging)
    const std::vector<uint32_t>& get_pattern_table(int i, uint8_t palette) const;
    
    // NMI status
    bool get_nmi() { bool temp = nmi; nmi = false; return temp; }
    
private:
    std::shared_ptr<Cartridge> cart;
    
    // Internal rendering functions
    void increment_scroll_x();
    void increment_scroll_y();
    void transfer_address_x();
    void transfer_address_y();
    void load_background_shifters();
    void update_shifters();
    
    // Color generation
    uint32_t get_color_from_palette_ram(uint8_t palette, uint8_t pixel);
    uint32_t nes2rgb(uint8_t nes_color);
    
    // Nametable mirroring helper
    uint16_t mirror_nametable_addr(uint16_t addr) const;
    
    // Sprite evaluation
    void evaluate_sprites();
    void load_sprite_shifters();

    // --- ChipBase interface ---
public:
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;
};

// ============================================================================
// CARTRIDGE SYSTEM
// ============================================================================

class Cartridge {
public:
    // iNES header structure
    struct Header {
        char name[4];           // "NES" + 0x1A
        uint8_t prg_rom_chunks; // Size of PRG ROM in 16KB chunks
        uint8_t chr_rom_chunks; // Size of CHR ROM in 8KB chunks
        uint8_t mapper1;        // Mapper, mirroring, battery, trainer
        uint8_t mapper2;        // Mapper, VS/Playchoice, NES 2.0
        uint8_t prg_ram_size;   // Size of PRG RAM in 8KB chunks
        uint8_t tv_system1;     // TV system (0=NTSC, 1=PAL)
        uint8_t tv_system2;     // TV system, PRG-RAM presence
        char unused[5];         // Unused padding
    };
    
    // Memory banks
    std::vector<uint8_t> prg_memory;  // Program ROM
    std::vector<uint8_t> chr_memory;  // Character ROM/RAM
    std::vector<uint8_t> prg_ram;     // Program RAM (battery backed)
    
    // Cartridge info
    uint8_t mapper_id = 0;
    uint8_t prg_banks = 0;
    uint8_t chr_banks = 0;
    bool battery_backed = false;
    
    /** Protected default constructor for subclasses (e.g. NsfCartridge). */
    Cartridge() = default;
    
public:
    explicit Cartridge(const std::string& filename);
    virtual ~Cartridge() = default;

    /** Parse iNES ROM from an already-loaded buffer.
     *  filepath_for_sram is stored for battery-backed SRAM persistence. */
    bool load_from_buffer(const uint8_t* data, size_t data_size,
                          const std::string& filepath_for_sram);
    
    // CPU bus interface — cartridge sits on the shared bus.
    // Returns the bus with data lines driven (for reads) or absorbed (for writes).
    // The bool return indicates whether the cartridge claimed the address.
    virtual bus_state_t cpu_bus_tick(bus_state_t bus, bool& handled);

    // PPU bus interface (CHR ROM/RAM) — separate internal bus, not CPU data bus.
    virtual bool ppu_read(uint16_t addr, uint8_t& data);
    virtual bool ppu_write(uint16_t addr, uint8_t data);
    
    // Nametable mirroring mode (mappers can change this dynamically)
    enum class Mirror {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LO,
        ONESCREEN_HI,
        FOUR_SCREEN
    };

    // Mirroring — the active mode may be changed by the mapper at runtime
    Mirror mirror_mode = Mirror::HORIZONTAL;
    bool get_mirror_horizontal() const { return mirror_mode == Mirror::HORIZONTAL; }
    bool get_mirror_vertical() const { return mirror_mode == Mirror::VERTICAL; }
    Mirror get_mirror_mode() const { return mirror_mode; }

    // Mapper IRQ (e.g. MMC3 scanline counter)
    bool irq_state() const;
    void irq_clear();

    // Scanline callback — the PPU calls this once per visible scanline
    void scanline();

    // Mapper interface
    virtual void reset();

    // Battery-backed SRAM persistence
    bool load_sram(const std::string& sav_path);
    bool save_sram(const std::string& sav_path) const;
    std::string sram_path_for_rom(const std::string& rom_path) const;
    const std::string& get_rom_filepath() const { return rom_filepath_; }

    // Debug read-only peek — no mapper side-effects, no bus modification
    uint8_t peek(uint16_t addr) const;
    
private:
    bool load_from_file(const std::string& filename);
    std::string rom_filepath_;  // stored for SRAM path derivation
    
    // Mapper implementations
    class Mapper {
    public:
        virtual ~Mapper() = default;
        virtual bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) = 0;
        virtual bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data = 0) = 0;
        virtual bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) = 0;
        virtual bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) = 0;
        virtual void reset() = 0;

        // Extended mapper interface (overridden by mappers that need it)
        virtual Mirror mirror() { return Mirror::HORIZONTAL; }  // default: no override
        virtual bool irq_state() { return false; }
        virtual void irq_clear() {}
        virtual void scanline() {}  // PPU notifies mapper on scanlines
    };
    
    std::unique_ptr<Mapper> mapper;
    
    // Specific mapper implementations
    class Mapper000; // NROM
    class Mapper001; // MMC1
    class Mapper002; // UxROM
    class Mapper003; // CNROM
    class Mapper004; // MMC3
};

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
// MEMORY BUS
// ============================================================================

class MemoryBus {
public:
    // CPU RAM (2KB) — public for NSF player stub injection and direct access
    std::vector<uint8_t> cpu_ram;
    
public:
    // Connected devices
    std::shared_ptr<PPU> ppu;
    std::shared_ptr<Cartridge> cartridge;
    std::array<Controller, 2> controllers;
    
    // DMA
    uint8_t dma_page = 0x00;
    uint8_t dma_addr = 0x00;
    uint8_t dma_data = 0x00;
    bool dma_transfer = false;
    bool dma_dummy = true;
    
    // System clock counter
    uint32_t system_clock_counter = 0;
    
public:
    MemoryBus() {
        cpu_ram.resize(2048, 0);
    }
    
    void connect_ppu(std::shared_ptr<PPU> p) { ppu = p; }
    void connect_cartridge(std::shared_ptr<Cartridge> c) { cartridge = c; }
    
    // CPU memory interface — unified bus_state_t
    bus_state_t mem_tick(bus_state_t bus);
    
    // System reset
    void reset();
    
    // Clock the bus and connected devices
    void clock();
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
    std::shared_ptr<MemoryBus> bus_;
    
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

    // Auto-detect PAL/NTSC from iNES header
    SystemConfiguration detect_optimal_configuration(
        const char* filepath, const uint8_t* data, size_t size) override;

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
