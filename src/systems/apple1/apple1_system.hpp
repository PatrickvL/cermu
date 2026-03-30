#pragma once

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/text_terminal.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/system_chip_visitors.hpp"
#include "systems/apple1/apple1_constants.hpp"
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
// Apple 1 chip declaration — single source of truth
// =============================================================================
//
// Every chip is declared once.  Visitors generate manifest, chipset fields,
// binding, and component registration from this list.
//
// Row: X(ctx, type, chip, base, mask, overlay, label, info_label, rom_files)
//
//   Slot 0: RAM         — 64 KB (full address space, actual size configurable)
//   Slot 1: Monitor ROM — 256 bytes at $FF00
//   Slot 2: BASIC ROM   — 4 KB at $E000 (optional — ?prefix)
//   Slot 3: PIA         — MMIO-only, 4-byte window at $D010
//   Slot 4: CPU         — not bus-mapped
//
// Write side: only RAM.  ROM reads overlay RAM; writes pass through to RAM
// (4K/8K modes unmap ROM read pages; 64K mode never maps ROM at all).
// Page $D0 uses an auto-created MaskedSubTable for PIA ($D010–$D013).
//

#define APPLE1_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                                \
    X(ctx, RAMChip,    ram,     0x0000, 0x10000,      0, 0, "RAM",     "RAM",          nullptr)            \
    X(ctx, ROMChip,    monitor, 0xFF00,  0x0100,      0, 0, "Monitor", "Monitor ROM",  "apple1.rom|monitor.rom|wozmon.rom") \
    X(ctx, ROMChip,    basic,   0xE000,  0x1000,      0, 0, "BASIC",   "BASIC ROM",   "?apple1basic.rom|basic.rom") \
    X(ctx, pia6820_t,  pia,     0xD010,       0, 0xFFFC, 0, "PIA",     "PIA 6820",    nullptr)             \
    X(ctx, MOS6502,    cpu,     0x0000,       0,      0, 0, "MOS 6502","MOS 6502",    nullptr)

static constexpr size_t kApple1ChipCount = 0 APPLE1_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kApple1ChipCount> kApple1Chips = ChipManifest<kApple1ChipCount>{{
    APPLE1_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

// BusSpec auto-derived from the manifest
using Apple1BusSpec = ManifestBusSpec<kApple1Chips, 16, 8>;

// ============================================================================
// Apple 1 Chips — value-typed chips owned by Board (auto-generated)
// ============================================================================

struct Apple1Chips {
    APPLE1_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

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
    
    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_text_input(const char* text) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

private:
    // Chip instances
    TextTerminal* terminal_;         // Text terminal (40x24)
    
    // Memory chips — Signetics 2513 not on the bus (terminal renderer only)
    ROMChip* char_rom_    = nullptr;  // Signetics 2513 character ROM (512 bytes)
    
    // MemoryBus — declarative setup via chip manifest + Board::apply()
    using Bus = MemoryBus<Apple1BusSpec>;
    using PT  = PackingTraits<Apple1BusSpec>;
    using MainBoard = Board<Apple1BusSpec, Apple1Chips>;
    Bus bus_;
    MainBoard board_{kApple1Chips};

    // Display — pixel buffer + video stream
    uint8_t pixel_buffer_[apple1_constants::DISPLAY_WIDTH * apple1_constants::DISPLAY_HEIGHT] = {};
    std::unique_ptr<CompositeVideoPort> video_port_;

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
    void setup_ports();

    // Default peripherals
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

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