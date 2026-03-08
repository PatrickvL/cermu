#pragma once
/*
 * z80_ctc.h — Zilog Z80 CTC (Counter/Timer Circuit)
 *
 * The Z80 CTC (1977) provides four independent counter/timer channels
 * with interrupt capability and daisy-chain support.  Each channel can
 * operate as a timer (triggered by CPU clock) or as a counter (triggered
 * by an external clock/trigger pin).
 *
 * DDR clone: U857 (VEB MME Erfurt)
 *
 * Channel modes:
 *   Timer mode:   count down from time constant, clocked by system CLK / prescaler (16 or 256)
 *   Counter mode: count down from time constant, clocked by CLK/TRGn input edge
 *
 * Channels can be cascaded (e.g. CH0→CLK/TRG1) for longer periods.
 *
 * Register access (directly via IORQ, active-low accent):
 *   A0-A1 select channel (0-3)
 *   Channel register is either control word or time constant (selected by bit 2 of control)
 *
 * Used in: KC85 series, Z9001/KC87, many Z80 systems for baud rate generation,
 *          video timing, and periodic interrupts.
 *
 * 28-pin DIP package.
 */

#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include <cstdint>
#include <cstring>

// ============================================================================
// Z80 CTC Channel Control Bits
// ============================================================================

namespace ctc_ctrl {
    constexpr uint8_t INT_ENABLE  = 0x80;  // Bit 7: interrupt enable
    constexpr uint8_t COUNTER_MODE = 0x40; // Bit 6: 1=counter, 0=timer
    constexpr uint8_t PRESCALER   = 0x20;  // Bit 5: 1=256, 0=16 (timer mode only)
    constexpr uint8_t EDGE_SELECT = 0x10;  // Bit 4: trigger edge (1=rising, 0=falling)
    constexpr uint8_t TRIGGER     = 0x08;  // Bit 3: timer trigger (1=CLK/TRG pulse, 0=auto)
    constexpr uint8_t TC_FOLLOWS  = 0x04;  // Bit 2: next write is time constant
    constexpr uint8_t SW_RESET    = 0x02;  // Bit 1: software reset of channel
    constexpr uint8_t CONTROL     = 0x01;  // Bit 0: 1=control word, 0=vector (CH0 only)
} // namespace ctc_ctrl

// ============================================================================
// Z80 CTC
// ============================================================================

class z80_ctc_t : public ChipBase {
public:
    explicit z80_ctc_t(bool is_u857 = false)
        : ChipBase(ChipInfo(is_u857 ? "U857" : "Z80 CTC",
                             is_u857 ? "VEB MME Erfurt" : "Zilog"))
    {
        category_ = "I/O";
    }

    void init() {
        for (int i = 0; i < 4; ++i) {
            ch_[i] = {};
        }
        int_vector_base_ = 0x00;
    }

    void reset() { init(); }

    // === Register access ===

    /// Write to a CTC channel register (control or time constant).
    void write(int channel, uint8_t data) {
        auto& ch = ch_[channel & 3];

        if (channel == 0 && !(data & ctc_ctrl::CONTROL)) {
            // Channel 0, bit 0 = 0 → interrupt vector base (shared by all channels)
            int_vector_base_ = data & 0xF8;  // Lower 3 bits are channel index
            return;
        }

        if (ch.tc_follows) {
            // Time constant value
            ch.time_constant = data ? data : 256;  // 0 = 256
            ch.counter = ch.time_constant;
            ch.tc_follows = false;
            ch.running = true;
            return;
        }

        // Control word
        ch.control = data;
        ch.int_enabled  = (data & ctc_ctrl::INT_ENABLE) != 0;
        ch.counter_mode = (data & ctc_ctrl::COUNTER_MODE) != 0;
        ch.prescaler    = (data & ctc_ctrl::PRESCALER) ? 256 : 16;
        ch.edge_rising  = (data & ctc_ctrl::EDGE_SELECT) != 0;
        ch.tc_follows   = (data & ctc_ctrl::TC_FOLLOWS) != 0;

        if (data & ctc_ctrl::SW_RESET) {
            ch.running = false;
            ch.counter = ch.time_constant;
            ch.prescale_counter = 0;
        }
    }

    /// Read counter value from a CTC channel.
    uint8_t read(int channel) const {
        return static_cast<uint8_t>(ch_[channel & 3].counter & 0xFF);
    }

    /// Tick all timer-mode channels by one system clock.
    void tick() {
        for (int i = 0; i < 4; ++i) {
            auto& ch = ch_[i];
            if (!ch.running || ch.counter_mode) continue;

            ch.prescale_counter++;
            if (ch.prescale_counter >= ch.prescaler) {
                ch.prescale_counter = 0;
                if (--ch.counter == 0) {
                    ch.counter = ch.time_constant;
                    ch.zero_count = true;
                    if (ch.int_enabled) {
                        ch.int_pending = true;
                    }
                }
            }
        }
    }

    /// External trigger/clock input for counter-mode channels.
    void trigger(int channel, bool edge) {
        auto& ch = ch_[channel & 3];
        if (!ch.running || !ch.counter_mode) return;

        bool active = ch.edge_rising ? edge : !edge;
        if (active && !ch.prev_trigger) {
            if (--ch.counter == 0) {
                ch.counter = ch.time_constant;
                ch.zero_count = true;
                if (ch.int_enabled) {
                    ch.int_pending = true;
                }
            }
        }
        ch.prev_trigger = active;
    }

    // === Interrupt support ===

    bool interrupt_pending() const {
        for (const auto& ch : ch_) {
            if (ch.int_pending) return true;
        }
        return false;
    }

    /// Get interrupt vector for highest-priority pending channel.
    uint8_t interrupt_vector() const {
        for (int i = 0; i < 4; ++i) {
            if (ch_[i].int_pending) {
                return int_vector_base_ | (i << 1);
            }
        }
        return int_vector_base_;
    }

    /// Check if a specific channel reached zero count (and clear the flag).
    bool check_zero_count(int channel) {
        bool zc = ch_[channel & 3].zero_count;
        ch_[channel & 3].zero_count = false;
        return zc;
    }

private:
    struct Channel {
        uint8_t  control = 0;
        uint16_t time_constant = 256;
        uint16_t counter = 256;
        uint16_t prescaler = 16;
        uint16_t prescale_counter = 0;
        bool     running = false;
        bool     counter_mode = false;
        bool     edge_rising = false;
        bool     int_enabled = false;
        bool     int_pending = false;
        bool     tc_follows = false;
        bool     zero_count = false;
        bool     prev_trigger = false;
    };

    Channel ch_[4]{};
    uint8_t int_vector_base_ = 0x00;
};
