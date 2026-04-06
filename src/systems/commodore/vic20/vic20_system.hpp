#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/video/vic/mos6560.hpp"
#include "chip/video/vic/mos6561.hpp"
#include "chip/video/vic/vic_common.hpp"
#include "systems/commodore/vic20/vic20_bus.hpp"
#include "systems/commodore/vic20/vic20_chips.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "systems/commodore/vic20/vic20_io_decoder.hpp"

#include <cstdint>
#include <memory>
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
// Row: V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
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
// Value-typed chips (CPU, VIAs) + the default PAL VIC variant are declared
// here.  NTSC systems swap the VIC at runtime via Board::override_factory()
// before create_chips(); see VIC20System::initialize().
//
// MMIO chips have base=0, size=0, mask=0 — they are NOT directly bus-mapped.
// The I/O decoder routes bus accesses to VIC/VIA1/VIA2 via CS-tick dispatch.
// Color RAM ($9400) is a regular 1 KB buffer chip (4-bit masking TODO: MOS2114).
//


// ── Chip count, manifest, BusTraits ──────────────────────────────────────


inline constexpr auto kVIC20Manifest = make_manifest(
    // Chips
    Slot<MOS6502>{.base_addr = 0x0000, .label = "MOS 6502"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x0400, .label = "Base RAM 0"},
    Slot<RAMChip>{.base_addr = 0x0400, .size_bytes = 0x0C00, .label = "Expansion Block 0"},
    Slot<RAMChip>{.base_addr = 0x1000, .size_bytes = 0x1000, .label = "Base RAM 1"},
    Slot<RAMChip>{.base_addr = 0x2000, .size_bytes = 0x2000, .label = "Expansion Block 1"},
    Slot<RAMChip>{.base_addr = 0x4000, .size_bytes = 0x2000, .label = "Expansion Block 2"},
    Slot<RAMChip>{.base_addr = 0x6000, .size_bytes = 0x2000, .label = "Expansion Block 3"},
    Slot<ROMChip>{.base_addr = 0x8000, .size_bytes = 0x1000, .label = "CHARROM", .rom = {"characters.901460-03.bin|chargen.rom|901460-03.bin"}},
    Slot<mos6561_t>{.label = "MOS 6561 (PAL)"},
    Slot<mos6522_t>{.label = "VIA 1"},
    Slot<mos6522_t>{.label = "VIA 2"},
    Slot<vic20_io_decoder_t>{.base_addr = 0x9000, .label = "I/O Decoder"},
    Slot<RAMChip>{.base_addr = 0x9400, .size_bytes = 0x0400, .label = "Color RAM"},
    Slot<RAMChip>{.base_addr = 0xA000, .size_bytes = 0x2000, .label = "Cartridge Area"},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x2000, .label = "BASIC ROM", .rom = {"basic.901486-01.bin|basic.rom|901486-01.bin"}},
    Slot<ROMChip>{.base_addr = 0xE000, .size_bytes = 0x2000, .label = "KERNAL ROM", .rom = {"kernal.901486-07.bin|kernal.rom|901486-07.bin"}},
    // Ports
    Slot<PortControlDB9>{.name = "Control Port", .port_number = 1, .default_device = "joystick"},
    Slot<PortIecSerial>{.name = "IEC Serial Bus", .is_bus = true, .default_device = "1541"},
    Slot<PortCassette>{.name = "Cassette Port", .default_device = "datasette"},
    Slot<PortUserPort>{.name = "User Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_1702"},
    Slot<PortAudioMono>{.name = "Audio Out"},
    Slot<PortCustom>{.name = "Keyboard", .is_internal = true}
);

inline constexpr size_t kVIC20ChipCount = decltype(kVIC20Manifest)::chip_count;

// Slot indices for runtime override (constexpr lookup by factory pointer).
inline constexpr size_t kVIC20_VicSlot = kVIC20Manifest.find<mos6561_t>();

// Cart slot index (constexpr lookup by base address).
inline constexpr size_t kVIC20_CartSlot = [] {
    for (size_t i = 0; i < kVIC20ChipCount; ++i)
        if (kVIC20Manifest.chips[i].base_addr == 0xA000) return i;
    return kVIC20ChipCount;
}();


struct VIC20BusTraits {
    static constexpr const auto& kManifest = kVIC20Manifest;
    using Spec = ManifestBusSpec<kVIC20Manifest, 16, 10, 1, true>;  // 1 KB pages, CS-enabled
};

struct VIC20Board : Board<VIC20BusTraits::Spec> {
    using ComponentTuple = decltype(kVIC20Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    MOS6502&            cpu      = std::get<0>(components_);
    RAMChip&            ram0     = std::get<1>(components_);
    RAMChip&            blk0     = std::get<2>(components_);
    RAMChip&            ram1     = std::get<3>(components_);
    RAMChip&            blk1     = std::get<4>(components_);
    RAMChip&            blk2     = std::get<5>(components_);
    RAMChip&            blk3     = std::get<6>(components_);
    ROMChip&            charrom  = std::get<7>(components_);
    mos6561_t&          vic      = std::get<8>(components_);
    mos6522_t&          via1     = std::get<9>(components_);
    mos6522_t&          via2     = std::get<10>(components_);
    vic20_io_decoder_t& io_dec   = std::get<11>(components_);
    RAMChip&            colorram = std::get<12>(components_);
    RAMChip&            cart     = std::get<13>(components_);
    ROMChip&            basic    = std::get<14>(components_);
    ROMChip&            kernal   = std::get<15>(components_);

    // Port aliases
    PortControlDB9&     control_port    = std::get<16>(components_);
    PortIecSerial&      iec_serial_port = std::get<17>(components_);
    PortCassette&       cassette_port   = std::get<18>(components_);
    PortUserPort&       user_port       = std::get<19>(components_);
    PortExpansion&      expansion_port  = std::get<20>(components_);
    PortCompositeVideo& video_port      = std::get<21>(components_);
    PortAudioMono&      audio_port      = std::get<22>(components_);
    PortCustom&         keyboard_port   = std::get<23>(components_);

    VIC20Board() : Board(kVIC20Manifest) {}
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
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Audio output — drains VIC chip audio ring buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    vic20_bus_t bus_;
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    std::unique_ptr<AudioPort> audio_port_;            // Audio signal output
    
    // ── Memory bus (declarative manifest + page-pointer dispatch) ────────
    using Bus = MemoryBus<VIC20BusTraits::Spec>;
    using MainBoard = VIC20Board;
    Bus mem_bus_;
    MainBoard board_;

    // VIC chip — polymorphic pointer, concrete type depends on PAL/NTSC config.
    // PAL: points to value-typed board_.vic (mos6561_t).
    // NTSC: points to heap-owned mos6560_t created by Board::create_chips().
    vic_base_t* vic_ = nullptr;
    vic20_io_decoder_t* io_dec_ = nullptr;  // I/O decoder for $9000-$93FF
    
    // System state
    uint8_t expansion_flags_;        // Expansion RAM configuration
    bool cartridge_present_ = false; // Whether a cartridge ROM is loaded
    bool initialized_ = false;

public:
    // Load callback context — two contiguous regions addressable by the
    // commodore_load_context_t callbacks (write_byte / mem_read).
    struct MemLoadCtx {
        uint8_t* ram_base;   // board_.ram0.data() — contiguous for $0000-$7FFF
        uint8_t* cart_base;  // board_.cart.data() — for $A000-$BFFF
    };
private:
    MemLoadCtx load_mem_ctx_{};

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    bool is_system_initialized() const override { return initialized_; }
    int get_iec_port_index() const override { return 1; }       // IEC Serial Bus
    int get_cassette_port_index() const override { return 2; }  // Cassette Port

    // ---- CRT cartridge loading hooks ----
    bool on_file_parsed(format_load_result_t& result, const char* filepath) override;
    bool pre_apply_pending_load() override;
    
    // ROM loading
    bool load_roms();
    
    // Memory access for CPU
    void setup_expansion_map();                     // configure page pointers for expansion_flags_
    void setup_cartridge_pages(bool present);       // write-protect or restore $A000-$BFFF

    // Memory access callbacks for VIC chip
    static uint8_t vic_mem_read(void* user_data, uint16_t addr);
    static uint8_t vic_color_read(void* user_data, uint16_t addr);
    
    // VIA2 port read callbacks for keyboard matrix scanning
    static uint8_t vic20_via2_port_a_read(void* context, uint8_t port_a_output);
    static uint8_t vic20_via2_port_b_read(void* context, uint8_t port_b_output);
    
    // Connector port setup (registers VIC-20 connector ports with base class)
};
