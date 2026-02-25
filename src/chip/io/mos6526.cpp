#include "mos6526.h" // cia
#include "../../core/system_lines.h"
#include <string.h>
#include <stdlib.h>
#include <cstdio>

// =========================================================================
// SDR delay pipeline constants (per VICE ciacore.c)
// Each group is a countdown: bits shift LEFT each tick, action fires at ...0
// =========================================================================
#define CIA_SDR_TOGGLE_CNT2     0x0001u  // Countdown to toggling CNT
#define CIA_SDR_TOGGLE_CNT1     0x0002u
#define CIA_SDR_TOGGLE_CNT0     0x0004u  // Action: toggle CNT, shift data
#define CIA_SDR_TOGGLE_CNT_1    0x0008u

#define CIA_SDR_NOGGLE_CNT2     0x0010u  // Countdown to NOT toggling (fast timer)
#define CIA_SDR_NOGGLE_CNT1     0x0020u
#define CIA_SDR_NOGGLE_CNT0     0x0040u
#define CIA_SDR_NOGGLE_CNT_1    0x0080u

#define CIA_SDR_SET_SDR_IRQ3    0x0100u  // Countdown to setting SDR IRQ
#define CIA_SDR_SET_SDR_IRQ2    0x0200u
#define CIA_SDR_SET_SDR_IRQ1    0x0400u
#define CIA_SDR_SET_SDR_IRQ0    0x0800u  // Action: set ICR_SP

#define CIA_SDR_CNT0            0x1000u  // CNT output state history
#define CIA_SDR_CNT1            0x2000u
#define CIA_SDR_CNT2            0x4000u
#define CIA_SDR_CNT3            0x8000u

#define CIA_SDR_SET3        0x00010000u  // Countdown to loading SDR into shifter
#define CIA_SDR_SET2        0x00020000u
#define CIA_SDR_SET1        0x00040000u
#define CIA_SDR_SET0        0x00080000u  // Action: load SDR

#define CIA_SDR_LEFTMOST    0x00100000u

// Bits cleared after each shift
#define CIA_SDR_CLEAR   (CIA_SDR_NOGGLE_CNT2 | CIA_SDR_SET_SDR_IRQ3 | \
                         CIA_SDR_CNT0 | CIA_SDR_SET3 | CIA_SDR_LEFTMOST)

// Bits that indicate active pipeline operations
#define CIA_SDR_ACTIVE  (CIA_SDR_TOGGLE_CNT2 | CIA_SDR_TOGGLE_CNT1 |   \
                         CIA_SDR_TOGGLE_CNT0 | CIA_SDR_TOGGLE_CNT_1 |  \
                         CIA_SDR_NOGGLE_CNT2 | CIA_SDR_NOGGLE_CNT1 |   \
                         CIA_SDR_NOGGLE_CNT0 | CIA_SDR_NOGGLE_CNT_1 |  \
                         CIA_SDR_SET_SDR_IRQ3 | CIA_SDR_SET_SDR_IRQ2 | \
                         CIA_SDR_SET_SDR_IRQ1 | CIA_SDR_SET_SDR_IRQ0 | \
                         CIA_SDR_SET3 | CIA_SDR_SET2 |                 \
                         CIA_SDR_SET1 | CIA_SDR_SET0)

#define ALL_SDR_CNT     (CIA_SDR_CNT0|CIA_SDR_CNT1|CIA_SDR_CNT2|CIA_SDR_CNT3)

void mos6526_s::reset() {
    // "Hardware RESET resets all I/O lines to inputs, and
    // thanks to the CIA's internal pull-up resistors,
    // the inputs actually output logical high voltage level.
    // So, upon -RESET, the video bank 0 is selected automatically,
    // and older Kernals could leave it uninitialized."
    // "/RES - Reset Input
    //  A low on the / RES pin resets all internal registers.
    // The port pins are set as inputs and port registers to
    // zero (although a read of the ports will return all high
    // because of passive pullups). The timer control
    // registers are set to zero and the timer latches to all
    // ones. All other registers are reset to zero."
    memset(reg, 0, sizeof(reg));
    reg[TOD_HR] = 1; // According to powerup
    // Ports all high
    // "The lines PA0 and PA1 of the second CIA are the inverse of the
    // virtual VIC-II address lines VA14 and VA15, respectively."
    // So below writes result in VICBase to become $C000
    port_a_value = 0xFF;
    port_b_value = 0xFF;
    // Timer latches all ones
    reg[TIMER_OFFSET + TA_LO] = 0xFF;
    reg[TIMER_OFFSET + TA_HI] = 0xFF;
    reg[TIMER_OFFSET + TB_LO] = 0xFF;
    reg[TIMER_OFFSET + TB_HI] = 0xFF;
    // Timer counters are also set to all ones after reset
    // (loaded from latches since control registers are 0 = stopped)
    reg[TA_LO] = 0xFF;
    reg[TA_HI] = 0xFF;
    reg[TB_LO] = 0xFF;
    reg[TB_HI] = 0xFF;
    timer_counter_[A] = 0xFFFF;
    timer_counter_[B] = 0xFFFF;
    // Also reset implementation-related variables
    read_tod_delta = 0;
    write_tod_delta = 0;
    is_running_tod = false;
    tod_cycles = 0;
    tod_tick_counter = 0;
    // Serial shift register state
    sdr_delay = 0;
    shifter = 0;
    sr_bits = 0;
    sdr_valid = false;
    cnt_output_state = true;  // CNT idles HIGH
    sp_output_bit = false;
    interrupt_mask = 0;
    interrupt_mask_delayed = 0;  // IMR delay (chips imr1)
    pending_bus_lines = 0;  // No pending interrupt assertions
    prev_alarm_state = false;  // No alarm initially
    icr_read_this_cycle = false;  // Timer B Bug state
    timer_underflowed = 0;  // No underflow events initially
    
    // Initialize delay line (multi-cycle signal propagation)
    delay_line.Clear();
    
    // Initialize PB6/PB7 toggle flip-flops (cleared on reset per CIA6526.txt line 107)
    pb67_toggle = 0;
    
    // Initialize serial output state
    // Already reset above in the grouped SDR state initialization
    
    // Initialize bus snapshot for edge detection (CNT/FLAG have pull-ups → start HIGH)
    bus_snapshot_ = BUS_BIT(BUS_CNT_BIT) | BUS_BIT(BUS_FLAG_BIT);
    
    // Call port A change callback with initial value (all high due to pull-ups)
    if (port_a_change_callback) {
        port_a_change_callback(port_a_callback_context, port_a_value);
    }
}

// ChipBase identity
ChipIdentity mos6526_s::chip_identity() const {
    return {"MOS6526", "MOS Technology"};
}

bool mos6526_s::has_debug_content()    const { return true; }
bool mos6526_s::has_settings_content() const { return true; }
bool mos6526_s::has_layout_content()   const { return true; }

// INTERRUPT CONTROL REGISTER (ICR) handling

uint8_t mos6526_s::read_and_clear_interrupt_control_register() {
    // "The interrupt DATA register is cleared" (the /IRQ line
    // does NOT return high following a read of the DATA register!)
    uint8_t v = reg[ICR];
    
    // "interrupt can be prevented by reading the ICR at the time of the underflow."
    reg[ICR] = 0;
    
    // Timer B Bug: Remember ICR reads to block Timer B interrupts this cycle
    // Per chips_mos6526.hpp lines 476-477: Set flag that will be checked in interrupt handling
    icr_read_this_cycle = true;
    
    // NOTE: With the new pull-up resistor model, we don't need to manage delayed_irq.
    // The interrupt line will be released automatically in the next cycle when
    // tick() sees that ICR_IRQ is clear and doesn't assert the line.
    // The system tick will pull the line HIGH via pull-up resistors.
    return v;
}

void mos6526_s::check_interrupt_mask() {
    // "In order for an interrupt flag to set IR
    // and generate an Interrupt Request, the
    // corresponding MASK bit must be set."
    uint8_t masked_interrupts = reg[ICR] & interrupt_mask;
    
    if (masked_interrupts > 0) {
        // "Any interrupt which is enabled by the MASK register will
        // set the IR bit (MSB) of the DATA register and bring
        // the /IRQ pin low."
        //
        // With the new pull-up resistor model, we simply set ICR_IRQ.
        // The next tick() will see this and assert the interrupt line.
        // No need for delayed_irq flag - the interrupt persists until ICR is read.
        reg[ICR] |= ICR_IRQ;
    }
    // Per CIA datasheet: "Clearing the bit in the IMR may not clear the interrupt."
    // ICR_IRQ is ONLY cleared by reading the ICR register (read_and_clear_interrupt_control_register).
    // Once the interrupt flip-flop has been set, changing the IMR has no effect on ICR_IRQ.
}

void mos6526_s::write_interrupt_control_register(uint32_t v) {
    // Only consider the 5 interrupt bits (bit 5 and 6 must become 0)
    uint8_t bits = v & MASK5;
    // "When writing to the MASK register, if bit 7 (SET/CLEAR)
    // of data written is a ZERO, any mask bit written with a one
    // will be cleared, while those mask bits written with a zero
    // will be unaffected. If bit 7 of the data written is a ONE,
    // any mask bit written with a one will be set, while those
    // mask bits written with a zero will be unaffected."
    // "Bit 7: Source bit.
    
    // Per chips_mos6526.hpp lines 443-455: Update interrupt_mask_delayed
    // This creates a 1-cycle delay before the mask takes effect
    if ((v & ICR_S_C) == 0)
        // 0 = set bits 0..4 are clearing the according mask bit.
        interrupt_mask_delayed &= ~bits;
    else
        // 1 = set bits 0..4 are setting the according mask bit."
        interrupt_mask_delayed |= bits;

    // "When a condition in the ICR is true, setting the corresponding bit in the IMR must also set the interrupt."
    // "Clearing the bit in the IMR may not clear the interrupt."
    // IMPORTANT: We still need to call check_interrupt_mask() because it handles
    // both setting ICR_IRQ when masked interrupts exist AND clearing it when they don't.
    // The chips version uses immediate check with delayed mask (lines 461-463), but we
    // need the full clearing logic that our check_interrupt_mask provides.
    // However, we temporarily copy delayed mask to active mask for this check.
    uint8_t saved_mask = interrupt_mask;
    interrupt_mask = interrupt_mask_delayed;  // Temporarily use delayed mask
    check_interrupt_mask();
    interrupt_mask = saved_mask;  // Restore for end-of-cycle update
    // "Once the interrupt flip-flop has been set, changing the condition in the IMR has no effect."
}

// PORT/PERIPHERAL DATA / DATA DIRECTION handling

void mos6526_s::write_data_direction_port(uint32_t p, uint8_t v) { // p:A or B
    // Delegate to generic io_port which handles:
    //   - output→input transitions: pull up pins (passive pull-ups)
    //   - input→output transitions: drive PRA/PRB value onto pins
    // Note: Pull-up bits can be lowered by connected devices (keyboard, joystick, mouse)
    // when that happens AFTER the CIA cycle update!
    auto& port = (p == A) ? port_a : port_b;
    (void)port.write_ddr(v);
}

void mos6526_s::update_output_port(uint32_t p, uint8_t v) { // p:A or B
    // Drive port pin values through output mask
    // For port A: mask = DDRA (standard)
    // For port B: mask = IDDRB (DDRB with PBON forced outputs) — see update_internal_data_direction_port_b()
    auto& port = (p == A) ? port_a : port_b;
    uint8_t mask = (p == A) ? *port_a.ddr : reg[IDDRB_OFFSET];
    port.force_pins(v, mask);
    
    // Call port A change callback if port A and callback is registered
    // Note: Always call for Port A writes (even if value unchanged) because VIC-II
    // banking needs to be notified on every PRA write for correct operation
    if (p == A && port_a_change_callback) {
        port_a_change_callback(port_a_callback_context, port_a_value);
    }
}

void mos6526_s::update_output_port_b(uint8_t v) {
    // Compute physical pin output with timer overrides (if PBON enabled)
    // Note: Port B register (reg[PRB]) is already set by the write handler
    
    // Handle PBON bits - these override the pin output, NOT the register value
    // "PBON   1 = TIMER A output appears on PB6.
    //         0 = PB6 normal operation."
    if ((reg[CRA] & CRA_PBON) > 0) { // PB6 output mode:Timer
        if ((reg[CRA] & CRA_OUTMODE) == 0) {
            // Pulse mode: Output HIGH for one cycle on underflow (ICR_TA set)
            // Will be cleared in next cycle by the pulse clear logic in tick()
            uint8_t timer_a_output = ((reg[ICR] & ICR_TA) << 6);
            v = (v & ~PB6_MASK) | timer_a_output;
        } else {
            // Toggle mode: Output the flip-flop state
            // Flip-flop toggles on each underflow and is set HIGH on START
            v = (v & ~PB6_MASK) | (pb67_toggle & PB6_MASK);
        }
    } // else PB6 output mode:Port (use port register bit)

    // "CRB[..]1 controls the output of TIMER B on PB7"
    if ((reg[CRB] & CRB_PBON) > 0) { // PB7 output mode:Timer
        if ((reg[CRB] & CRB_OUTMODE) == 0) {
            // Pulse mode: Output HIGH for one cycle on underflow (ICR_TB set)
            // Will be cleared in next cycle by the pulse clear logic in tick()
            uint8_t timer_b_output = ((reg[ICR] & ICR_TB) << 6);
            v = (v & ~PB7_MASK) | timer_b_output;
        } else {
            // Toggle mode: Output the flip-flop state
            // Flip-flop toggles on each underflow and is set HIGH on START
            v = (v & ~PB7_MASK) | (pb67_toggle & PB7_MASK);
        }
    } // else PB7 output mode:Port (use port register bit)

    // Drive physical pins with timer overrides applied
    // CRITICAL: This affects what external devices see, but NOT what reads return
    update_output_port(B, v);
}

uint8_t mos6526_s::read_port_data(uint32_t p) { // p:A or B
    // Start with current port value (pull-ups HIGH, or driven by output pins)
    uint8_t port_value = (p == A) ? port_a_value : port_b_value;
    // For Port B, use IDDRB which includes PBON-forced outputs
    // For Port A, use regular DDRA
    uint8_t output_mask = (p == A) ? reg[DDRA] : reg[IDDRB_OFFSET];
    
    // For input pins, call the read callback to get external device state
    // External devices (keyboard, joystick) can pull lines LOW
    if (p == A && port_a_read_callback) {
        // Callback receives current port output and returns modified value
        // It can pull any input lines LOW (0) that are pressed
        port_value = port_a_read_callback(port_a_read_context, port_value);
    } else if (p == B && port_b_read_callback) {
        port_value = port_b_read_callback(port_b_read_context, port_value);
    }
    
    // For Port B with PBON-forced output: timer output overrides register value.
    // PBON makes PB6/PB7 show the timer output (pulse or toggle), not reg[PRB].
    // Compute directly from current ICR/toggle state to avoid stale port_b_value.
    if (p == B) {
        uint8_t pbon_mask = 0;
        uint8_t pbon_value = 0;
        if (reg[CRA] & CRA_PBON) {
            pbon_mask |= PB6_MASK;
            if ((reg[CRA] & CRA_OUTMODE) == 0) {
                // Pulse mode: PB6 HIGH for exactly one phi2 cycle on timer A underflow.
                // Use timer_underflowed (one-cycle flag cleared each tick), not ICR
                // (which persists until read).
                if (timer_underflowed & (1 << A))
                    pbon_value |= PB6_MASK;
            } else {
                // Toggle mode: PB6 follows flip-flop
                pbon_value |= (pb67_toggle & PB6_MASK);
            }
        }
        if (reg[CRB] & CRB_PBON) {
            pbon_mask |= PB7_MASK;
            if ((reg[CRB] & CRB_OUTMODE) == 0) {
                // Pulse mode: PB7 HIGH for exactly one phi2 cycle on timer B underflow
                if (timer_underflowed & (1 << B))
                    pbon_value |= PB7_MASK;
            } else {
                // Toggle mode: PB7 follows flip-flop
                pbon_value |= (pb67_toggle & PB7_MASK);
            }
        }
        uint8_t normal_mask = output_mask & ~pbon_mask;  // Regular output bits
        return (port_value & ~output_mask)         // Input bits (from pin/callback)
             | (reg[PRB] & normal_mask)       // Normal output bits (from register)
             | (pbon_value & pbon_mask);            // PBON bits (timer output, live)
    }
    
    // Return combination: input bits from port_value, output bits from register
    return (port_value & ~output_mask) | (reg[PRA + p] & output_mask);
}

void mos6526_s::update_internal_data_direction_port_b(uint8_t port_b_output_mask) {
    // "PB On/Off
    //  A control bit allows the timer output to appear on
    // a PORT B output line (PB6 for TIMER A and PB7
    // for TIMER B). This function overrides the DDRB
    // control bit and forces the appropriate PB line to an
    // output."
    if ((reg[CRA] & CRA_PBON) > 0)
        port_b_output_mask = port_b_output_mask | PB6_MASK;

    // "CRB[..]1 controls the output of TIMER B on PB7"
    if ((reg[CRB] & CRB_PBON) > 0)
        // Override PB7 when CRB has PBON flag set
        port_b_output_mask = port_b_output_mask | PB7_MASK;

    reg[IDDRB_OFFSET] = port_b_output_mask;
}

// TIMER A/B handling

void mos6526_s::reload_timer(uint32_t t) { // t:A or B
    uint32_t i = t * 2; // Turn A or B into TA_LO / TB_LO offsets
    reg[TA_LO + i] = reg[TIMER_OFFSET + TA_LO + i];
    reg[TA_HI + i] = reg[TIMER_OFFSET + TA_HI + i];
    timer_counter_[t] = (reg[TA_HI + i] << 8) | reg[TA_LO + i];
}

void mos6526_s::check_reload_timer(uint32_t t) { // t:A or B
    // " The timer latch is loaded into the timer on any
    // timer underflow, on a force load or following a write
    // to the high byte of the prescaler while the timer is
    // stopped. If the timer is running, a write to the high
    // byte will load the timer latch, but not reload the
    // counter."
    // Check only the specific timer's START bit (generic bit works for both timers)
    if ((reg[CRA + t] & CR_START) == 0)
        reload_timer(t);
}

void mos6526_s::decrease_timer(uint32_t t) { // t:A or B
    uint16_t timer = timer_counter_[t];

    // =========================================================================
    // Pipeline state: MSB (bit 2) = decrement permission, bit 1 = "count active"
    // =========================================================================
    // Per chips_mos6526.hpp reference: two different pipeline positions are used:
    //   pip[0] (our MSB/bit 2) gates the actual decrement
    //   pip[1] (our bit 1) gates the underflow check
    // This separation is critical: underflow fires when counter IS zero AND
    // counting is "becoming active" (pip[1]), even WITHOUT a decrement occurring.
    // This handles the case where counter starts at 0 or is loaded to 0.
    //
    // ALL input mode logic (PHI2, CNT, cascade) is handled in the pipeline
    // injection section of tick_phi2 — this function just reads pipeline state.
    bool can_count = (t == A) ? delay_line.Check(ta_count_pipe) : delay_line.Check(tb_count_pipe);
    uint64_t pipe_bits = (t == A) ? delay_line.Read(ta_count_pipe) : delay_line.Read(tb_count_pipe);
    bool count_active = (pipe_bits >> 1) & 1;  // bit 1 = reference's pip[1]

    // =========================================================================
    // Phase 1: Decrement counter if pipeline says so
    // =========================================================================
    if (can_count) {
        timer--;
        timer_counter_[t] = timer;
    }

    // =========================================================================
    // Phase 2: Underflow detection
    // =========================================================================
    // Per chips_mos6526.hpp: t_out = (0 == counter) && pip[COUNT, 1]
    // Fires when counter IS zero AND counting is active, regardless of
    // whether a decrement just occurred. This handles the case where
    // counter starts at 0 (e.g., latch LO=1 written, counter still 0
    // from init, timer started → underflow fires before first decrement).
    if (timer == 0x0000 && count_active) {
        // Timer underflowed - reload from latch
        timer_underflowed |= 1 << t;
        
        // Set ICR flag, but for Timer B check the "Timer B Bug" first
        if (t == A || !icr_read_this_cycle) {
            reg[ICR] |= (uint8_t)(ICR_TA + t);
        }
        reload_timer(t);
        
        // Clear ONLY bit 1 of count pipeline to create 1-dead-cycle gap after reload.
        // Per chips_mos6526.hpp: _M6526_PIP_CLR(t->pip, M6526_PIP_TIMER_COUNT, 1)
        // Only clears the underflow-gate bit, leaving other pipeline bits intact.
        // This ensures pipeline-driven cascade/CNT modes work correctly.
        uint64_t bits = pipe_bits & ~(uint64_t(1) << 1);  // Clear bit 1 only
        if (t == A)
            delay_line.Inject(ta_count_pipe, bits);
        else
            delay_line.Inject(tb_count_pipe, bits);
        
        // Toggle PB6/PB7 flip-flop on timer underflow
        uint8_t toggle_bit = (t == A) ? PB6_MASK : PB7_MASK;
        pb67_toggle ^= toggle_bit;
        
        // Serial output: Timer A underflow schedules CNT toggle via delay pipeline
        // Per VICE: ~1.5 cycle delay until CNT is toggled after Timer A underflow.
        if (t == A && (reg[CRA] & CRA_SPMODE)) {
            if (sr_bits != 0 || sdr_valid) {
                uint32_t event = CIA_SDR_TOGGLE_CNT1;
                // If timer pulses come too fast, we can't detect the CNT transition.
                // Use NOGGLE (no-toggle) to handle very short timer periods.
                if (sdr_delay & (CIA_SDR_TOGGLE_CNT0 | CIA_SDR_NOGGLE_CNT0)) {
                    event = CIA_SDR_NOGGLE_CNT1;
                }
                sdr_delay |= event;
            }
        }
        
        // RUNMODE: 0 = continuous (keep running), 1 = one-shot (stop after underflow)
        if ((reg[CRA + t] & CR_RUNMODE) != 0) {
            reg[CRA + t] &= ~CR_START;
        }
    }
}

// CONTROL REGISTER (CRA/CRB) handling

void mos6526_s::write_control_register(uint32_t c, uint8_t v) { // c:A or B
    uint8_t old_crx = reg[CRA + c]; // c=B:CRB
    
    // CRA write logging disabled for performance

    if (c == A) {
        // TODO : Should toggling 50/60Hz reset the cycle counter?
        //if (c == A && (old_crv & CRA_TODIN) != (v & CRA_TODIN))
        //    tod_cycles = 0;

        // Detect a change in the Serial Port input/output bit
        if ((old_crx & CRA_SPMODE) != (v & CRA_SPMODE)) {
            // Reset the shift register and serial state
            reg[SHIFT_OFFSET] = 0;
            shifter = 0;
            sr_bits = 0;
            sdr_valid = false;
            sdr_delay = 0;
            cnt_output_state = true;  // CNT returns to idle HIGH
            sp_output_bit = false;
        }
        
        // Phase 7: Detect INMODE change (Timer A: PHI2 ↔ CNT switching)
        // Phase 7: Detect INMODE change (Timer A: PHI2 ↔ CNT switching)
        // Per CIA6526.txt lines 210-215: 2-cycle delay when switching timer input
        if ((old_crx & CRA_INMODE) != (v & CRA_INMODE)) {
            // Timer A input mode changed - inject switching delay
            delay_line.Inject(cnt_switch_a_pipe);
        }
    } else { // c == B
        // "CRB
        //   7   TODIN   1 = writing to TOD registers sets ALARM.
        //               0 = writing to TOD registers sets TOD clock."
        write_tod_delta = ((v & CRB_ALARM) > 0) ? ALARM_OFFSET : 0;
        
        // Phase 7: Detect INMODE change (Timer B: PHI2/CNT/Timer A mode switching)
        // Per CIA6526.txt lines 210-215: 2-cycle delay when switching timer input
        if ((old_crx & CRB_INMODE) != (v & CRB_INMODE)) {
            // Timer B input mode changed - inject switching delay
            delay_line.Inject(cnt_switch_b_pipe);
        }
    }
    
    // Pipeline injection is NOT done here - it's handled in tick_phi1's continuous
    // re-injection loop. Per chips_mos6526.hpp reference: the write handler only
    // updates the control register; the pipeline tick handles signal propagation.
    // This ensures correct startup delay timing.
    //
    // However, we DO need to clear the pipeline when the timer is stopped,
    // to prevent stale signals from causing phantom counts.
    if (c == A) {
        if (!(v & CRA_START)) {
            // Timer stopped - clear injection point of countdown pipeline
            // and clear oneshot signals (matching reference: CLR at injection point)
            delay_line.Inject(ta_count_pipe, false);
            delay_line.Clear(oneshot_a_pipe);
        }
    } else { // c == B
        if (!(v & CRB_START)) {
            // Timer stopped - clear injection point of countdown pipeline
            // and clear oneshot signals
            delay_line.Inject(tb_count_pipe, false);
            delay_line.Clear(oneshot_b_pipe);
        }
    }

    if ((v & CR_LOAD) > 0) {
        // "Force Load
        //  A strobe bit allows the timer latch to be loaded
        // into the timer counter at any time, whether the timer
        // is running or not."
        // Per chips_mos6526.hpp reference: force load goes through LOAD pipeline
        // with a 1-cycle delay (inject at bit 0, fires after next shift → MSB).
        // When the LOAD fires, it also clears the count pipeline, creating a
        // dead cycle that delays the first decrement by 1 cycle.
        if (c == A)
            delay_line.Inject(ta_load_pipe);
        else
            delay_line.Inject(tb_load_pipe);
        
        // "  4    LOAD   1 = FORCE LOAD (this is a STROBE input, there is no data storage, bit 4 will
        //                    always read back a zero and writing a zero has no effect)."
        v &= ~CR_LOAD; // Clear the LOAD strobe bit
    }
    
    // NOTE: Do NOT clear ICR bits when manually stopping a timer.
    // Per CIA6526.txt: "Only reading the ICR will clear it."
    // The ICR bits represent interrupt conditions that occurred and must persist
    // until software explicitly reads the ICR register to acknowledge them.
    // Manually stopping a timer (clearing START bit) does not clear pending interrupts.

    // Set toggle flip-flop HIGH on rising edge of START bit
    // Per CIA6526.txt lines 104-110 and CIA6526.cpp lines 415-420
    if ((v & CR_START) != 0 && (old_crx & CR_START) == 0) {
        // Timer transitions from stopped to started
        uint8_t toggle_bit = (c == A) ? PB6_MASK : PB7_MASK;
        pb67_toggle |= toggle_bit;
        
        // NOTE: Do NOT force-load timer from latch on START rising edge.
        // Per chips_mos6526.hpp reference: writing START=1 just starts counting
        // from the current counter value. The counter is only loaded from latch on:
        //   1. Timer underflow (automatic)
        //   2. Force LOAD bit (bit 4 of CRA/CRB)
        //   3. Writing HI byte while timer is stopped
        
        // "the frequency counter is being reset to 0 when the clock was stopped and is
        // restarted (->hzsync0.prg, hzsync1.prg)"
        tod_cycles = 0;
    }

    reg[CRA + c] = v;

    int old_pbon = old_crx & CR_PBON; // Generic bit works for both timers
    int new_pbon = v & CR_PBON;
    // Detect PBON bit change from high to low:
    if (old_pbon > new_pbon) {
        // Re-initialize this port B bit to 1. This solves $"{VICE_testprogs}CIA/pb6pb7/main.prg",
        // which expects 0x3F to restore to 0xFF once the CRA/CRB PBON bits are cleared.
        port_b_value |= ((c == A) ? PB6_MASK : PB7_MASK);
    }

    if (old_pbon != new_pbon)
        update_internal_data_direction_port_b(reg[DDRB]);
}

// TIME OF DAY (TOD) handling

// "Since a carry from one stage to the next can occur at any time with respect to read
// operation, a latching function is included to keep all Time Of Day information constant
// during a read sequence. All four TOD registers latch on a read of Hours and remain latched
// until after a read of 10ths of seconds. The TOD clock continues to count when the output
// registers are latched. If only one register is to be read, there is no carry problem and
// the register can be read "on the fly", provided that any read of Hours is followed by
// a read of 10ths of seconds to disable the latching."
uint8_t mos6526_s::latch_read_tod_hr() {
    read_tod_delta = CLOCK_OFFSET;
    reg[CLOCK_OFFSET + TOD_10THS] = reg[TOD_10THS];
    reg[CLOCK_OFFSET + TOD_SEC] = reg[TOD_SEC];
    reg[CLOCK_OFFSET + TOD_MIN] = reg[TOD_MIN];
    return reg[CLOCK_OFFSET + TOD_HR] = reg[TOD_HR];
}

uint8_t mos6526_s::unlatch_read_tod_10ths() {
    read_tod_delta = 0;
    return reg[CLOCK_OFFSET + TOD_10THS];
}

uint8_t mos6526_s::write_tod_hr(uint8_t v) {
    // When writing 12 hours (assuming more, too) flips the given AM/PM bit
    if ((v & TOD_HR_MASK) >= 0x12) // Note the BCD encoding!
        v ^= TOD_HR_PM;

    return v;
}

void mos6526_s::check_alarm_interrupt() {
    // Are time of day and alarm time equal?
    // Note, this must be checked BEFORE increasing any TOD register, so that
    // a preceding TOD reset to zero will hit such an alarm (as it should)

    // Per chips_mos6526.hpp lines 503-512: Only trigger interrupt on RISING EDGE
    // This prevents retriggering the alarm interrupt every cycle when alarm condition stays true
    bool alarm_active = (
        reg[TOD_10THS] == reg[ALARM_OFFSET + TOD_10THS] &&
        reg[TOD_SEC] == reg[ALARM_OFFSET + TOD_SEC] &&
        reg[TOD_MIN] == reg[ALARM_OFFSET + TOD_MIN] &&
        reg[TOD_HR] == reg[ALARM_OFFSET + TOD_HR]
    );

    // Only set interrupt flag on rising edge (alarm goes from false to true)
    if (alarm_active && !prev_alarm_state) {
        reg[ICR] |= ICR_ALRM;
    }

    // Store current alarm state for next cycle's edge detection
    prev_alarm_state = alarm_active;
}

uint8_t mos6526_s::bcd_inc(uint32_t r) { // r:TOD_SEC,TOD_MIN or TOD_HR
    uint8_t v = ++reg[r]; // Increment the TOD register
    if ((v & 0x0F) > 9) { // Did low BCD nibble overflow? TODO : Verify; Should this be == 0x0A?
        v += 6; // Carry over to a high nibble increase TODO : Verify; Should this also do & 0xF0?
        reg[r] = v; // Update the TOD register too
    }
    return v; // Return the result, so that caller can immediately check and handle upper-bound
}

void mos6526_s::increase_tod_and_check_alarm() {
    // Count CPU cycles to simulate the power-line frequency input (50/60 Hz)
    if (tod_cycles++ < cycles_tod[(reg[CRA] & CRA_TODIN) >> 7])
        return;

    tod_cycles = 0;

    // The TOD pin receives 50Hz or 60Hz from the power supply.
    // TOD_10THS must increment at 10Hz, so we divide:
    //   50Hz / 5 = 10Hz, or 60Hz / 6 = 10Hz
    // CRA_TODIN selects the expected power frequency (0=60Hz, 1=50Hz)
    int divider = (reg[CRA] & CRA_TODIN) ? 5 : 6;
    if (++tod_tick_counter < divider)
        return;

    tod_tick_counter = 0;

    // Increment TOD_10THS first, THEN check alarm (per VICE behavior)
    if (++reg[TOD_10THS] <= 9) {
        check_alarm_interrupt();
        return;
    }

    reg[TOD_10THS] = 0;
    // Note : Invalid BCD-encoded register values are treated as if they ARE valid;
    // Only when they overflow, does a reset happen which makes them valid BCD again.
    if (bcd_inc(TOD_SEC) <= 0x59) { // Note the BCD encoding!
        check_alarm_interrupt();
        return;
    }

    reg[TOD_SEC] = 0;
    if (bcd_inc(TOD_MIN) <= 0x59) { // Note the BCD encoding!
        check_alarm_interrupt();
        return;
    }

    reg[TOD_MIN] = 0;
    // Hour increments are somewhat special (besides their BCD encoding);
    // 0x11 (11 AM) must not become 0x12 (12 AM) but 0x92 (12 PM)
    // 0x12 (12 AM) must not become 0x91 ( 1 PM) but 0x01 ( 1 AM)
    // 0x91 (11 PM) must not become 0x92 (12 PM) but 0x12 (12 AM)
    // 0x92 (12 PM) must not become 0x01 ( 1 AM) but 0x81 (01 PM)
    // So, after increment, check the masked hours:
    // * when below 12, there's no change
    // * when equal to 12, swap the AM/PM state
    // * when exceeding 12, reset to 1
    uint8_t hr_new = bcd_inc(TOD_HR);
    int hr_HR = hr_new & TOD_HR_MASK;
    if (hr_HR < 0x12) { // Note the BCD encoding!
        check_alarm_interrupt();
        return;
    }

    int hr_PM = hr_new & TOD_HR_PM;
    if (hr_HR == 0x12) // Note the BCD encoding!
        hr_PM ^= TOD_HR_PM;
    else
        hr_HR = 1;

    reg[TOD_HR] = (uint8_t)(hr_PM | hr_HR);
    check_alarm_interrupt();
}

// SERIAL DATA REGISTER (SDR) handling

void mos6526_s::write_serial_data_register(uint8_t v) {
    reg[SDR] = v;
    // In output mode: load data into shift pipeline
    if (reg[CRA] & CRA_SPMODE) {
        if (sr_bits == 0) {
            // Shifter idle: schedule load with ~2 cycle delay
            sdr_delay |= CIA_SDR_SET1;
        } else {
            // Shifter busy: buffer for continuous transmission
            sdr_valid = true;
        }
    }
}

/**
 * Process the SDR delay pipeline — called once per tick.
 * This implements VICE's sdr_alarm logic as a per-cycle pipeline shift.
 * Actions fire when their countdown bit reaches the ...0 position.
 */
void mos6526_s::process_sdr_pipeline() {
    // SET0: Load SDR value into the 16-bit shifter
    if (sdr_delay & CIA_SDR_SET0) {
        if (sr_bits == 0) {
            sr_bits = 16;
            shifter = (uint16_t)reg[SDR] << 1;
        } else if (sr_bits == 1) {
            // Mid-completion: append new byte
            shifter |= reg[SDR];
            sr_bits = 17;
        } else {
            // Shifter busy: mark as buffered
            sdr_valid = true;
        }
    }

    // TOGGLE_CNT0: Toggle CNT and shift data (delayed ~1.5 cycles from Timer A underflow)
    if (sdr_delay & CIA_SDR_TOGGLE_CNT0) {
        if (sr_bits && (--sr_bits & 1)) {
            // Odd sr_bits: data phase — output bit from shifter, CNT goes LOW
            sp_output_bit = (shifter >> 8) & 1;
            cnt_output_state = false;

            if (sr_bits == 1) {
                // Last bit: byte transmission complete
                // Schedule IRQ with 2-cycle delay (per VICE)
                sdr_delay |= CIA_SDR_SET_SDR_IRQ2;
                
                // If another byte is buffered, start continuous transmission
                if (sdr_valid) {
                    shifter |= reg[SDR];
                    sdr_valid = false;
                    sr_bits = 17;
                }
            }
        } else {
            // Even sr_bits (or was 0): clock phase — shift left, CNT goes HIGH
            shifter <<= 1;
            cnt_output_state = true;
        }
    }

    // SET_SDR_IRQ0: Set the SDR interrupt flag (2 cycles after byte complete)
    if (sdr_delay & CIA_SDR_SET_SDR_IRQ0) {
        reg[ICR] |= ICR_SP;
    }

    // Advance the pipeline: shift left, clear overflow bits
    sdr_delay <<= 1;
    sdr_delay &= ~CIA_SDR_CLEAR;

    // Track CNT output state history in the delay word
    if (cnt_output_state) {
        sdr_delay |= CIA_SDR_CNT0;
    }
}

// The CIA 1 registers are repeated each 16 bytes in the area $dc00-$dcff
// The CIA 2 registers are repeated each 16 bytes in the area $dd00-$ddff
bus_state_t mos6526_s::registers_read(void* context, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)context;
    uint16_t full_addr = BUS_GET_ADDR(bus_state);
    uint8_t r = full_addr & CIA_REGS_MASK;
    
    // CIA register read logging disabled for now
    
    switch (r) {
        // Read ports
        case PRA:
            BUS_SET_DATA(bus_state, cia->read_port_data(A));
            break;
        case PRB:
            BUS_SET_DATA(bus_state, cia->read_port_data(B));
            break;
        case DDRA:
            BUS_SET_DATA(bus_state, cia->reg[DDRA]);
            break;
        case DDRB:
            // Note : Assume this always excludes the optional PBON output mask? (If not, use IDDRB!)
            BUS_SET_DATA(bus_state, cia->reg[DDRB]);
            break;
        // Read timers - return current counter value from native 16-bit counter
        case TA_LO:
            BUS_SET_DATA(bus_state, (uint8_t)(cia->timer_counter_[A] & 0xFF));
            break;
        case TA_HI:
            BUS_SET_DATA(bus_state, (uint8_t)(cia->timer_counter_[A] >> 8));
            break;
        case TB_LO:
            BUS_SET_DATA(bus_state, (uint8_t)(cia->timer_counter_[B] & 0xFF));
            break;
        case TB_HI:
            BUS_SET_DATA(bus_state, (uint8_t)(cia->timer_counter_[B] >> 8));
            break;
        // Read TOD registers
        case TOD_10THS: {
            uint8_t tod_value = (cia->read_tod_delta > 0) ? cia->unlatch_read_tod_10ths() : cia->reg[TOD_10THS];
            BUS_SET_DATA(bus_state, tod_value);
            break;
        }
        case TOD_SEC:
            BUS_SET_DATA(bus_state, cia->reg[cia->read_tod_delta + TOD_SEC]);
            break;
        case TOD_MIN:
            BUS_SET_DATA(bus_state, cia->reg[cia->read_tod_delta + TOD_MIN]);
            break;
        case TOD_HR: {
            uint8_t tod_hr_value = (cia->read_tod_delta > 0) ? cia->reg[CLOCK_OFFSET + TOD_HR] : cia->latch_read_tod_hr();
            BUS_SET_DATA(bus_state, tod_hr_value);
            break;
        }
        // Read control registers
        case SDR:
            BUS_SET_DATA(bus_state, cia->reg[SDR]);
            break;
        case ICR: {
            uint8_t icr_value = cia->read_and_clear_interrupt_control_register();
            BUS_SET_DATA(bus_state, icr_value);
            break;
        }
        case CRA: {
            uint8_t cra_value = cia->reg[CRA];
            // Mask out LOAD bit - it always reads as 0 (strobe bit)
            cra_value &= ~CR_LOAD;
            BUS_SET_DATA(bus_state, cra_value);
            break;
        }
        case CRB: {
            uint8_t crb_value = cia->reg[CRB];
            // Mask out LOAD bit - it always reads as 0 (strobe bit)
            crb_value &= ~CR_LOAD;
            BUS_SET_DATA(bus_state, crb_value);
            break;
        }
        default:
            // Unused registers return the last value on the bus (already in BUS_GET_DATA(bus_state))
            // No action needed - BUS_GET_DATA(bus_state) already contains what was on the bus
            break;
    }
    
    // Logging disabled for performance
    
    return bus_state;
}

bus_state_t mos6526_s::registers_write(void* context, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)context;
    uint8_t r = BUS_GET_ADDR(bus_state) & CIA_REGS_MASK;
    uint8_t value = BUS_GET_DATA(bus_state);
    
    // CIA register write logging - disabled for performance
    
    switch (r) {
        // Write ports
        case PRA:
            cia->reg[PRA] = value;
            cia->update_output_port(A, value);
            // Hardware: CIA2 Data Port A bits 0-1 control VIC-II memory bank selection
            // Note: VIC-II will monitor CIA2 writes at $DD00 directly in its tick function
            // This eliminates the need for callbacks and global state
            break;
        case PRB:
            cia->reg[PRB] = value;
            cia->update_output_port_b(value);
            break;
        case DDRA:
            cia->write_data_direction_port(A, value);
            // CRITICAL: ALWAYS trigger callback when DDRA changes
            // write_data_direction_port() already updated port_a_value correctly
            // Don't call update_output_port() as it would overwrite the value!
            if (cia->port_a_change_callback) {
                cia->port_a_change_callback(cia->port_a_callback_context, cia->port_a_value);
            }
            break;
        case DDRB:
            cia->write_data_direction_port(B, value);
            cia->update_internal_data_direction_port_b(value);
            cia->update_output_port_b(cia->reg[PRB]);
            break;
        // Write timer latches
        case TA_LO:
            cia->reg[TIMER_OFFSET + TA_LO] = value;
            // Per CIA6526 datasheet: Writing to timer registers updates LATCH only
            // Counter is only updated on: underflow, LOAD bit, or HI byte write while stopped
            break;
        case TA_HI:
            cia->reg[TIMER_OFFSET + TA_HI] = value;
            // Writing to HI byte while stopped triggers reload (latch → counter)
            cia->check_reload_timer(A);
            break;
        case TB_LO:
            cia->reg[TIMER_OFFSET + TB_LO] = value;
            // Per CIA6526 datasheet: Writing to timer registers updates LATCH only
            break;
        case TB_HI:
            cia->reg[TIMER_OFFSET + TB_HI] = value;
            // Writing to HI byte while stopped triggers reload (latch → counter)
            cia->check_reload_timer(B);
            break;
        // Write TOD registers / ALARM latches
        case TOD_10THS:
            cia->reg[cia->write_tod_delta + TOD_10THS] = value;
            // Only start TOD clock when writing to TOD registers (not alarm)
            if (cia->write_tod_delta == 0) {
                cia->tod_tick_counter = 0;
                cia->is_running_tod = true;
            }
            cia->check_alarm_interrupt();
            break;
        case TOD_SEC:
            cia->reg[cia->write_tod_delta + TOD_SEC] = value;
            break;
        case TOD_MIN:
            cia->reg[cia->write_tod_delta + TOD_MIN] = value;
            break;
        case TOD_HR:
            cia->reg[cia->write_tod_delta + TOD_HR] = cia->write_tod_hr(value);
            // Only stop TOD clock when writing to TOD registers (not alarm)
            if (cia->write_tod_delta == 0) {
                cia->is_running_tod = false;
            }
            break;
        // Write control registers
        case SDR:
            cia->write_serial_data_register(value);
            break;
        case ICR:
            cia->write_interrupt_control_register(value);
            break;
        case CRA:
            cia->write_control_register(A, value);
            break;
        case CRB:
            cia->write_control_register(B, value);
            break;
    }
    return bus_state;
}

/**
 * Main CIA tick function - handles all CIA cycle processing including timers,
 * TOD clock, serial I/O, and interrupt generation with 1-cycle delay.
 *
 * @param chip Pointer to CIA chip instance
 * @param bus_state Current bus state
 * @return Updated bus state with interrupt lines updated
 */
/**
 * CIA tick PHI2 phase - called BEFORE CPU PHI2.
 *
 * Per chips_mos6526.hpp reference: within one CIA cycle, the order is:
 *   1. Timer processing (decrement counters, detect underflows)
 *   2. Interrupt generation (set ICR flags, update IRQ line)
 *   3. Pipeline tick (shift delay lines, re-inject signals)
 *   4. Register reads/writes (CPU sees post-decrement values)
 *
 * By running timer decrements BEFORE CPU register reads, the CPU sees
 * post-decrement timer values, matching real CIA hardware behavior.
 * Interrupt assertion uses pending_bus_lines for 1-cycle delay.
 */
bus_state_t mos6526_s::tick_phi2(bus_state_t bus_state) {
    
    // =========================================================================
    // INTERRUPT LINE ASSERTION (from PREVIOUS cycle's pending)
    // =========================================================================
    // 1-cycle delay: ICR_IRQ set at end of previous cycle → assert now
    if (pending_bus_lines) {
        bus_state &= ~pending_bus_lines;
    }

    // When PB6 and PB7 should pulse, clear them (the chance for a read was in previous cycle)
    uint8_t port_b_pulse_clear_mask = 0xFF;
    if ((reg[CRA] & CRA_PBON) > 0)
        if ((reg[CRA] & CRA_OUTMODE) == 0)
            port_b_pulse_clear_mask &= ~PB6_MASK;

    if ((reg[CRB] & CRB_PBON) > 0)
        if ((reg[CRB] & CRB_OUTMODE) == 0)
            port_b_pulse_clear_mask &= ~PB7_MASK;

    if (port_b_pulse_clear_mask != 0xFF)
        port_b_value &= port_b_pulse_clear_mask;

    // =========================================================================
    // Clear one-cycle underflow pulse flags from previous cycle
    // =========================================================================
    timer_underflowed = 0;

    // =========================================================================
    // PIN STATE AND EDGE DETECTION
    // =========================================================================
    bool cnt_pin = BUS_GET_BIT(bus_state, BUS_CNT_BIT);
    bool flag_pin = BUS_GET_BIT(bus_state, BUS_FLAG_BIT);
    
    bool prev_cnt = BUS_GET_BIT(bus_snapshot_, BUS_CNT_BIT);
    bool prev_flag = BUS_GET_BIT(bus_snapshot_, BUS_FLAG_BIT);
    
    bool cnt_is_positive_edge = (cnt_pin && !prev_cnt);
    bool cnt_is_negative_edge = (!cnt_pin && prev_cnt);
    bool flag_is_negative_edge = (!flag_pin && prev_flag);

    // =========================================================================
    // TIMER COUNTDOWN (before pipeline tick, so timer sees PREVIOUS cycle's pipeline)
    // =========================================================================
    // Per chips_mos6526.hpp reference order: Timer → Interrupt → Pipeline.
    // Timer processing uses the pipeline state from the PREVIOUS cycle.
    // This gives a 3-cycle delay from START write to first decrement:
    //   Cycle N: Register write sets START
    //   Cycle N+1: Pipeline injects+shifts (but timer already ran with old empty pip)
    //   Cycle N+2: Timer checks pip MSB → not yet set (signal at middle bit)
    //   Cycle N+3: Timer checks pip MSB → set! First decrement.
    // Timer A must be processed first so Timer B cascade mode can see the underflow.
    // ALWAYS process timers regardless of START state — the pipeline gates counting.
    // In-flight pipeline bits must drain naturally after STOP, producing 1-2 more
    // decrements (matching real hardware behavior tested by cia4).
    decrease_timer(A);
    decrease_timer(B);

    // =========================================================================
    // LOAD PIPELINE CHECK (from force load or HI byte write while stopped)
    // =========================================================================
    // Per chips_mos6526.hpp: TIMER_LOAD[0] → counter = latch, CLR COUNT[1]
    // Runs regardless of timer state (force load works even when timer is stopped).
    // Must run AFTER decrease_timer so underflow reload happens first.
    // Clear only bit 1 of count pipeline (matching reference), not all bits.
    if (delay_line.Check(ta_load_pipe)) {
        reload_timer(A);
        uint64_t bits = delay_line.Read(ta_count_pipe);
        bits &= ~(uint64_t(1) << 1);  // Clear bit 1 only
        delay_line.Inject(ta_count_pipe, bits);
    }
    if (delay_line.Check(tb_load_pipe)) {
        reload_timer(B);
        uint64_t bits = delay_line.Read(tb_count_pipe);
        bits &= ~(uint64_t(1) << 1);  // Clear bit 1 only
        delay_line.Inject(tb_count_pipe, bits);
    }

    if (is_running_tod)
        increase_tod_and_check_alarm();

    // Serial I/O:
    // - Output mode (SPMODE=1): Handled by SDR delay pipeline (process_sdr_pipeline).
    //   Timer A underflows schedule delayed CNT toggles; pipeline processes them each tick.
    // - Input mode (SPMODE=0): External CNT drives the shift register clock.
    //   Per CIA datasheet: "data on the SP pin is shifted into the shift register
    //   on the rising edge of the signal applied to the CNT pin."
    if ((reg[CRA] & CRA_SPMODE) == 0) {
        // Input mode: handle CNT edges for serial input
        // Falling edge starts a new byte (per VICE ciacore_set_cnt)
        if (cnt_is_negative_edge && sr_bits == 0) {
            sr_bits = 16;
        }
        if (sr_bits > 0) {
            sr_bits--;
        }
        // Rising edge: shift data in, sample SP
        if (cnt_is_positive_edge) {
            bool sp_bit = BUS_GET_BIT(bus_state, BUS_SP_BIT);
            shifter = (shifter << 1) | (sp_bit ? 1 : 0);
            
            if (sr_bits == 0) {
                // Byte complete: dump into SDR and generate interrupt
                reg[SDR] = shifter & 0xFF;
                reg[ICR] |= ICR_SP;
            }
        }
    }
    
    // Process SDR output delay pipeline (runs every tick)
    if (reg[CRA] & CRA_SPMODE) {
        process_sdr_pipeline();
    }
    
    // Drive CIA serial output pins onto bus for external visibility (user port)
    if (reg[CRA] & CRA_SPMODE) {
        // SPMODE=output: CIA drives CNT and SP pins
        if (!cnt_output_state) {
            BUS_CLR_BIT(bus_state, BUS_CNT_BIT);  // Pull CNT LOW
        }
        if (sp_output_bit) {
            BUS_SET_BIT(bus_state, BUS_SP_BIT);     // Drive SP HIGH
        } else {
            BUS_CLR_BIT(bus_state, BUS_SP_BIT);    // Drive SP LOW
        }
    }

    // FLAG negative edge detection
    if (flag_is_negative_edge) {
        reg[ICR] |= ICR_FLG;
    }

    // =========================================================================
    // INTERRUPT PROCESSING
    // =========================================================================
    check_interrupt_mask();
    
    // Update pending_bus_lines for NEXT cycle's assertion
    if (reg[ICR] & ICR_IRQ) {
        pending_bus_lines = BUS_BIT(configured_interrupt_bit);
    } else {
        pending_bus_lines = 0;
    }

    // =========================================================================
    // DELAY LINE (Pipeline tick AFTER timer - matches reference order)
    // =========================================================================
    // Per chips_mos6526.hpp: _m6526_tick_pipeline
    // Pipeline injection is INMODE-aware. The timer function itself is mode-
    // agnostic — it just checks pipeline state for decrement/underflow.
    // ALL counting mode logic lives HERE in the pipeline injection.
    
    // Timer A counter pipeline
    // Timer A INMODE: bit 5 of CRA (0=PHI2, 1=CNT)
    bool ta_active = false;
    if ((reg[CRA] & CRA_INMODE) == 0) {
        ta_active = true;  // PHI2 mode: always active
    } else {
        ta_active = cnt_is_positive_edge;  // CNT mode: active on positive edge
    }
    if (ta_active && (reg[CRA] & CRA_START)) {
        delay_line.Inject(ta_count_pipe);
    } else {
        // Clear injection point only (LSB), matching reference CLR(pip, COUNT, 2)
        // Lets in-flight pipeline bits drain naturally
        delay_line.Inject(ta_count_pipe, false);
    }
    
    // Timer B counter pipeline
    // Timer B INMODE: bits 5-6 of CRB (00=PHI2, 01=CNT, 10=Timer A, 11=Timer A+CNT)
    bool tb_active = false;
    uint8_t crb_inmode = reg[CRB] & CRB_INMODE;
    switch (crb_inmode) {
        case 0x00:  // PHI2
            tb_active = true;
            break;
        case 0x20:  // CNT
            tb_active = cnt_is_positive_edge;
            break;
        case 0x40:  // Timer A cascade
            tb_active = (timer_underflowed & (1 << A)) != 0;
            break;
        case 0x60:  // Timer A + CNT
            tb_active = ((timer_underflowed & (1 << A)) != 0) && cnt_pin;
            break;
    }
    if (tb_active && (reg[CRB] & CRB_START)) {
        delay_line.Inject(tb_count_pipe);
    } else {
        // Clear injection point only (LSB), matching reference CLR(pip, COUNT, 2)
        delay_line.Inject(tb_count_pipe, false);
    }
    
    // Inject one-shot mode state each cycle (only while timer is running)
    if ((reg[CRA] & (CR_START | CR_RUNMODE)) == (CR_START | CR_RUNMODE)) {
        delay_line.Feed(oneshot_a_pipe);
    }
    if ((reg[CRB] & (CR_START | CR_RUNMODE)) == (CR_START | CR_RUNMODE)) {
        delay_line.Feed(oneshot_b_pipe);
    }
    
    // Shift all pipeline signals towards MSB (output position)
    delay_line.Shift();

    return bus_state;
}

/**
 * CIA tick PHI1 phase - called AFTER CPU PHI2 and memory service.
 * Handles delayed mask transfer and cleanup. Timer counting has already
 * happened in phi2, so register reads during memory service see post-decrement values.
 */
bus_state_t mos6526_s::tick_phi1(bus_state_t bus_state) {
    
    // Transfer delayed mask to active mask at end of cycle
    interrupt_mask = interrupt_mask_delayed;
    
    // Clear Timer B Bug flag for next cycle
    icr_read_this_cycle = false;
    
    // Store final bus state — serves as previous-cycle reference for
    // edge detection (CNT/FLAG) AND as GUI layout rendering snapshot.
    bus_snapshot_ = bus_state;
    
    return bus_state;
}

/**
 * Single-phase CIA tick (calls phi2 + phi1 in sequence).
 */
bus_state_t mos6526_s::tick(bus_state_t bus_state) {
    bus_state = tick_phi2(bus_state);
    bus_state = tick_phi1(bus_state);
    return bus_state;
}
