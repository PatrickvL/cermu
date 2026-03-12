#pragma once

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/text_terminal.hpp"
#include "core/board.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/io/pia6820.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>
#include <memory>

// Apple 1 default bus state — derived from CPU.
// MOS6502 provides: RW, RDY, IRQ, NMI, RES.
#define APPLE1_BUS_DEFAULT_STATE \
    (MOS6502::default_bus_state())


// =============================================================================
// Apple 1 chip manifest — declarative memory layout
// =============================================================================
//
// Slot 0: RAM         — 64 KB (full address space, actual size configurable)
// Slot 1: Monitor ROM — 256 bytes at $FF00
// Slot 2: BASIC ROM   — 4 KB at $E000
// Slot 3: PIA         — MMIO-only, 4-byte window at $D010
//
// Write side: only RAM.  ROM reads overlay RAM; writes pass through to RAM
// (4K/8K modes unmap ROM read pages; 64K mode never maps ROM at all).
// Page $D0 uses an auto-created MaskedSubTable for PIA ($D010–$D013).
//
inline constexpr auto kApple1Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0xFF00,   256, 0, "Monitor"},
    Slot<ROMChip>{0xE000,  4096, 0, "BASIC"},
    Slot<pia6820_t> {0xD010,     0, 0xFFFC}                // MMIO-only, 4-byte window
);

// BusSpec auto-derived from the manifest
using Apple1BusSpec = ManifestBusSpec<kApple1Chips, 16, 8>;

namespace apple1_chips {
    inline constexpr size_t kRamSlot     = 0;
    inline constexpr size_t kMonitorSlot = 1;
    inline constexpr size_t kBasicSlot   = 2;
    inline constexpr size_t kPiaSlot     = 3;

    // Compile-time chip ids (from manifest prefix-sum)
    inline constexpr size_t kRamId       = kApple1Chips.base_id(kRamSlot, Apple1BusSpec::PageBits);      // 0
    inline constexpr size_t kMonitorId   = kApple1Chips.base_id(kMonitorSlot, Apple1BusSpec::PageBits);  // 256
    inline constexpr size_t kBasicId     = kApple1Chips.base_id(kBasicSlot, Apple1BusSpec::PageBits);    // 257
}


/**
 * Apple 1 System Implementation
 * Clean implementation using the new System architecture (VIC-20 pattern)
 *
 * The Apple 1 was Steve Wozniak's first computer design (1976)
 * Features:
 * - MOS 6502 CPU @ 1 MHz
 * - 4KB-8KB RAM (typically 8KB)
 * - 40x24 character display (via terminal/video card)
 * - Woz Monitor ROM (256 bytes at $FF00-$FFFF)
 * - Optional BASIC ROM
 */
class Apple1System : public System {
public:
    Apple1System();
    ~Apple1System() override;
    
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
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_text_input(const char* text) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;

private:
    // Chip instances
    MOS6502* cpu_ = nullptr;         // MOS6502 CPU instance @ 1 MHz
    pia6820_t pia_;                  // PIA 6820 for keyboard and display I/O
    TextTerminal* terminal_;         // Text terminal (40x24)
    
    // Memory chips — owned by bus_mem_, borrowed here for post-init access
    ROMChip* monitor_rom_ = nullptr;  // Woz Monitor ROM at $FF00-$FFFF (256 bytes)
    ROMChip* basic_rom_   = nullptr;  // Optional Apple 1 BASIC (4KB at various addresses)
    ROMChip* char_rom_    = nullptr;  // Signetics 2513 character ROM (512 bytes)
    
    // MemoryBus — declarative setup via chip manifest + Board::apply()
    using Bus = MemoryBus<Apple1BusSpec>;
    using PT  = PackingTraits<Apple1BusSpec>;
    using Mem = Board<Apple1BusSpec>;
    Bus bus_;
    Mem bus_mem_{kApple1Chips};

    // System state
    uint32_t cycles_per_frame_;
    uint32_t ram_size_;              // Configured RAM size (4KB or 8KB)
    bool has_basic_;                 // Whether BASIC ROM is loaded
    int cursor_col_;                 // Terminal cursor position
    int cursor_row_;
    bus_state_t pins_;               // CPU bus state (persisted across ticks)

    // Helper methods
    void tick_cpu();
    void configure_bus_memory_map();  // (Re)configure page tables for current ram_size_

    // Connector port setup (registers Apple 1 connector ports with base class)
    void setup_connector_ports();

    // ROM loading
    bool load_roms();

    // PIA callbacks
    static uint8_t pia_keyboard_read(void* user_data);  // Port A read (keyboard)
    static void pia_display_write(void* user_data, uint8_t data);  // Port B write (display)

    // Apple 1 keyboard helpers (PIA Port A specific)
    void set_keyboard_data(uint8_t key_code);   // Sets bit 7 strobe + ASCII in bits 0-6
    bool keyboard_ready() const;                 // Checks if bit 7 is set
    void clear_keyboard_strobe();                // Clears bit 7 (called after read)
    
    // Display helpers
    void display_char(uint8_t ch);
    void convert_2513_to_8x8_font(const uint8_t* char_rom, uint8_t* font_8x8);
    
    // Keystroke injection (paste / auto-type)
    void queue_text(const char* text);  // Queue text to be auto-typed
    void pump_paste_queue();            // Feed next char from queue → PIA
    std::string paste_queue_;           // Pending characters to inject
    uint32_t paste_delay_cycles_ = 0;   // Countdown between injected keystrokes
};