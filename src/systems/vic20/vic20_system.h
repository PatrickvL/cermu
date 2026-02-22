#pragma once

#include "../commodore/commodore_system.h"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h"
#include "../../chip/io/mos6522.h"
#include "../../chip/video/vic/mos6560.h"
#include "../../chip/video/vic/mos6561.h"
#include "../../chip/video/vic/vic_common.h"
#include "vic20_bus.h"
#include "vic20_config.h"
#include "vic20_memory.h"
#include "vic20_chips.h"
#include <cstdint>
#include <string>

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
 * 
 * Memory Map (stock unexpanded VIC-20):
 * $0000–$03FF  1 KB   RAM 0 (zero page, stack, system variables)
 * $0400–$0FFF  3 KB   Unmapped (expansion RAM block 0)
 * $1000–$1FFF  4 KB   RAM 1 (main user BASIC RAM)
 * $2000–$3FFF  8 KB   Unmapped (expansion RAM block 2)
 * $4000–$5FFF  8 KB   Unmapped (expansion RAM block 3)
 * $6000–$7FFF  8 KB   Unmapped (expansion RAM block 5)
 * $8000–$8FFF  4 KB   Character ROM
 * $9000–$900F  16 B   VIC chip registers (mirrored in $9000-$93FF)
 * $9010–$901F  16 B   VIA #1 (mirrored in $9000-$93FF)
 * $9020–$902F  16 B   VIA #2 (mirrored in $9000-$93FF)
 * $9030–$93FF  ~1 KB  I/O mirrors
 * $9400–$97FF  1 KB   Color RAM (4-bit wide)
 * $9800–$9BFF  1 KB   Unmapped (I/O expansion block 2)
 * $9C00–$9FFF  1 KB   Unmapped (I/O expansion block 3)
 * $A000–$BFFF  8 KB   Unmapped (expansion ROM / cartridge)
 * $C000–$DFFF  8 KB   BASIC ROM
 * $E000–$FFFF  8 KB   KERNAL ROM
 */
class VIC20System : public CommodoreSystem {
public:
    VIC20System();
    ~VIC20System() override;
    
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

    // Auto-detect memory expansion and region from file contents
    SystemConfiguration detect_optimal_configuration(
        const char* filepath, const uint8_t* data, size_t size) override;

    // Audio output — drains VIC chip audio ring buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

private:
    vic20_bus_t bus_;
    
    // New memory banking system
    vic20_memory_t* memory_;         // Unified memory banking system
    
    // Chip instances (properly typed)
    mos6502_t* cpu_;                 // MOS6502 CPU instance
    vic_base_t* vic_;                // VIC chip: MOS6561 (PAL) or MOS6560 (NTSC)
    mos6522_t* via1_;                // MOS6522 VIA 1 - keyboard, joystick
    mos6522_t* via2_;                // MOS6522 VIA 2 - user port, serial
    
    // System state
    uint8_t expansion_flags_;        // Expansion RAM configuration

    // Deferred autostart — file loading and keyboard buffer injection
    // must wait until the KERNAL boot sequence completes, because boot
    // clears zero-page ($2B-$32 BASIC pointers, $C6 keyboard count)
    // and initializes RAM.  We store the filepath and load directly
    // into system memory from run_frame() after boot reaches READY.
    std::string pending_filepath_;       // File to load after boot (empty = none)
    int  autostart_delay_frames_;        // Frames remaining before deferred load
    
    // Internal: load file directly into system memory (called after boot)
    bool load_file_into_memory(const char* filepath);
    
    // ROM loading
    bool load_roms();
    
    // Memory access callbacks for CPU
    bus_state_t mem_tick(bus_state_t s);

    // Memory access callbacks for VIC chip
    static uint8_t vic_mem_read(void* user_data, uint16_t addr);
    static uint8_t vic_color_read(void* user_data, uint16_t addr);
    
    // VIA2 port read callbacks for keyboard matrix scanning
    static uint8_t vic20_via2_port_a_read(void* context, uint8_t port_a_output);
    static uint8_t vic20_via2_port_b_read(void* context, uint8_t port_b_output);
    
    // Connector port setup (registers VIC-20 connector ports with base class)
    void setup_connector_ports();

    /// Register all VIC-20 chips into registered_chips_ for the Hardware menu.
    void register_vic20_chips();
    
    // Legacy integration methods (deprecated, kept for compatibility)
    void memory_init(const rom_config_t* rom_config);
    bool reload_roms(const rom_config_t* rom_config);
};
