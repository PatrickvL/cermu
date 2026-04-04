#pragma once

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/text_terminal.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
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
// Apple 1 component manifest — single source of truth
// =============================================================================
//
// Chips:
//   Slot 0: CPU         — not bus-mapped
//   Slot 1: RAM         — 64 KB (full address space, actual size configurable)
//   Slot 2: PIA         — MMIO-only, 4-byte window at $D010
//   Slot 3: BASIC ROM   — 4 KB at $E000 (optional)
//   Slot 4: Monitor ROM — 256 bytes at $FF00
//
// Ports:
//   Slot 5: Expansion   — 44-pin edge connector (full 6502 bus)
//   Slot 6: Cassette    — ACI cassette interface
//   Slot 7: Video Out   — Composite video output
//
// Write side: only RAM.  ROM reads overlay RAM; writes pass through to RAM
// (4K/8K modes unmap ROM read pages; 64K mode never maps ROM at all).
// Page $D0 uses an auto-created MaskedSubTable for PIA ($D010–$D013).
//

inline constexpr auto kApple1Manifest = make_manifest(
    // Chips
    Slot<MOS6502>  {.base_addr = 0x0000,                                   .label = "MOS 6502"},
    Slot<RAMChip>  {.base_addr = 0x0000, .size_bytes = 0x10000,            .label = "RAM"},
    Slot<pia6820_t>{.base_addr = 0xD010,              .addr_mask = 0xFFFC, .label = "PIA"},
    Slot<ROMChip>  {.base_addr = 0xE000, .size_bytes = 0x1000,             .label = "BASIC",
                    .rom = {"apple1basic.rom|basic.rom", true}},
    Slot<ROMChip>  {.base_addr = 0xFF00, .size_bytes = 0x0100,             .label = "Monitor",
                    .rom = {"apple1.rom|monitor.rom|wozmon.rom"}},
    // Ports
    Slot<PortExpansion>      {.name = "Expansion Connector"},
    Slot<PortCassette>       {.name = "Cassette Interface (ACI)"},
    Slot<PortCompositeVideo> {.name = "Video Out", .default_device = "crt_green"}
);

// BusSpec auto-derived from the manifest.  EnableCs=true enables CS-tick:
// resolve() embeds the decoded chip ID into bus_state_t CS field,
// and each chip self-selects via its bus_chip_id_.
using Apple1BusSpec = ManifestBusSpec<kApple1Manifest, 16, 8, 1, true>;

// ============================================================================
// Apple 1 Board — typed component tuple with named references
// ============================================================================
//
// Chips and ports live as value members in a std::tuple; reference aliases
// preserve board_.field syntax for chip access and provide typed port access.

struct Apple1Board : Board<Apple1BusSpec> {
    using ComponentTuple = decltype(kApple1Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases (indices 0–4)
    MOS6502&   cpu     = std::get<0>(components_);
    RAMChip&   ram     = std::get<1>(components_);
    pia6820_t& pia     = std::get<2>(components_);
    ROMChip&   basic   = std::get<3>(components_);
    ROMChip&   monitor = std::get<4>(components_);

    // Port aliases (indices 5–7)
    PortExpansion&      expansion_port = std::get<5>(components_);
    PortCassette&       cassette_port  = std::get<6>(components_);
    PortCompositeVideo& video_port_hw  = std::get<7>(components_);

    Apple1Board() : Board(kApple1Manifest) {}
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
    Bus bus_;
    Apple1Board board_;

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