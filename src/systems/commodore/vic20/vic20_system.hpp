#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
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

#define VIC20_FOR_EACH_SYSTEM_CHIP(V, ctx)                                                                                          \
    V(ctx, MOS6502,              cpu,       0x0000,     0,      0, 0, "MOS 6502",           nullptr)                                           \
    V(ctx, RAMChip,              ram0,      0x0000,  1024,      0, 0, "Base RAM 0",         nullptr)                                           \
    V(ctx, RAMChip,              blk0,      0x0400,  3072,      0, 0, "Expansion Block 0",  nullptr)                                           \
    V(ctx, RAMChip,              ram1,      0x1000,  4096,      0, 0, "Base RAM 1",         nullptr)                                           \
    V(ctx, RAMChip,              blk1,      0x2000,  8192,      0, 0, "Expansion Block 1",  nullptr)                                           \
    V(ctx, RAMChip,              blk2,      0x4000,  8192,      0, 0, "Expansion Block 2",  nullptr)                                           \
    V(ctx, RAMChip,              blk3,      0x6000,  8192,      0, 0, "Expansion Block 3",  nullptr)                                           \
    V(ctx, ROMChip,              charrom,   0x8000,  4096,      0, 0, "CHARROM",            "characters.901460-03.bin|chargen.rom|901460-03.bin") \
    V(ctx, mos6561_t,            vic,       0,          0,      0, 0, "MOS 6561 (PAL)",     nullptr)                                           \
    V(ctx, mos6522_t,            via1,      0,          0,      0, 0, "VIA 1",              nullptr)                                           \
    V(ctx, mos6522_t,            via2,      0,          0,      0, 0, "VIA 2",              nullptr)                                           \
    V(ctx, vic20_io_decoder_t,   io_dec,    0x9000,     0,      0, 0, "I/O Decoder",        nullptr)                                           \
    V(ctx, RAMChip,              colorram,  0x9400,  1024,      0, 0, "Color RAM",          nullptr)                                           \
    V(ctx, RAMChip,              cart,      0xA000,  8192,      0, 0, "Cartridge Area",     nullptr)                                           \
    V(ctx, ROMChip,              basic,     0xC000,  8192,      0, 0, "BASIC ROM",          "basic.901486-01.bin|basic.rom|901486-01.bin")       \
    V(ctx, ROMChip,              kernal,    0xE000,  8192,      0, 0, "KERNAL ROM",         "kernal.901486-07.bin|kernal.rom|901486-07.bin")

// ── Chip count, manifest, BusTraits ──────────────────────────────────────

static constexpr size_t kVIC20ChipCount = 0 VIC20_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kVIC20ChipCount> kVIC20Chips = ChipManifest<kVIC20ChipCount>{{
    VIC20_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

// Slot indices for runtime override (constexpr lookup by factory pointer).
inline constexpr size_t kVIC20_VicSlot = kVIC20Chips.find<mos6561_t>();

// Cart slot index (constexpr lookup by base address).
inline constexpr size_t kVIC20_CartSlot = [] {
    for (size_t i = 0; i < kVIC20ChipCount; ++i)
        if (kVIC20Chips.chips[i].base_addr == 0xA000) return i;
    return kVIC20ChipCount;
}();

struct VIC20BusTraits {
    static constexpr const auto& kManifest = kVIC20Chips;
    using Spec = ManifestBusSpec<kVIC20Chips, 16, 10, 1, true>;  // 1 KB pages, CS-enabled
};

// ============================================================================
// VIC-20 Chips — value-typed chipset owned by Board
// ============================================================================
// All 16 chips are value-typed.  The PAL VIC (mos6561_t) is the default.
// For NTSC, the VIC slot is unbound and factory-replaced with mos6560_t
// before create_chips() — the value-typed field sits unused (~200 bytes).
// Both variants are accessed uniformly via vic_base_t* vic_.
//
// RAM layout: ram0/blk0/ram1/blk1/blk2/blk3 are declared in address order
// and thus contiguous in Board's flat memory ($0000-$7FFF, 32 KB).  Code
// that reads this range (VIC video callbacks, load helpers) may index
// ram0.data() beyond the chip's own 1 KB — the contiguity guarantee
// makes this safe.

struct VIC20Chipset {
    VIC20_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
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
    
    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    
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
    using MainBoard = Board<VIC20BusTraits::Spec, VIC20Chipset>;
    Bus mem_bus_;
    MainBoard board_{kVIC20Chips};

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
    void inject_keys(const char* str) override;
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
    void setup_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;
};
