#pragma once
/*
 * keyboard_encoder.hpp — Serial keyboard encoder chip
 *
 * Models a keyboard matrix scanner that transmits key codes as timed
 * serial pulses. The receiver (typically a Z80 PIO in bit-control mode)
 * measures the interval between pulses using a timer (CTC) to determine
 * whether each bit is a 0 or 1, or if the byte is complete.
 *
 * This is the protocol used by the U807 keyboard encoder in the
 * KC85/2, KC85/3, and KC85/4 home computers (VEB Mühlhausen, DDR).
 * The chip may also be applicable to other systems using similar
 * serial keyboard encoders (e.g. Z9001/KC87 variants).
 *
 * Protocol:
 *   The encoder scans an N×M keyboard matrix. When a key is detected,
 *   it transmits the key's scancode (8 bits) as a series of 9 pulses
 *   (8 data + 1 end-of-byte marker) on a single output line.
 *
 *   The receiver's ISR fires on each pulse and reads a timer to measure
 *   the elapsed time since the previous pulse:
 *     - Short interval → bit value 0
 *     - Medium interval → bit value 1
 *     - Long interval → scancode byte complete
 *
 *   Bits are shifted into the scancode MSB-first (via RR instruction
 *   with carry, building the byte from bit 7 down to bit 0).
 *
 *   After all 8 bits + end marker, the receiver looks up the scancode
 *   in a ROM table (KTAB) to get the ASCII key code.
 *
 * Timing (KC85 defaults, CPU @ ~1.77 MHz, CTC prescaler 256):
 *   - Bit 0 pulse interval:      ~7,000 CPU cycles
 *   - Bit 1 pulse interval:     ~14,000 CPU cycles
 *   - End-of-byte pulse interval: ~22,000 CPU cycles
 *
 * Header-only — no side effects, suitable for inline use.
 */

#include "chip/input/input_chip_base.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// Configuration — timing and matrix parameters
// ============================================================================

struct KeyboardEncoderConfig {
    // Matrix dimensions
    uint8_t rows = 8;
    uint8_t cols = 8;

    // Pulse intervals in CPU cycles
    // These define the time between consecutive pulses on the serial line.
    // The receiver's timer measures these intervals to decode bits.
    uint32_t pulse_interval_bit0     = 7000;   // Short: decoded as bit 0
    uint32_t pulse_interval_bit1     = 14000;  // Medium: decoded as bit 1
    uint32_t pulse_interval_end_byte = 22000;  // Long: signals byte complete

    // Debounce: minimum ticks a key must be stable before triggering
    uint32_t debounce_ticks = 1000;

    // Inter-key delay: ticks between end of one scancode and start of next
    // (if the same key is still held, this controls the hardware repeat rate)
    uint32_t repeat_delay_ticks = 100000;       // ~56 ms at 1.77 MHz
    uint32_t repeat_interval_ticks = 20000;     // ~11 ms at 1.77 MHz
};

// ============================================================================
// Keyboard Encoder
// ============================================================================

class KeyboardEncoder : public InputChipBase {
public:
    explicit KeyboardEncoder(const KeyboardEncoderConfig& cfg = {},
                             const char* name = "U807",
                             const char* manufacturer = "VEB Mikroelektronik Erfurt")
        : InputChipBase(ChipInfo{name, manufacturer, "Keyboard Encoder"})
        , config_(cfg)
    {
        static_assert(sizeof(matrix_) >= 16, "matrix_ must hold at least 16 rows");
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // ── Matrix input ─────────────────────────────────────────────────

    /// Set a single key state. row/col are 0-indexed.
    /// pressed=true means the key contact is closed.
    void set_key(uint8_t row, uint8_t col, bool pressed) {
        if (row >= config_.rows || col >= config_.cols) return;
        if (pressed)
            matrix_[row] &= ~(1 << col);   // Active-low: 0 = pressed
        else
            matrix_[row] |= (1 << col);    // 1 = released
    }

    /// Set an entire row's state (active-low bitmask).
    void set_row(uint8_t row, uint8_t state) {
        if (row < config_.rows)
            matrix_[row] = state;
    }

    /// Get a row's current state (active-low).
    uint8_t get_row(uint8_t row) const {
        return (row < config_.rows) ? matrix_[row] : 0xFF;
    }

    /// Clear all keys (set all matrix positions to released).
    void clear_all_keys() {
        std::memset(matrix_, 0xFF, sizeof(matrix_));
    }

    /// Convenience: set a matrix position from a flat scancode.
    /// scancode = row * cols + col.
    void set_key_by_scancode(uint8_t scancode, bool pressed) {
        uint8_t row = scancode / config_.cols;
        uint8_t col = scancode % config_.cols;
        set_key(row, col, pressed);
    }

    /// Set all keys from a flat array of row bytes (active-low).
    void set_matrix(const uint8_t* rows, uint8_t count) {
        uint8_t n = (count < config_.rows) ? count : config_.rows;
        std::memcpy(matrix_, rows, n);
    }

    // ── Serial output ────────────────────────────────────────────────

    /// Advance the encoder by one CPU cycle.
    /// Returns true on the exact cycle a pulse is emitted on the serial line.
    /// The system should trigger PIO-B BSTB (strobe) when this returns true.
    bool tick() {
        bool pulse = false;

        switch (state_) {
        case State::IDLE:
            tick_idle();
            break;

        case State::DEBOUNCE:
            if (++debounce_counter_ >= config_.debounce_ticks) {
                // Verify key is still pressed after debounce
                uint8_t sc = scan_matrix();
                if (sc != 0xFF) {
                    current_scancode_ = sc;
                    bit_index_ = 7;  // MSB first (RR shifts from carry into bit 7)
                    state_ = State::SENDING_BIT;
                    interval_counter_ = 0;
                    // First pulse: marks start of transmission
                    pulse = true;
                } else {
                    state_ = State::IDLE;
                }
            }
            break;

        case State::SENDING_BIT:
            if (++interval_counter_ >= current_interval()) {
                interval_counter_ = 0;
                pulse = true;

                if (bit_index_ < 0) {
                    // We just sent the end-of-byte marker pulse
                    state_ = State::COOLDOWN;
                    cooldown_counter_ = 0;
                    last_scancode_ = current_scancode_;
                } else {
                    --bit_index_;
                    // bit_index_ goes from 7→0 for data bits, then -1 for end marker
                }
            }
            break;

        case State::COOLDOWN:
            // Wait before scanning again (allows repeat or new key)
            if (++cooldown_counter_ >= config_.repeat_interval_ticks) {
                state_ = State::IDLE;
            }
            break;
        }

        return pulse;
    }

    /// Check if the encoder is currently transmitting.
    bool is_transmitting() const { return state_ == State::SENDING_BIT; }

    /// Get the scancode currently being transmitted (or last transmitted).
    uint8_t current_scancode() const { return current_scancode_; }
    uint8_t last_transmitted_scancode() const { return last_scancode_; }

    // ── Lifecycle ────────────────────────────────────────────────────

    void reset() {
        std::memset(matrix_, 0xFF, sizeof(matrix_));
        state_ = State::IDLE;
        current_scancode_ = 0;
        last_scancode_ = 0xFF;
        bit_index_ = -1;
        interval_counter_ = 0;
        debounce_counter_ = 0;
        cooldown_counter_ = 0;
    }

    const KeyboardEncoderConfig& config() const { return config_; }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    enum class State : uint8_t {
        IDLE,           // Waiting for key press
        DEBOUNCE,       // Key detected, waiting for stable contact
        SENDING_BIT,    // Transmitting scancode bits as timed pulses
        COOLDOWN,       // Post-transmission delay before next scan
    };

    KeyboardEncoderConfig config_;

    // Keyboard matrix state (active-low: 0 = pressed, 1 = released)
    uint8_t matrix_[16] = {};

    // Transmission state
    State    state_ = State::IDLE;
    uint8_t  current_scancode_ = 0;
    uint8_t  last_scancode_ = 0xFF;
    int8_t   bit_index_ = -1;         // 7..0 for data bits, -1 = end marker
    uint32_t interval_counter_ = 0;
    uint32_t debounce_counter_ = 0;
    uint32_t cooldown_counter_ = 0;

    /// Scan the matrix for the first pressed key.
    /// Returns scancode (row*cols + col) or 0xFF if no key pressed.
    uint8_t scan_matrix() const {
        for (uint8_t row = 0; row < config_.rows; ++row) {
            uint8_t state = matrix_[row];
            if (state != 0xFF) {
                for (uint8_t col = 0; col < config_.cols; ++col) {
                    if (!(state & (1 << col))) {
                        return row * config_.cols + col;
                    }
                }
            }
        }
        return 0xFF;  // No key pressed
    }

    /// Get the pulse interval for the current bit being transmitted.
    uint32_t current_interval() const {
        if (bit_index_ < 0) {
            // End-of-byte marker: long interval
            return config_.pulse_interval_end_byte;
        }
        // Data bit: short for 0, medium for 1
        bool bit_val = (current_scancode_ >> bit_index_) & 1;
        return bit_val ? config_.pulse_interval_bit1 : config_.pulse_interval_bit0;
    }

    void tick_idle() {
        uint8_t sc = scan_matrix();
        if (sc != 0xFF) {
            state_ = State::DEBOUNCE;
            debounce_counter_ = 0;
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using KE = const KeyboardEncoder;
        static const char* const state_names[] = {
            "IDLE", "DEBOUNCE", "SENDING", "COOLDOWN"
        };
        debug_registry_
            .category("Keyboard Encoder")
            .value("State", +[](const ChipBase* c) -> uint32_t {
                return static_cast<uint32_t>(static_cast<KE*>(c)->state_);
            })
            .value("Scancode", +[](const ChipBase* c) -> uint32_t {
                return static_cast<KE*>(c)->current_scancode_;
            })
            .value("Bit Index", +[](const ChipBase* c) -> uint32_t {
                auto idx = static_cast<KE*>(c)->bit_index_;
                return idx < 0 ? 0xFF : static_cast<uint32_t>(idx);
            })
            .value("Last Scancode", +[](const ChipBase* c) -> uint32_t {
                return static_cast<KE*>(c)->last_scancode_;
            });

        // Matrix rows
        for (uint8_t r = 0; r < 8; ++r) {
            // Can't capture r in a static lambda, so we use the row index
            // encoded in the category name
        }
    }
#endif
};
