#pragma once
// iec_bus.hpp — IEC Serial Bus shared state
//
// Models the physical IEC serial bus as a shared, wired-AND signal aggregate.
// Both the host system (C64/C128/VIC-20/C16) and attached drive systems
// (1541, 1571, 1581, …) read and write through this structure.
//
// Design constraints:
//   - All fields are naturally aligned for future std::atomic upgrade.
//   - One writer per "side" (host vs. each device), multiple readers.
//   - The combined line state is recomputed on every read — no stale cache.
//   - Active-low open-collector: a line is LOW if ANY participant pulls it LOW.
//     The wired-AND of all outputs gives the bus state.
//
// Threading model (current):
//   Single-thread interleaved — host tick loop calls drive.advance()
//   after each host cycle.  No atomics needed yet.
//
// Threading model (future):
//   Replace uint8_t fields with std::atomic<uint8_t> using relaxed ordering.
//   Add a leash counter (std::atomic<int64_t>) for cycle-skew governance.
//   The interface remains identical — callers just see the combined state.

#include <cstdint>

// ── IEC line bit positions (matches PortSignals::IECBit) ────────────────────

namespace iec {

enum Line : uint8_t {
    ATN   = 0,  // Attention    — host-only, directly active-low
    CLK   = 1,  // Clock        — open-collector, active-low
    DATA  = 2,  // Data         — open-collector, active-low
    SRQ   = 3,  // Service Req  — directly active-low (directly active-low)
    RESET = 4,  // Reset        — active-low
    COUNT = 5,
};

inline constexpr uint8_t ALL_RELEASED = 0x1F;  // All 5 lines HIGH (released)

} // namespace iec

// ── Participant output ──────────────────────────────────────────────────────

/// One participant's contribution to the IEC bus.
/// Bits set to 1 = released (high), bits cleared to 0 = pulled low.
/// Default: all lines released (passive).
struct IECOutput {
    uint8_t lines = iec::ALL_RELEASED;

    void pull_low(iec::Line line)  { lines &= ~(1u << line); }
    void release(iec::Line line)   { lines |=  (1u << line); }
    void set(iec::Line line, bool high) {
        if (high) release(line); else pull_low(line);
    }
    bool is_low(iec::Line line) const { return !(lines & (1u << line)); }
    bool is_high(iec::Line line) const { return  (lines & (1u << line)); }
};

// ── IEC Bus ─────────────────────────────────────────────────────────────────

/// Maximum number of devices on a single IEC bus (host counts as slot 0).
/// Slots 1–3 correspond to device numbers 8–10.
inline constexpr int IEC_MAX_PARTICIPANTS = 4;

/// Shared IEC bus state.
///
/// The host system creates this and passes a pointer to each drive system.
/// Both sides call read_line() to observe the combined open-collector state
/// and write their output via their own IECOutput slot.
///
/// By convention:
///   - Slot 0 = host system (C64 CIA#2 output)
///   - Slot 1 = device #8  (first drive)
///   - Slot 2 = device #9  (second drive)
///   - Slot 3 = device #10 (third drive)
struct IECBus {
    IECOutput participants[IEC_MAX_PARTICIPANTS];

    // ── Write interface ─────────────────────────────────────────────────

    /// Get mutable reference to a participant's output (for the owner to modify).
    IECOutput& output(int slot) { return participants[slot]; }

    // ── Read interface ──────────────────────────────────────────────────

    /// Compute the combined bus state (wired-AND of all participants).
    /// A line is LOW if ANY participant pulls it LOW.
    uint8_t combined() const {
        uint8_t result = iec::ALL_RELEASED;
        for (int i = 0; i < IEC_MAX_PARTICIPANTS; ++i)
            result &= participants[i].lines;
        return result;
    }

    /// Read a single line from the combined bus state.
    bool line_high(iec::Line line) const { return  (combined() & (1u << line)); }
    bool line_low(iec::Line line) const  { return !(combined() & (1u << line)); }

    // Convenience — read all lines for a participant to compare against.
    bool atn() const   { return line_high(iec::ATN); }
    bool clk() const   { return line_high(iec::CLK); }
    bool data() const  { return line_high(iec::DATA); }
    bool srq() const   { return line_high(iec::SRQ); }
    bool reset() const { return line_high(iec::RESET); }

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Release all participants (power-on state).
    void reset_all() {
        for (auto& p : participants)
            p.lines = iec::ALL_RELEASED;
    }
};
