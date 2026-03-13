#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/board.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/video/vic/mos6560.hpp"
#include "chip/video/vic/mos6561.hpp"
#include "chip/video/vic/vic_common.hpp"
#include "systems/commodore/vic20/vic20_bus.hpp"
#include "systems/commodore/vic20/vic20_config.hpp"
#include "systems/commodore/vic20/vic20_chips.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"

#include <cstdint>
#include <string>

/**
 * VIC-20 System Implementation
 * Clean implementation using the new System architecture (CHIP-8 pattern)
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

// ============================================================================
// VIC-20 chip manifest — declarative system chip list
// ============================================================================
//
// Memory map (CPU view):
//   $0000-$03FF  RAM (always present, base RAM 0)
//   $0400-$0FFF  Expansion block 0 (3KB) or unmapped
//   $1000-$1FFF  RAM (always present, base RAM 1)
//   $2000-$3FFF  Expansion block 2 (8KB) or unmapped
//   $4000-$5FFF  Expansion block 3 (8KB) or unmapped
//   $6000-$7FFF  Expansion block 5 (8KB) or unmapped
//   $8000-$8FFF  Character ROM (4KB, read-only)
//   $9000-$9FFF  I/O (VIC, VIAs, Color RAM — handled manually in io_tick())
//   $A000-$BFFF  Cartridge ROM or unmapped (loaded into RAM buffer, write-protected)
//   $C000-$DFFF  BASIC ROM (8KB, read-only)
//   $E000-$FFFF  KERNAL ROM (8KB, read-only)
//
// Expansion mapping is controlled by page-pointer reconfiguration.
// Cartridge ROM is loaded into the RAM buffer and write-protected via page pointers.
//
// Non-bus chips (CPU, VIC, VIAs) are declared with size_bytes=0 — they are
// factory-created by Board::create_chips() but not mapped into the bus.
// Conditional chips use condition tags evaluated at create_chips() time.
//

// Condition tags for VIC-20 configuration-dependent chips.
namespace vic20_cond {
    inline constexpr uint16_t kPAL  = 1;   // PAL region (MOS 6561)
    inline constexpr uint16_t kNTSC = 2;   // NTSC region (MOS 6560)
}

inline constexpr auto kVIC20Chips = make_chip_manifest(
    // ── Bus-mapped memory chips ──────────────────────────────────────────
    Slot<RAMChip>   {0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>   {0x8000,  4096, 0, "CHARROM"},
    Slot<ROMChip>   {0xC000,  8192, 0, "BASIC ROM"},
    Slot<ROMChip>   {0xE000,  8192, 0, "KERNAL ROM"},
    // ── Non-bus chips (factory-created, not mapped) ──────────────────────
    Slot<MOS6502>   {0, 0, 0, "MOS 6502"},
    Slot<mos6561_t> {0, 0, 0, "MOS 6561 (PAL)",  vic20_cond::kPAL},
    Slot<mos6560_t> {0, 0, 0, "MOS 6560 (NTSC)", vic20_cond::kNTSC},
    Slot<mos6522_t> {0, 0, 0, "VIA 1"},
    Slot<mos6522_t> {0, 0, 0, "VIA 2"}
);

namespace vic20_slot {
    inline constexpr size_t kRam       = 0;
    inline constexpr size_t kCharRom   = 1;
    inline constexpr size_t kBasicRom  = 2;
    inline constexpr size_t kKernalRom = 3;
    inline constexpr size_t kVicPal    = 5;
    inline constexpr size_t kVicNtsc   = 6;
    inline constexpr size_t kVia1      = 7;
    inline constexpr size_t kVia2      = 8;
}

struct VIC20BusTraits {
    static constexpr const auto& kManifest = kVIC20Chips;
    using Spec = ManifestBusSpec<kVIC20Chips, 16, 8>;
};

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
    
    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
    
    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Audio output — drains VIC chip audio ring buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

private:
    vic20_bus_t bus_;
    
    // ── Memory bus (declarative manifest + page-pointer dispatch) ────────
    using Bus = MemoryBus<VIC20BusTraits::Spec>;
    using MainBoard = Board<VIC20BusTraits::Spec>;
    Bus mem_bus_;
    MainBoard board_{kVIC20Chips};

    // Convenience chip pointers (all owned by board_, accessed via chip_as/first_chip)
    RAMChip* ram_         = nullptr;  // 64 KB flat mem
    ROMChip* charrom_     = nullptr;  // Character ROM $8000-$8FFF (4 KB)
    ROMChip* basic_rom_   = nullptr;  // BASIC ROM $C000-$DFFF (8 KB)
    ROMChip* kernal_rom_  = nullptr;  // KERNAL ROM $E000-$FFFF (8 KB)
    MOS6502* cpu_         = nullptr;  // MOS 6502 CPU
    vic_base_t* vic_      = nullptr;  // VIC chip: MOS 6561 (PAL) or MOS 6560 (NTSC)
    mos6522_t* via1_      = nullptr;  // MOS 6522 VIA 1 - keyboard, joystick
    mos6522_t* via2_      = nullptr;  // MOS 6522 VIA 2 - user port, serial
    
    // System state
    uint8_t expansion_flags_;        // Expansion RAM configuration
    bool cartridge_present_ = false; // Whether a cartridge ROM is loaded
    bool initialized_ = false;

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    void inject_keys(const char* str) override;
    bool is_system_initialized() const override { return initialized_ && cpu_ != nullptr; }
    int get_iec_port_index() const override { return 1; }       // IEC Serial Bus
    int get_cassette_port_index() const override { return 2; }  // Cassette Port

    // ---- CRT cartridge loading hooks ----
    bool on_file_parsed(format_load_result_t& result, const char* filepath) override;
    bool pre_apply_pending_load() override;
    
    // ROM loading
    bool load_roms();
    
    // Memory access for CPU
    bus_state_t mem_tick(bus_state_t s);
    bus_state_t io_tick(bus_state_t s);             // $9000-$9FFF I/O dispatch
    void setup_expansion_map();                     // configure page pointers for expansion_flags_
    void setup_cartridge_pages(bool present);       // write-protect or restore $A000-$BFFF

    // Memory access callbacks for VIC chip
    static uint8_t vic_mem_read(void* user_data, uint16_t addr);
    static uint8_t vic_color_read(void* user_data, uint16_t addr);
    
    // VIA2 port read callbacks for keyboard matrix scanning
    static uint8_t vic20_via2_port_a_read(void* context, uint8_t port_a_output);
    static uint8_t vic20_via2_port_b_read(void* context, uint8_t port_b_output);
    
    // Connector port setup (registers VIC-20 connector ports with base class)
    void setup_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    // Legacy integration methods (deprecated, kept for compatibility)
    void memory_init(const rom_config_t* rom_config);
    bool reload_roms(const rom_config_t* rom_config);
};
