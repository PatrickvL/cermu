// drive_1541_system.cpp — Cycle-accurate 1541 drive system implementation
//
// Instantiates C1541System<Traits> for all 1541 variants and provides
// the tick loop, memory map, IEC bus bridge, and drive mechanics.

#include "devices/storage/drive_1541_system.hpp"
#include "core/bus_defs.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

// ============================================================================
// D64Image — Disk image container
// ============================================================================

bool D64Image::load(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    auto size = f.tellg();
    if (size != SIZE_35_TRACKS && size != SIZE_35_TRACKS_ERR &&
        size != SIZE_40_TRACKS && size != SIZE_40_TRACKS_ERR)
        return false;

    data.resize(static_cast<size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), size);

    loaded    = true;
    filepath  = path;
    num_tracks = (size >= SIZE_40_TRACKS) ? 40 : 35;
    return true;
}

void D64Image::eject() {
    data.clear();
    loaded    = false;
    read_only = false;
    num_tracks = 35;
    filepath.clear();
}

bool D64Image::read_sector(uint8_t track, uint8_t sector, uint8_t* out) const {
    if (!loaded || track < 1 || track > num_tracks) return false;
    if (sector >= sectors_per_track(track)) return false;
    uint32_t offset = track_sector_offset(track, sector);
    if (offset + 256 > data.size()) return false;
    std::memcpy(out, data.data() + offset, 256);
    return true;
}

bool D64Image::write_sector(uint8_t track, uint8_t sector, const uint8_t* in) {
    if (!loaded || read_only || track < 1 || track > num_tracks) return false;
    if (sector >= sectors_per_track(track)) return false;
    uint32_t offset = track_sector_offset(track, sector);
    if (offset + 256 > data.size()) return false;
    std::memcpy(data.data() + offset, in, 256);
    return true;
}

uint8_t D64Image::sectors_per_track(uint8_t track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;  // Tracks 31-40
}

uint32_t D64Image::track_sector_offset(uint8_t track, uint8_t sector) {
    // Accumulate sectors for all tracks before this one.
    uint32_t offset = 0;
    for (uint8_t t = 1; t < track; ++t)
        offset += sectors_per_track(t) * 256;
    return offset + sector * 256;
}

// ============================================================================
// C1541System — Constructor (Board + Bus initialization)
// ============================================================================

template <const DriveTraits& Traits>
C1541System<Traits>::C1541System() {
    // Bind all manifest components (CPU, RAM, VIA1, VIA2, ROM) to the board.
    bind_all(board_, board_.components_, kDrive1541Manifest);

    // Wire page tables and MMIO handlers from the manifest.
    board_.apply(bus_);

    // VIAs assert IRQ on the CPU bus
    board_.via1.interrupt_bit = BUS_IRQ_BIT;
    board_.via2.interrupt_bit = BUS_IRQ_BIT;

    // Wire VIA callbacks for IEC bus and drive mechanics
    board_.via1.set_port_b_read_callback(via1_port_b_read, this);
    board_.via1.set_port_b_write_callback(via1_port_b_write, this);
    board_.via2.set_port_a_read_callback(via2_port_a_read, this);
    board_.via2.set_port_b_write_callback(via2_port_b_write, this);

    // Initialize CPU
    pins_ = board_.cpu.init();

    reset();
}

// ============================================================================
// C1541System — Reset
// ============================================================================

template <const DriveTraits& Traits>
void C1541System<Traits>::reset() {
    // Reset all chips (VIA reset preserves callbacks and interrupt_bit)
    board_.cpu.reset();
    board_.via1.reset();
    board_.via2.reset();

    // Reset drive head
    head_ = DriveHeadState{};

    // Reset IEC output
    if (iec_bus_)
        iec_bus_->output(iec_slot_).lines = iec::ALL_RELEASED;

    prev_atn_ = 1;
    total_cycles_ = 0;
    pins_ = MOS6502::default_bus_state();
}

// ============================================================================
// ROM loading
// ============================================================================

template <const DriveTraits& Traits>
bool C1541System<Traits>::load_rom_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    auto size = f.tellg();
    if (static_cast<size_t>(size) != board_.rom.size_bytes()) return false;

    f.seekg(0);
    f.read(reinterpret_cast<char*>(board_.rom.data()), size);
    return true;
}

template <const DriveTraits& Traits>
bool C1541System<Traits>::load_roms(const char* rom_root) {
    return board_.load_roms(rom_root, "1541");
}

template <const DriveTraits& Traits>
bool C1541System<Traits>::swap_disk(const char* filepath) {
    disk_.eject();
    return disk_.load(filepath);
}

// ── Tick loop ───────────────────────────────────────────────────────────────
//
// One call = one 1541 CPU cycle (1 μs at 1 MHz).
//
// Phase ordering mirrors the real hardware:
//   1. VIA timer ticks (generates IRQ if pending)
//   2. IEC bus input is sampled into VIA#1
//   3. CPU PHI2 (instruction execution)
//   4. Bus resolve + service + MMIO self-dispatch
//   5. CPU PHI1 (prepare next fetch)
//   6. Push VIA#1 output → IEC bus
//   7. Drive mechanics advance (motor, head, GCR)

template <const DriveTraits& Traits>
void C1541System<Traits>::tick() {
    total_cycles_++;

    bus_state_t s = pins_;

    // Phase 1: VIA timer ticks + IRQ generation
    s = board_.via1.tick(s);
    s = board_.via2.tick(s);

    // Phase 2: Sample IEC bus → VIA#1 port B input pins
    iec_update_via1_input();

    // Phase 3: CPU PHI2 — instruction execution
    s = board_.cpu.template tick<MOS6502::Phase::PHI2>(s);

    // Phase 4: Bus resolve + service + MMIO self-dispatch
    //   resolve() decodes address → chip ID in CS field
    //   service() handles RAM/ROM buffer reads/writes
    //   tick_mmio() on each VIA checks CS match and responds
    s = bus_.resolve(s);
    s = bus_.service(s);
    s = board_.via1.tick_mmio(s);
    s = board_.via2.tick_mmio(s);

    // Phase 5: CPU PHI1 — prepare next fetch
    s = board_.cpu.template tick<MOS6502::Phase::PHI1>(s);
    board_.cpu.sample_nmi_pin(s);

    // Phase 6: Push VIA#1 output → IEC bus
    iec_update_bus_output();

    // Phase 7: Advance drive mechanics
    if (head_.motor_on)
        gcr_advance();

    drive_mechanics_update();

    BUS_SET_BIT(s, BUS_RW_BIT);  // Default to read for next cycle
    pins_ = s;
}

// ── IEC bus bridge ──────────────────────────────────────────────────────────
//
// VIA#1 Port B mapping (from 1541 schematic):
//
//   Bit 0 (input):  DATA IN   — from IEC DATA line (inverted)
//   Bit 1 (output): DATA OUT  — drives IEC DATA line (active-low)
//   Bit 2 (input):  CLK IN    — from IEC CLK line (inverted)
//   Bit 3 (output): CLK OUT   — drives IEC CLK line (active-low)
//   Bit 4 (output): ATN ACK   — drives DATA low when ATN is asserted
//   Bit 5 (input):  Device address config (mainboard jumper, active-low)
//   Bit 6 (input):  Device address config
//   Bit 7 (input):  ATN IN    — directly from IEC ATN line (inverted)
//
// The physical bus uses active-low logic with open-collector/drain outputs.
// A HIGH on the VIA output register means "pull the line LOW" (inverted).

template <const DriveTraits& Traits>
void C1541System<Traits>::iec_update_via1_input() {
    if (!iec_bus_) return;

    uint8_t combined = iec_bus_->combined();
    uint8_t pb_in = 0;

    // DATA IN (bit 0): inverted from bus (bus LOW → VIA bit HIGH)
    if (!(combined & (1u << iec::DATA)))
        pb_in |= VIA1_PB_DATA_IN;

    // CLK IN (bit 2): inverted from bus
    if (!(combined & (1u << iec::CLK)))
        pb_in |= VIA1_PB_CLK_IN;

    // ATN IN (bit 7): inverted from bus
    if (!(combined & (1u << iec::ATN)))
        pb_in |= VIA1_PB_ATN_IN;

    // Device address jumpers (bits 5-6): default = device 8
    //   Device 8:  bits 5,6 = 0,0  (both jumpers cut = lines float high, inverted = low)
    //   Device 9:  bits 5,6 = 1,0
    //   Device 10: bits 5,6 = 0,1
    //   Device 11: bits 5,6 = 1,1
    uint8_t dev_offset = device_number_ - 8;
    pb_in |= (dev_offset & 0x03) << 5;

    board_.via1.port_b_pins_ = pb_in;

    // ATN edge detection — generates CA1 interrupt on VIA#1 when ATN transitions
    uint8_t atn_now = (combined & (1u << iec::ATN)) ? 1 : 0;
    if (atn_now != prev_atn_) {
        // CA1 negative edge triggers interrupt (ATN asserted = line goes LOW)
        if (!atn_now)
            board_.via1.ifr |= MOS6522_IFR_CA1;
        prev_atn_ = atn_now;
    }
}

template <const DriveTraits& Traits>
void C1541System<Traits>::iec_update_bus_output() {
    if (!iec_bus_) return;

    auto& out = iec_bus_->output(iec_slot_);
    uint8_t pb = board_.via1.port_b.read_output();

    // DATA OUT (bit 1): HIGH in register = pull DATA low on bus
    out.set(iec::DATA, !(pb & VIA1_PB_DATA_OUT));

    // CLK OUT (bit 3): HIGH in register = pull CLK low on bus
    out.set(iec::CLK, !(pb & VIA1_PB_CLK_OUT));

    // ATN ACK (bit 4): when ATN is asserted, the device automatically
    // pulls DATA low to acknowledge.  This is handled by the ATN logic
    // in the 1541's hardware (auto-acknowledge circuit).
    if (board_.via1.port_b_pins_ & VIA1_PB_ATN_IN) {
        // ATN is asserted (bit 7 HIGH = bus ATN LOW) — acknowledge by
        // additionally pulling DATA low if ATN ACK bit is set
        if (pb & VIA1_PB_ATN_ACK)
            out.pull_low(iec::DATA);
    }
}

// ── Drive mechanics ─────────────────────────────────────────────────────────

template <const DriveTraits& Traits>
void C1541System<Traits>::drive_mechanics_update() {
    uint8_t pb = board_.via2.port_b.read_output();

    // Motor control (bit 2)
    head_.motor_on = (pb & VIA2_PB_MOTOR) != 0;

    // LED (bit 3)
    head_.led_on = (pb & VIA2_PB_LED) != 0;

    // Speed zone / density (bits 5-6)
    head_.speed_zone = (pb & VIA2_PB_DENSITY) >> 5;

    // Step motor (bits 0-1): 4-phase stepper motor for head positioning
    uint8_t new_phase = pb & VIA2_PB_STEPPER;
    if (new_phase != head_.stepper_phase) {
        // Determine direction from phase sequence:
        //   Forward  (toward higher tracks): 0→1→2→3→0→...
        //   Backward (toward track 1):       3→2→1→0→3→...
        int8_t delta = static_cast<int8_t>(new_phase) - static_cast<int8_t>(head_.stepper_phase);
        // Handle wrap-around: 3→0 = +1, 0→3 = -1
        if (delta == 3) delta = -1;
        if (delta == -3) delta = 1;

        // Only move on single-phase steps (delta = ±1)
        if (delta == 1 || delta == -1) {
            int new_ht = head_.half_track + delta;
            // Clamp to physical range: track 1 (half_track 2) to track 42 (half_track 84)
            if (new_ht >= 2 && new_ht <= 84)
                head_.half_track = static_cast<uint8_t>(new_ht);
        }
        head_.stepper_phase = new_phase;
    }

    // Write-protect sensor (bit 4, input): active-low
    uint8_t wp_bit = disk_.read_only ? 0 : VIA2_PB_WP;
    // Set the input pin state (not through the output register)
    board_.via2.port_b_pins_ = (board_.via2.port_b_pins_ & ~VIA2_PB_WP) | wp_bit;
}

template <const DriveTraits& Traits>
void C1541System<Traits>::gcr_advance() {
    // TODO: Full GCR bitstream emulation.
    //
    // For now, this is a placeholder. The full implementation needs:
    // 1. GCR-encode D64 sector data into a bitstream per track
    // 2. Advance the bit pointer at the rate determined by the speed zone
    // 3. Feed bits into VIA#2 port A (GCR data register) via the shift register
    // 4. Detect SYNC marks (10+ consecutive 1-bits → set VIA#2 PB bit 7)
    // 5. Handle byte-ready signal (VIA#2 CA1, triggers SOE → byte available)
    //
    // This is the most complex part of 1541 emulation and will be built
    // incrementally.  The IEC bus protocol and drive CPU/VIA infrastructure
    // above can be tested with ROM code that doesn't directly access the
    // disk (e.g., the 1541 startup self-test, IEC handshake).
}

// ── VIA callbacks ──────────────────────────────────────────────────────────

template <const DriveTraits& Traits>
uint8_t C1541System<Traits>::via1_port_b_read(void* ctx, uint8_t output) {
    // Port B read combines output register with external input pins.
    // The VIA's io_port already handles DDR masking; we just need to
    // ensure the input pins reflect the current IEC bus state.
    auto* self = static_cast<C1541System*>(ctx);
    self->iec_update_via1_input();
    return self->board_.via1.port_b_pins_;
}

template <const DriveTraits& Traits>
void C1541System<Traits>::via1_port_b_write(void* ctx, uint8_t data) {
    // VIA#1 port B write — immediately update IEC bus output lines.
    auto* self = static_cast<C1541System*>(ctx);
    self->iec_update_bus_output();
}

template <const DriveTraits& Traits>
uint8_t C1541System<Traits>::via2_port_a_read(void* ctx, uint8_t output) {
    // VIA#2 port A = GCR data byte from the disk head.
    // TODO: Return the current GCR byte from the bitstream.
    (void)ctx;
    (void)output;
    return 0xFF;  // No data (placeholder)
}

template <const DriveTraits& Traits>
void C1541System<Traits>::via2_port_b_write(void* ctx, uint8_t data) {
    // VIA#2 port B write — update motor/LED/stepper immediately.
    auto* self = static_cast<C1541System*>(ctx);
    self->drive_mechanics_update();
}

// ── Explicit template instantiations ────────────────────────────────────────

template class C1541System<CBM1541Traits>;
template class C1541System<CBM1541CTraits>;
template class C1541System<CBM1541IITraits>;
