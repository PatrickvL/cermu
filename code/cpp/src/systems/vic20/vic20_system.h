#pragma once

#include "../../core/emulated_system.h"
#include "../../core/system.h"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h"
#include "../../chip/io/mos6522.h"
#include "../../chip/video/vic/mos6560.h"
#include "../../chip/video/vic/mos6561.h"
#include "vic20_bus.h"
#include "vic20_config.h"
#include <cstdint>
#include <memory>

/**
 * VIC-20 System Implementation
 * Clean implementation using the new EmulatedSystem architecture (CHIP-8 pattern)
 *
 * The VIC-20 was Commodore's first successful mass-market home computer (1980-1985)
 * Features:
 * - MOS 6502 CPU @ ~1 MHz
 * - 5KB RAM (expandable to 32KB+)
 * - 22x23 character display with 16 colors
 * - VIC (6560/6561) video chip
 * - 3 channel + noise sound
 */
class VIC20System : public EmulatedSystem {
public:
    VIC20System();
    ~VIC20System() override;
    
    // System identification
    const SystemDescriptor& get_descriptor() const override;
    
    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
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
    void handle_keyboard_event(int key, bool pressed) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    
    // State
    uint32_t get_target_fps() const override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;

private:
    // Legacy system integration
    system_8bit_t system_;           // Legacy system wrapper
    vic20_bus_t bus_;
    
    // Chip instances (properly typed)
    mos6502_t* cpu_;                 // MOS6502 CPU instance
    ram_t* ram_;                     // RAM memory $0000-$FFFF (35KB)
    mos6560_t* vic_;                 // VIC 6560 (PAL) or 6561 (NTSC) video & sound chip ($9000-$9FFF, 4KB)
    mos6522_t* via1_;                // MOS6522 VIA 1 ($9120-$912F, 16 bytes) - keyboard, joystick
    mos6522_t* via2_;                // MOS6522 VIA 2 ($9140-$914F, 16 bytes) - user port, serial (optional)
    mos2114_t* colorram_;            // Color RAM (1KB at $9400-$97FF)
    rom_t* basic_;                   // BASIC ROM $A000-$BFFF (8KB)
    rom_t* charrom_;                 // Character ROM $D000-$DFFF (4KB)
    rom_t* kernal_;                  // Kernal ROM $E000-$FFFF (8KB)

    // VIC-20 Memory (simplified arrays for now)
    uint8_t ram_simple_[5120];       // 5KB base RAM
    uint8_t expansion_ram_[32768];   // Optional expansion RAM
    uint8_t color_ram_simple_[1024]; // Color RAM
    
    // ROMs (buffers for ROM loading)
    uint8_t basic_rom_[8192];        // BASIC ROM
    uint8_t char_rom_[4096];         // Character ROM
    uint8_t kernal_rom_[8192];       // KERNAL ROM
    
    // System state
    uint32_t cycles_per_frame_;
    uint32_t expansion_size_;        // Size of expansion RAM
    
    // Helper methods
    void tick_cpu();
    void tick_vic();
    void cpu_cycle();
    void non_cpu_cycle();
    
    // Memory access callbacks for CPU
    static uint8_t cpu_read(void* user_data, uint32_t addr, uint8_t bus_state);
    static void cpu_write(void* user_data, uint32_t addr, uint8_t data);
    
    // Legacy integration methods
    void memory_init(const rom_config_t* rom_config);
    bool reload_roms(const rom_config_t* rom_config);
};