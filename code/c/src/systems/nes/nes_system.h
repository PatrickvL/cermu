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

#include <stdint.h>
#include <stdbool.h>
#include <memory>
#include <vector>
#include <string>

#include "../../chip/cpu/fam65xx/nes6502.h"
#include "../../core/chip.h"
#include "../../core/system_lines.h"

#ifdef __cplusplus

// Forward declarations
namespace nes_system {
    class PPU;
    class Cartridge;
    class Controller;
    class MemoryBus;
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

class PPU {
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
    
    // Region
    bool is_pal = false;
    
    // Frame buffer (RGB888)
    std::vector<uint32_t> screen;
    
    // Pattern tables (for debugging)
    std::vector<uint32_t> pattern_table[2];
    
public:
    PPU(bool pal = false) : is_pal(pal) {
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
    
    // Register access
    uint8_t cpu_read(uint16_t addr, bool read_only = false);
    void cpu_write(uint16_t addr, uint8_t data);
    
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
    
    // Sprite evaluation
    void evaluate_sprites();
    void load_sprite_shifters();
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
    bool mirror_horizontal = false;
    bool mirror_vertical = false;
    bool battery_backed = false;
    
public:
    Cartridge(const std::string& filename);
    ~Cartridge() = default;
    
    // CPU memory access
    bool cpu_read(uint16_t addr, uint8_t& data);
    bool cpu_write(uint16_t addr, uint8_t data);
    
    // PPU memory access
    bool ppu_read(uint16_t addr, uint8_t& data);
    bool ppu_write(uint16_t addr, uint8_t data);
    
    // Mirroring
    bool get_mirror_horizontal() const { return mirror_horizontal; }
    bool get_mirror_vertical() const { return mirror_vertical; }
    
    // Mapper interface
    void reset();
    
private:
    bool load_from_file(const std::string& filename);
    
    // Mapper implementations
    class Mapper {
    public:
        virtual ~Mapper() = default;
        virtual bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) = 0;
        virtual bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data = 0) = 0;
        virtual bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) = 0;
        virtual bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) = 0;
        virtual void reset() = 0;
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
private:
    std::vector<uint8_t> cpu_ram;  // 2KB CPU RAM
    
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
    
    // CPU memory interface
    uint8_t cpu_read(uint16_t addr, bool read_only = false);
    void cpu_write(uint16_t addr, uint8_t data);
    
    // System reset
    void reset();
    
    // Clock the bus and connected devices
    void clock();
};

// ============================================================================
// MAIN NES SYSTEM
// ============================================================================

class NESSystem {
private:
    // Core components
    nes6502_t* cpu = nullptr;
    std::shared_ptr<PPU> ppu;
    std::shared_ptr<Cartridge> cartridge;
    std::shared_ptr<MemoryBus> bus;
    
    // System state
    bool is_pal = false;
    bool system_ready = false;
    uint64_t total_cycles = 0;
    
    // Audio buffer
    std::vector<float> audio_buffer;
    uint32_t audio_sample_rate = 44100;
    uint32_t audio_samples_per_frame;
    uint32_t audio_sample_counter = 0;
    
    // Timing
    double residual_time = 0.0;
    
public:
    NESSystem(bool pal = false);
    ~NESSystem();
    
    // Cartridge loading
    bool load_cartridge(const std::string& filename);
    void eject_cartridge();
    
    // System control
    void reset();
    void power_cycle();
    
    // Main emulation step
    void clock();
    void run_frame();
    
    // Input
    void set_controller_state(int controller, uint8_t state);
    void press_button(int controller, Controller::Button button);
    void release_button(int controller, Controller::Button button);
    
    // Output
    const std::vector<uint32_t>& get_screen() const;
    const std::vector<float>& get_audio_buffer() const { return audio_buffer; }
    void clear_audio_buffer() { audio_buffer.clear(); audio_sample_counter = 0; }
    
    // Debug/Development
    const std::vector<uint32_t>& get_pattern_table(int table, uint8_t palette) const;
    void set_audio_sample_rate(uint32_t rate);
    
    // Save states
    bool save_state(const std::string& filename) const;
    bool load_state(const std::string& filename);
    
    // System info
    bool is_cartridge_loaded() const { return cartridge != nullptr; }
    uint64_t get_total_cycles() const { return total_cycles; }
    bool is_system_ready() const { return system_ready; }
    
private:
    void setup_audio_timing();
    bus_state_t create_bus_state(uint16_t addr, uint8_t data, bool rw);
};

} // namespace nes_system

#endif // __cplusplus

// ============================================================================
// C INTERFACE
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef struct nes_system_t nes_system_t;

// System creation/destruction
nes_system_t* nes_system_create(bool is_pal);
void nes_system_destroy(nes_system_t* system);

// Cartridge management
bool nes_system_load_cartridge(nes_system_t* system, const char* filename);
void nes_system_eject_cartridge(nes_system_t* system);

// System control
void nes_system_reset(nes_system_t* system);
void nes_system_power_cycle(nes_system_t* system);
void nes_system_clock(nes_system_t* system);
void nes_system_run_frame(nes_system_t* system);

// Input
void nes_system_set_controller_state(nes_system_t* system, int controller, uint8_t state);
void nes_system_press_button(nes_system_t* system, int controller, uint8_t button);
void nes_system_release_button(nes_system_t* system, int controller, uint8_t button);

// Output
const uint32_t* nes_system_get_screen(nes_system_t* system);
const float* nes_system_get_audio_buffer(nes_system_t* system, uint32_t* sample_count);
void nes_system_clear_audio_buffer(nes_system_t* system);

// Configuration
void nes_system_set_audio_sample_rate(nes_system_t* system, uint32_t rate);

// System info
bool nes_system_is_cartridge_loaded(nes_system_t* system);
uint64_t nes_system_get_total_cycles(nes_system_t* system);
bool nes_system_is_ready(nes_system_t* system);

#ifdef __cplusplus
}
#endif