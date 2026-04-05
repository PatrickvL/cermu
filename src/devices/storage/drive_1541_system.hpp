#pragma once
// drive_1541_system.hpp — Cycle-accurate Commodore 1541 disk drive system
//
// Models the 1541 as a complete independent computer:
//   - MOS 6502 CPU @ 1 MHz
//   - VIA#1 (6522) at $1800 — IEC serial bus interface
//   - VIA#2 (6522) at $1C00 — Drive mechanics (motor, head, R/W)
//   - 2 KB RAM ($0000-$07FF, mirrored through $0FFF)
//   - 16 KB ROM ($C000-$FFFF)
//
// Template parameter: DriveTraits NTTP reference (CBM1541Traits, CBM1541CTraits,
// CBM1541IITraits).  All three share identical logic; only ROM content and
// model metadata differ.  `if constexpr` on Traits fields gates any future
// variant-specific behavior (e.g., 1541-II edge-connector detect).
//
// This is NOT a System subclass — it's an autonomous subsystem driven by
// the host computer's tick loop.  The host calls advance() each cycle to
// keep the drive in lockstep.
//
// Architecture follows the cermu Board + Manifest + MMIO chip-select model:
//   - A typed manifest declares all chips (CPU, RAM, ROM, 2× VIA)
//   - Board<Spec> owns flat memory, wires page tables + MMIO handlers
//   - MemoryBus resolves addresses, embeds chip ID in bus_state_t CS field
//   - Each VIA self-selects via bus_chip_id_ in tick_mmio()
//
// IEC communication flows through the shared IECBus struct.  The drive reads
// ATN/CLK/DATA from the bus, drives CLK/DATA via its VIA#1 port B output.
//
// Disk image: Currently supports D64.  GCR encoding is modeled at the
// track/sector level (not raw bitstream) for efficiency, with hooks for
// future G64 raw-GCR support.
//
// Warp mode: The host system monitors VIA#2 port B bit 2 (motor on) and
// signals its WarpController accordingly.

#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/memory/memory_chip.hpp"
#include "core/board.hpp"
#include "core/iec_bus.hpp"
#include "core/system_lines.hpp"
#include "core/typed_manifest.hpp"
#include "devices/storage/drive_traits.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace fam65xx;

// ============================================================================
// GCR disk state
// ============================================================================

/// Represents the current state of the read/write head and spindle motor.
struct DriveHeadState {
    uint8_t  half_track    = 1 * 2;  // Current head position (half-tracks, 1-based: track 1 = 2)
    bool     motor_on      = false;  // Spindle motor running
    bool     led_on        = false;  // Drive activity LED
    uint8_t  speed_zone    = 3;      // Current GCR speed zone (0-3)
    bool     write_protect = false;  // Write-protect sensor

    // Bitstream position within current track
    uint32_t bit_position  = 0;      // Current bit offset in track data
    uint32_t bits_per_track = 0;     // Total bits on current track

    // Step motor state (4-phase stepper)
    uint8_t  stepper_phase = 0;      // Current stepper motor phase (0-3)

    uint8_t track() const { return half_track / 2; }
};

// ============================================================================
// D64 disk image container
// ============================================================================

/// Low-level D64 sector access.  Separate from the drive system so it can
/// be reused by the trap-based Drive1541Device and future G64/D71 support.
struct D64Image {
    std::vector<uint8_t> data;
    bool     loaded    = false;
    bool     read_only = false;
    uint8_t  num_tracks = 35;
    std::string filepath;

    bool load(const char* path);
    void eject();

    bool read_sector(uint8_t track, uint8_t sector, uint8_t* out) const;
    bool write_sector(uint8_t track, uint8_t sector, const uint8_t* in);

    static uint32_t track_sector_offset(uint8_t track, uint8_t sector);
    static uint8_t  sectors_per_track(uint8_t track);

    // D64 standard sizes
    static constexpr uint32_t SIZE_35_TRACKS     = 174848;
    static constexpr uint32_t SIZE_35_TRACKS_ERR = 175531;
    static constexpr uint32_t SIZE_40_TRACKS     = 196608;
    static constexpr uint32_t SIZE_40_TRACKS_ERR = 197376;
};

// ============================================================================
// 1541 Board Manifest
// ============================================================================
//
// 1541 memory map (active-low partial address decode):
//   $0000–$07FF  RAM (2 KB, mirrored at $0800–$0FFF via A11 don't-care)
//   $1800–$1BFF  VIA#1 — IEC bus (16 regs mirrored every 16 bytes)
//   $1C00–$1FFF  VIA#2 — Drive mechanics (same mirroring)
//   $C000–$FFFF  ROM (16 KB)
//   All other addresses: open bus
//
// Chip slots:
//   [0] CPU       — MOS 6502, not bus-mapped
//   [1] RAM       — 2 KB at $0000, mirrored through $0FFF (effective_size)
//   [2] VIA#1     — MMIO at $1800, 16-register window, mirrored through $1BFF
//   [3] VIA#2     — MMIO at $1C00, 16-register window, mirrored through $1FFF
//   [4] ROM       — 16 KB at $C000
//

inline constexpr auto kDrive1541Manifest = make_manifest(
    Slot<MOS6502>   {.label = "MOS 6502"},
    Slot<RAMChip>   {.base_addr = 0x0000, .size_bytes = 0x0800,
                     .effective_size = 0x1000,                        // Mirror to fill $0000–$0FFF
                     .label = "RAM"},
    Slot<mos6522_t> {.base_addr = 0x1800, .addr_mask = 0xFFF0,
                     .bank_size = 0x0400,                             // Mirror across $1800–$1BFF
                     .label = "VIA 1 (IEC)"},
    Slot<mos6522_t> {.base_addr = 0x1C00, .addr_mask = 0xFFF0,
                     .bank_size = 0x0400,                             // Mirror across $1C00–$1FFF
                     .label = "VIA 2 (Drive)"},
    Slot<ROMChip>   {.base_addr = 0xC000, .size_bytes = 0x4000,
                     .label = "DOS ROM",
                     .rom = {"1541.rom|1541-c000.901229-05.bin"}}
);

// BusSpec: 16-bit address, 256-byte pages, 1 viewer, CS-tick enabled
using Drive1541BusSpec = ManifestBusSpec<kDrive1541Manifest, 16, 8, 1, true>;

// ============================================================================
// 1541 Board — typed component tuple with named references
// ============================================================================

struct Drive1541Board : Board<Drive1541BusSpec> {
    using ComponentTuple = decltype(kDrive1541Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases (indices 0–4)
    MOS6502&   cpu  = std::get<0>(components_);
    RAMChip&   ram  = std::get<1>(components_);
    mos6522_t& via1 = std::get<2>(components_);  // IEC bus VIA
    mos6522_t& via2 = std::get<3>(components_);  // Drive mechanics VIA
    ROMChip&   rom  = std::get<4>(components_);

    Drive1541Board() : Board(kDrive1541Manifest) {}
};

// ============================================================================
// C1541 Drive System
// ============================================================================

template <const DriveTraits& Traits>
class C1541System {
    static_assert(Traits.is_1541_family(), "C1541System requires a 1541-family DriveTraits");

public:
    C1541System();

    // ── Identity ────────────────────────────────────────────────────────

    static constexpr const char* model_id()      { return Traits.model_id; }
    static constexpr const char* display_name()   { return Traits.display_name; }
    static constexpr uint32_t    cpu_clock_hz()   { return Traits.cpu_clock_hz; }
    static constexpr DriveModel  model()          { return Traits.model; }

    // ── IEC bus attachment ──────────────────────────────────────────────

    /// Attach to a shared IEC bus.  The slot determines which participant
    /// output this drive writes to (1 = device #8, 2 = device #9, etc.).
    void attach_iec_bus(IECBus* bus, int slot) {
        iec_bus_  = bus;
        iec_slot_ = slot;
    }

    // ── Core tick interface ─────────────────────────────────────────────

    /// Advance the drive by exactly one CPU cycle.
    /// Called from the host system's tick loop.
    void tick();

    /// Advance the drive by N CPU cycles (convenience for batch catch-up).
    void advance(uint32_t cycles) {
        for (uint32_t i = 0; i < cycles; ++i)
            tick();
    }

    /// Full reset (power-on equivalent).
    void reset();

    // ── ROM loading ─────────────────────────────────────────────────────

    /// Load the drive ROM from a file path.
    bool load_rom_file(const char* path);

    /// Attempt to load ROMs from the standard data directory.
    bool load_roms(const char* rom_root);

    // ── Disk image ──────────────────────────────────────────────────────

    bool insert_disk(const char* filepath) { return disk_.load(filepath); }
    void eject_disk()                      { disk_.eject(); }
    bool swap_disk(const char* filepath);
    bool is_disk_inserted() const          { return disk_.loaded; }
    const std::string& disk_path() const   { return disk_.filepath; }

    // ── Status queries (for warp detection, UI, etc.) ───────────────────

    bool is_motor_on() const   { return head_.motor_on; }
    bool is_led_on() const     { return head_.led_on; }
    uint8_t current_track() const { return head_.track(); }
    uint8_t current_half_track() const { return head_.half_track; }

    /// True when the drive is actively performing disk I/O.
    /// Used by WarpController to trigger warp mode.
    bool is_active() const { return head_.motor_on; }

    // ── Direct chip access (for debug UI, save states) ──────────────────

    Drive1541Board& board()              { return board_; }
    const Drive1541Board& board() const  { return board_; }

    MOS6502&   cpu()  { return board_.cpu; }
    mos6522_t& via1() { return board_.via1; }
    mos6522_t& via2() { return board_.via2; }

    const MOS6502&   cpu() const  { return board_.cpu; }
    const mos6522_t& via1() const { return board_.via1; }
    const mos6522_t& via2() const { return board_.via2; }

    uint64_t total_cycles() const { return total_cycles_; }

private:
    // ── IEC bus interface (VIA#1 port B ↔ IEC lines) ────────────────────

    /// Sync VIA#1 port B input pins from the IEC bus combined state.
    void iec_update_via1_input();

    /// Push VIA#1 port B output to the IEC bus participant slot.
    void iec_update_bus_output();

    // VIA#1 Port B bit assignments:
    //   Bit 0: DATA IN  (input, active-low from IEC bus)
    //   Bit 1: DATA OUT (output, active-low to IEC bus)
    //   Bit 2: CLK IN   (input, active-low from IEC bus)
    //   Bit 3: CLK OUT  (output, active-low to IEC bus)
    //   Bit 4: ATN ACK  (output, auto-acknowledge circuit)
    //   Bit 5: Device address jumper (input, active-low)
    //   Bit 6: Device address jumper (input, active-low)
    //   Bit 7: ATN IN   (input, from IEC bus)
    //
    // Port A is connected to the GCR data register (read/write byte from disk).

    static constexpr uint8_t VIA1_PB_DATA_IN  = 0x01;
    static constexpr uint8_t VIA1_PB_DATA_OUT = 0x02;
    static constexpr uint8_t VIA1_PB_CLK_IN   = 0x04;
    static constexpr uint8_t VIA1_PB_CLK_OUT  = 0x08;
    static constexpr uint8_t VIA1_PB_ATN_ACK  = 0x10;
    static constexpr uint8_t VIA1_PB_DEV_ADDR = 0x60;
    static constexpr uint8_t VIA1_PB_ATN_IN   = 0x80;

    // ── Drive mechanics (VIA#2 ↔ motor, head, GCR) ─────────────────────

    /// Process VIA#2 port B output changes (motor control, LED, stepper).
    void drive_mechanics_update();

    /// Advance the GCR read/write head by one bit period.
    void gcr_advance();

    // VIA#2 Port B bit assignments:
    //   Bit 0-1: Step motor phase (head positioning)
    //   Bit 2:   Motor on (1 = spinning)
    //   Bit 3:   Drive LED (1 = on)
    //   Bit 4:   Write protect sensor (input, active-low)
    //   Bit 5-6: Density select (GCR speed zone)
    //   Bit 7:   SYNC detect (input, 10+ consecutive 1-bits)

    static constexpr uint8_t VIA2_PB_STEPPER  = 0x03;
    static constexpr uint8_t VIA2_PB_MOTOR    = 0x04;
    static constexpr uint8_t VIA2_PB_LED      = 0x08;
    static constexpr uint8_t VIA2_PB_WP       = 0x10;
    static constexpr uint8_t VIA2_PB_DENSITY  = 0x60;
    static constexpr uint8_t VIA2_PB_SYNC     = 0x80;

    // ── Board + Bus ─────────────────────────────────────────────────────

    using Bus = MemoryBus<Drive1541BusSpec>;
    Bus            bus_;
    Drive1541Board board_;

    // ── IEC bus ─────────────────────────────────────────────────────────

    IECBus*  iec_bus_  = nullptr;
    int      iec_slot_ = 1;
    uint8_t  device_number_ = 8;
    uint8_t  prev_atn_ = 1;

    // ── Disk state ──────────────────────────────────────────────────────

    D64Image       disk_;
    DriveHeadState head_;

    // ── Timing ──────────────────────────────────────────────────────────

    uint64_t    total_cycles_ = 0;
    bus_state_t pins_ = 0;

    // ── VIA callbacks ───────────────────────────────────────────────────

    static uint8_t via1_port_b_read(void* ctx, uint8_t output);
    static void    via1_port_b_write(void* ctx, uint8_t data);
    static uint8_t via2_port_a_read(void* ctx, uint8_t output);
    static void    via2_port_b_write(void* ctx, uint8_t data);
};

// ── Explicit instantiation declarations ─────────────────────────────────────

extern template class C1541System<CBM1541Traits>;
extern template class C1541System<CBM1541CTraits>;
extern template class C1541System<CBM1541IITraits>;

// ── Convenience aliases ─────────────────────────────────────────────────────

using C1541   = C1541System<CBM1541Traits>;
using C1541C  = C1541System<CBM1541CTraits>;
using C1541II = C1541System<CBM1541IITraits>;
