// drive_1541_system.cpp — Cycle-accurate 1541 drive system implementation
//
// Instantiates C1541System<Traits> for all 1541 variants and provides
// the tick loop, memory map, IEC bus bridge, and drive mechanics.

#include "devices/storage/drive_1541_system.hpp"
#include "core/vfs/vfs.hpp"

#include <cstring>
#include <fstream>

// ============================================================================
// D64Image — Disk image container
// ============================================================================

bool D64Image::load(const char* path) {
    // Use VFS so archive paths ("game.zip!/disk.d64") work transparently.
    size_t size = 0;
    uint8_t* raw = vfs_read_file(path, &size);
    if (!raw) return false;

    if (size != SIZE_35_TRACKS && size != SIZE_35_TRACKS_ERR &&
        size != SIZE_40_TRACKS && size != SIZE_40_TRACKS_ERR) {
        free(raw);
        return false;
    }

    data.assign(raw, raw + size);
    free(raw);

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

    // Reset GCR bitstream state
    last_read_data_ = 0;
    gcr_data_byte_  = 0xFF;
    bit_counter_    = 0;
    sync_detected_  = false;
    byte_ready_     = false;
    ue7_counter_    = 0;
    uf4_counter_    = 0;
    gcr_cached_track_ = 0;

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
    if (!disk_.load(filepath)) return false;
    encode_disk_to_gcr();
    return true;
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

    // Start each cycle with default bus state (all control lines released).
    // VIAs re-assert IRQ each cycle if they still have enabled interrupts.
    // Without this precharge, IRQ stays asserted forever once any VIA fires.
    bus_state_t s = MOS6502::default_bus_state();
    BUS_SET_ADDR(s, BUS_GET_ADDR(pins_));
    BUS_SET_DATA(s, BUS_GET_DATA(pins_));

    // Phase 1: VIA timer ticks + IRQ generation
    s = board_.via1.tick(s);
    s = board_.via2.tick(s);

    // Phase 2: Sample IEC bus → VIA#1 port B input pins
    iec_update_via1_input();

    // Drive SO pin from the byte-ready circuit.
    // On the real 1541, the UF4 byte-ready signal is gated by SOE (VIA#2
    // CB2 output).  When byte_ready_ is active and SOE is HIGH, the /SO
    // pin is driven LOW.  The CPU's sample_so_pin() detects the falling
    // edge and sets the V flag — this is how the disk controller code
    // ($Dxxx) polls for byte-ready using CLV + BVC.
    if (byte_ready_) {
        uint8_t pcr = board_.via2.pcr;
        bool soe = (pcr & 0xE0) == 0xE0;  // CB2 manual HIGH
        if (soe)
            s &= ~FAM65XX_SO;  // Pull SO LOW → falling edge sets V
    }

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
    board_.cpu.sample_so_pin(s);

    // Phase 6: Push VIA#1 output → IEC bus
    iec_update_bus_output();

    // Phase 7: Advance drive mechanics
    if (head_.motor_on)
        gcr_advance();

    drive_mechanics_update();

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

    // ATN IN (bit 7): inverted from bus (bus LOW → VIA bit HIGH)
    // Same inversion as DATA/CLK — the 1541's IEC input buffers (7414
    // Schmitt triggers) invert all signals.  VICE confirms via ^0x85 XOR
    // on read_prb: PB7=1 when ATN asserted (bus LOW), PB7=0 when released.
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
    uint8_t pb = board_.via1.port_b.output();

    // CLK OUT (bit 3): HIGH in register = pull CLK low on bus
    out.set(iec::CLK, !(pb & VIA1_PB_CLK_OUT));

    // DATA line is driven by TWO sources (active-low open-collector OR):
    //
    // 1. Software DATA OUT (VIA PB1): HIGH in register = pull DATA low.
    //
    // 2. ATN acknowledge XOR circuit (74LS86 on 1541 schematic):
    //    The hardware XOR gate compares the VIA PB4 (ATNA) output with the
    //    ATN input from the bus.  When ATNA == ATN_state, the XOR output is
    //    LOW → inverted by 7406 → transistor ON → DATA pulled low.
    //
    //    The XOR gate input from ATN goes through a 7414 inverter, so:
    //      XOR inputs: ATNA (PB4 pin) and inverted-ATN (~ATN_bus)
    //      XOR output HIGH (→ 7406 → pull DATA) when inputs DIFFER.
    //
    //    Truth table (verified against VICE):
    //      ATNA=0, ATN released: no pull  (idle, no acknowledge needed)
    //      ATNA=0, ATN asserted: PULL     (auto-acknowledge ATN)
    //      ATNA=1, ATN released: PULL     (ATNA held after ATN release)
    //      ATNA=1, ATN asserted: no pull  (ISR matched, waiting for release)
    //
    // VICE formula: drv_bus_DATA = NOT(PRB[1]) AND (PRB[4] XOR cpu_ATN)
    //   where cpu_ATN uses 0=asserted convention.  Our atn_active uses
    //   true=asserted, so the XOR becomes: atna ^ atn_active.

    bool data_out   = (pb & VIA1_PB_DATA_OUT) != 0;    // VIA PB1 software DATA (DDR-gated)

    // ATNA for XOR: physical PB4 pin state.  When DDR=output, driven by
    // register.  When DDR=input, pin floats HIGH (internal pull-up).
    // The XOR gate is connected to the physical pin, not the register.
    uint8_t ddr = *board_.via1.port_b.ddr;
    bool atna = (ddr & VIA1_PB_ATN_ACK)
        ? (*board_.via1.port_b.data & VIA1_PB_ATN_ACK) != 0
        : true;   // Pull-up HIGH when DDR=input
    bool atn_active = iec_bus_->line_low(iec::ATN);     // ATN asserted on bus (LOW = asserted)
    bool xor_pulls  = (atna ^ atn_active);              // XOR=1 → pull DATA

    out.set(iec::DATA, !(data_out || xor_pulls));
}

// ── Drive mechanics ─────────────────────────────────────────────────────────

template <const DriveTraits& Traits>
void C1541System<Traits>::drive_mechanics_update() {
    uint8_t pb = board_.via2.port_b.output();

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
    // The 1541 R/W timing chain:
    //   - UE7 divides the 16 MHz master clock by (16 - speed_zone)
    //   - UF4 counts 4 UE7 overflows per bit cell
    //   - On each bit cell, one flux bit is shifted into the 10-bit window
    //
    // The 1541 R/W timing chain:
    //   - UE7 divides the 16 MHz master clock by (16 - speed_zone)
    //   - UF4 counts 4 UE7 overflows per bit cell
    //   - At 1 MHz CPU clock, 16 ref ticks per CPU cycle
    //
    // Optimized: advance UE7 by 16 ref ticks arithmetically instead of
    // iterating 16 times.  At most 1-2 UE7 overflows per CPU cycle
    // (16/13 ≈ 1.23 max), so the while loop body runs 0-2 times.

    if (!disk_.loaded) return;
    ensure_gcr_track();

    uint8_t track = head_.track();
    if (track < 1 || track > 42) return;
    auto& trk = gcr_tracks_[track];
    if (trk.num_bits == 0) return;

    uint8_t ue7_threshold = static_cast<uint8_t>(16 - head_.speed_zone);  // 13-16

    // Advance UE7 counter by 16 reference ticks (16 MHz / 1 MHz)
    ue7_counter_ += 16;

    // Process any UE7 overflows (typically 0-2 per CPU cycle)
    while (ue7_counter_ >= ue7_threshold) {
        ue7_counter_ -= ue7_threshold;

        // UF4 divide-by-4 — bit cell fires at count 2
        uf4_counter_ = (uf4_counter_ + 1) & 0x03;
        if (uf4_counter_ != 0x02)
            continue;

        // ── One bit cell: shift a flux bit into the 10-bit window ──

        uint8_t bit = trk.read_bit(head_.bit_position);
        head_.bit_position = (head_.bit_position + 1) % trk.num_bits;

        last_read_data_ = ((last_read_data_ << 1) | bit) & 0x3FF;

        // SYNC detection: 10 consecutive 1-bits
        if (last_read_data_ == 0x3FF) {
            sync_detected_ = true;
            bit_counter_ = 0;
            byte_ready_ = false;
        } else if (sync_detected_ && !(last_read_data_ & 1)) {
            // First zero after SYNC — sync is over, start counting
            sync_detected_ = false;
        }

        // Count bits toward byte-ready (only when not in SYNC)
        if (!sync_detected_) {
            if (++bit_counter_ >= 8) {
                bit_counter_ = 0;
                gcr_data_byte_ = static_cast<uint8_t>(last_read_data_ & 0xFF);
                byte_ready_ = true;

                // Signal byte-ready on VIA#2 CA1 if SOE (byte-ready enable)
                // is active.  The 1541 uses VIA#2 CB2 as SOE (directly
                // accessible as PCR bit 5 in manual output mode).
                // When SOE is HIGH, byte-ready triggers CA1 interrupt.
                uint8_t pcr = board_.via2.pcr;
                bool soe = (pcr & 0xE0) >= 0xC0;  // CB2 manual HIGH
                if (soe) {
                    board_.via2.ifr |= MOS6522_IFR_CA1;
                }
            }
        }
    }

    // Update SYNC bit in VIA#2 port B (bit 7, active-LOW: 0 = SYNC detected)
    if (sync_detected_)
        board_.via2.port_b_pins_ &= ~VIA2_PB_SYNC;
    else
        board_.via2.port_b_pins_ |= VIA2_PB_SYNC;
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
    // Returns the last complete byte shifted in from the GCR bitstream.
    auto* self = static_cast<C1541System*>(ctx);
    self->byte_ready_ = false;
    return self->gcr_data_byte_;
}

template <const DriveTraits& Traits>
void C1541System<Traits>::via2_port_b_write(void* ctx, uint8_t data) {
    // VIA#2 port B write — update motor/LED/stepper immediately.
    auto* self = static_cast<C1541System*>(ctx);
    self->drive_mechanics_update();
}

// ── GCR disk encoding ──────────────────────────────────────────────────────

template <const DriveTraits& Traits>
void C1541System<Traits>::encode_disk_to_gcr() {
    if (!disk_.loaded) return;

    // Read disk ID bytes from BAM sector (track 18, sector 0, offsets 0xA2-0xA3)
    uint8_t bam[256];
    if (disk_.read_sector(18, 0, bam)) {
        disk_id1_ = bam[0xA2];
        disk_id2_ = bam[0xA3];
    } else {
        disk_id1_ = disk_id2_ = 0x30;  // Fallback: ASCII '0'
    }

    // Encode all tracks
    for (uint8_t t = 1; t <= disk_.num_tracks; ++t) {
        uint8_t num_sectors = gcr::sectors_per_track(t);
        std::vector<uint8_t> sector_buf(256 * num_sectors);
        std::vector<const uint8_t*> sector_ptrs(num_sectors);

        for (uint8_t s = 0; s < num_sectors; ++s) {
            disk_.read_sector(t, s, sector_buf.data() + s * 256);
            sector_ptrs[s] = sector_buf.data() + s * 256;
        }

        gcr_tracks_[t] = gcr::encode_track(t, sector_ptrs.data(), num_sectors,
                                            disk_id1_, disk_id2_);
    }

    gcr_dirty_ = false;
    gcr_cached_track_ = head_.track();
}

template <const DriveTraits& Traits>
void C1541System<Traits>::ensure_gcr_track() {
    uint8_t track = head_.track();

    // Re-encode all tracks if disk was just inserted
    if (gcr_dirty_ && disk_.loaded) {
        encode_disk_to_gcr();
    }

    // When the head moves to a different track, update the bit position
    // to a proportional position on the new track (approximate rotation continuity)
    if (track != gcr_cached_track_) {
        auto& old_trk = gcr_tracks_[gcr_cached_track_];
        auto& new_trk = gcr_tracks_[track];
        if (old_trk.num_bits > 0 && new_trk.num_bits > 0) {
            // Scale bit position proportionally
            head_.bit_position = static_cast<uint32_t>(
                static_cast<uint64_t>(head_.bit_position) * new_trk.num_bits / old_trk.num_bits
            ) % new_trk.num_bits;
        } else {
            head_.bit_position = 0;
        }
        gcr_cached_track_ = track;
    }
}

// ── Explicit template instantiations ────────────────────────────────────────

template class C1541System<CBM1541Traits>;
template class C1541System<CBM1541CTraits>;
template class C1541System<CBM1541IITraits>;
