#include "mos6526.h" // cia
#include "../../core/system_lines.h"
#include <string.h>
#include <stdlib.h>
#include <cstdio>

// Forward declaration for serial I/O (called from decrease_timer before definition)
void mos6526_serial_output(mos6526_t* cia);

void mos6526_reset(mos6526_t* cia) {
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
    memset(cia->reg, 0, sizeof(cia->reg));
    cia->reg[TOD_HR] = 1; // According to powerup
    // Ports all high
    // "The lines PA0 and PA1 of the second CIA are the inverse of the
    // virtual VIC-II address lines VA14 and VA15, respectively."
    // So below writes result in VICBase to become $C000
    cia->port_a_value = 0xFF;
    cia->port_b_value = 0xFF;
    // Timer latches all ones
    cia->reg[TIMER_OFFSET + TA_LO] = 0xFF;
    cia->reg[TIMER_OFFSET + TA_HI] = 0xFF;
    cia->reg[TIMER_OFFSET + TB_LO] = 0xFF;
    cia->reg[TIMER_OFFSET + TB_HI] = 0xFF;
    // Timer counters are also set to all ones after reset
    // (loaded from latches since control registers are 0 = stopped)
    cia->reg[TA_LO] = 0xFF;
    cia->reg[TA_HI] = 0xFF;
    cia->reg[TB_LO] = 0xFF;
    cia->reg[TB_HI] = 0xFF;
    // Also reset implementation-related variables
    cia->read_tod_delta = 0;
    cia->write_tod_delta = 0;
    cia->is_running_tod = false;
    cia->tod_cycles = 0;
    cia->interrupt_mask = 0;
    cia->interrupt_mask_delayed = 0;  // IMR delay (chips imr1)
    cia->pending_bus_lines = 0;  // No pending interrupt assertions
    cia->prev_alarm_state = false;  // No alarm initially
    cia->icr_read_this_cycle = false;  // Timer B Bug state
    cia->timer_underflowed = 0;  // No underflow events initially
    
    // Initialize delay line (multi-cycle signal propagation)
    cia->delay_line.Clear();
    
    // Initialize PB6/PB7 toggle flip-flops (cleared on reset per CIA6526.txt line 107)
    cia->pb67_toggle = 0;
    
    // Initialize serial output state
    // CNT idles HIGH when no transmission is active
    cia->cnt_output_state = true;
    cia->sp_output_bit = false;
    cia->serial_shift = 0;
    
    // Initialize previous bus state for edge detection
    // CNT and FLAG pins have internal pull-ups, so they start HIGH
    cia->prev_bus_state = BUS_BIT(BUS_CNT_BIT) | BUS_BIT(BUS_FLAG_BIT);
    
    // Call port A change callback with initial value (all high due to pull-ups)
    if (cia->port_a_change_callback) {
        cia->port_a_change_callback(cia->port_a_callback_context, cia->port_a_value);
    }
}

mos6526_t* mos6526_create() {
    mos6526_t* cia = new mos6526_t();
    cia->configured_interrupt_bit = BUS_IRQ_BIT; // Default to IRQ; caller must set to NMI for CIA2
    // Constructor equivalent - set up cycles for TOD
    // Used when CRA_TODIN = 0 (60 Hz TOD pin input pulses)
    cia->cycles_tod[0] = 1000000 / 60; // Assuming 1MHz CPU clock
    // Used when CRA_TODIN = 1 (50 Hz TOD pin input pulses)
    cia->cycles_tod[1] = 1000000 / 50;
    
    mos6526_reset(cia);
    return cia;
}

void mos6526_destroy(mos6526_t* cia) {
    delete cia;
}

// ChipBase identity
ChipIdentity mos6526_s::chip_identity() const {
    return {"MOS6526", "MOS Technology"};
}

bool mos6526_s::has_debug_content()    const { return true; }
bool mos6526_s::has_settings_content() const { return true; }
bool mos6526_s::has_layout_content()   const { return true; }

// INTERRUPT CONTROL REGISTER (ICR) handling

uint8_t mos6526_read_and_clear_interrupt_control_register(mos6526_t* cia) {
    // "The interrupt DATA register is cleared" (the /IRQ line
    // does NOT return high following a read of the DATA register!)
    uint8_t v = cia->reg[ICR];
    
    // "interrupt can be prevented by reading the ICR at the time of the underflow."
    cia->reg[ICR] = 0;
    
    // Timer B Bug: Remember ICR reads to block Timer B interrupts this cycle
    // Per chips_mos6526.hpp lines 476-477: Set flag that will be checked in interrupt handling
    cia->icr_read_this_cycle = true;
    
    // NOTE: With the new pull-up resistor model, we don't need to manage delayed_irq.
    // The interrupt line will be released automatically in the next cycle when
    // mos6526_tick() sees that ICR_IRQ is clear and doesn't assert the line.
    // The system tick will pull the line HIGH via pull-up resistors.
    return v;
}

void mos6526_check_interrupt_mask(mos6526_t* cia) {
    // "In order for an interrupt flag to set IR
    // and generate an Interrupt Request, the
    // corresponding MASK bit must be set."
    uint8_t masked_interrupts = cia->reg[ICR] & cia->interrupt_mask;
    
    if (masked_interrupts > 0) {
        // "Any interrupt which is enabled by the MASK register will
        // set the IR bit (MSB) of the DATA register and bring
        // the /IRQ pin low."
        //
        // With the new pull-up resistor model, we simply set ICR_IRQ.
        // The next mos6526_tick() will see this and assert the interrupt line.
        // No need for delayed_irq flag - the interrupt persists until ICR is read.
        cia->reg[ICR] |= ICR_IRQ;
    }
    // Per CIA datasheet: "Clearing the bit in the IMR may not clear the interrupt."
    // ICR_IRQ is ONLY cleared by reading the ICR register (mos6526_read_and_clear_interrupt_control_register).
    // Once the interrupt flip-flop has been set, changing the IMR has no effect on ICR_IRQ.
}

void mos6526_write_interrupt_control_register(mos6526_t* cia, uint32_t v) {
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
        cia->interrupt_mask_delayed &= ~bits;
    else
        // 1 = set bits 0..4 are setting the according mask bit."
        cia->interrupt_mask_delayed |= bits;

    // "When a condition in the ICR is true, setting the corresponding bit in the IMR must also set the interrupt."
    // "Clearing the bit in the IMR may not clear the interrupt."
    // IMPORTANT: We still need to call mos6526_check_interrupt_mask() because it handles
    // both setting ICR_IRQ when masked interrupts exist AND clearing it when they don't.
    // The chips version uses immediate check with delayed mask (lines 461-463), but we
    // need the full clearing logic that our check_interrupt_mask provides.
    // However, we temporarily copy delayed mask to active mask for this check.
    uint8_t saved_mask = cia->interrupt_mask;
    cia->interrupt_mask = cia->interrupt_mask_delayed;  // Temporarily use delayed mask
    mos6526_check_interrupt_mask(cia);
    cia->interrupt_mask = saved_mask;  // Restore for end-of-cycle update
    // "Once the interrupt flip-flop has been set, changing the condition in the IMR has no effect."
}

// PORT/PERIPHERAL DATA / DATA DIRECTION handling

void mos6526_write_data_direction_port(mos6526_t* cia, uint32_t p, uint8_t v) { // p:A or B
    // Access either DDRA or DDRB
    uint32_t i = DDRA + p;
    // First fetch the existing output data direction value
    // (so it can be compared) and then store the new value.
    uint8_t old_outputs = cia->reg[i]; // DDRA / DDRB
    cia->reg[i] = v; // DDRA / DDRB
    
    uint8_t* port = (p == A) ? &cia->port_a_value : &cia->port_b_value;
    uint8_t pra_value = cia->reg[PRA + p];
    
    // Determine which port bit lines have changed from output to input.
    uint8_t new_inputs = old_outputs & ~v;
    if (new_inputs > 0) {
        // 'Pull up' all port bit lines that changed from output to input.
        *port |= new_inputs;
    }
    
    // Determine which port bit lines have changed from input to output.
    uint8_t new_outputs = ~old_outputs & v;
    if (new_outputs > 0) {
        // Drive PRA/PRB value onto bits that changed from input to output
        *port = (*port & ~new_outputs) | (pra_value & new_outputs);
    }
    
    // Note : Pull-up bits can be lowered by connected devices (keyboard, joystick, mouse)
    // when that happens AFTER the CIA cycle update!
}

void mos6526_update_output_port(mos6526_t* cia, uint32_t p, uint8_t v) { // p:A or B
    // Update only the port pins that are set to output
    uint8_t* port = (p == A) ? &cia->port_a_value : &cia->port_b_value;
    uint8_t mask = cia->reg[(p == A) ? DDRA : IDDRB_OFFSET];
    // Note : For port B, IDDRB is DDRB but with PBON taken into account - see UpdateInternalDataDirectionPortB()
    *port = (*port & ~mask) | (v & mask);
    
    // Call port A change callback if port A and callback is registered
    // Note: Always call for Port A writes (even if value unchanged) because VIC-II
    // banking needs to be notified on every PRA write for correct operation
    if (p == A && cia->port_a_change_callback) {
        cia->port_a_change_callback(cia->port_a_callback_context, *port);
    }
}

void mos6526_update_output_port_b(mos6526_t* cia, uint8_t v) {
    // Compute physical pin output with timer overrides (if PBON enabled)
    // Note: Port B register (cia->reg[PRB]) is already set by the write handler
    
    // Handle PBON bits - these override the pin output, NOT the register value
    // "PBON   1 = TIMER A output appears on PB6.
    //         0 = PB6 normal operation."
    if ((cia->reg[CRA] & CRA_PBON) > 0) { // PB6 output mode:Timer
        if ((cia->reg[CRA] & CRA_OUTMODE) == 0) {
            // Pulse mode: Output HIGH for one cycle on underflow (ICR_TA set)
            // Will be cleared in next cycle by the pulse clear logic in mos6526_tick()
            uint8_t timer_a_output = ((cia->reg[ICR] & ICR_TA) << 6);
            v = (v & ~PB6_MASK) | timer_a_output;
        } else {
            // Toggle mode: Output the flip-flop state
            // Flip-flop toggles on each underflow and is set HIGH on START
            v = (v & ~PB6_MASK) | (cia->pb67_toggle & PB6_MASK);
        }
    } // else PB6 output mode:Port (use port register bit)

    // "CRB[..]1 controls the output of TIMER B on PB7"
    if ((cia->reg[CRB] & CRB_PBON) > 0) { // PB7 output mode:Timer
        if ((cia->reg[CRB] & CRB_OUTMODE) == 0) {
            // Pulse mode: Output HIGH for one cycle on underflow (ICR_TB set)
            // Will be cleared in next cycle by the pulse clear logic in mos6526_tick()
            uint8_t timer_b_output = ((cia->reg[ICR] & ICR_TB) << 6);
            v = (v & ~PB7_MASK) | timer_b_output;
        } else {
            // Toggle mode: Output the flip-flop state
            // Flip-flop toggles on each underflow and is set HIGH on START
            v = (v & ~PB7_MASK) | (cia->pb67_toggle & PB7_MASK);
        }
    } // else PB7 output mode:Port (use port register bit)

    // Drive physical pins with timer overrides applied
    // CRITICAL: This affects what external devices see, but NOT what reads return
    mos6526_update_output_port(cia, B, v);
}

uint8_t mos6526_read_port_data(mos6526_t* cia, uint32_t p) { // p:A or B
    // Start with current port value (pull-ups HIGH, or driven by output pins)
    uint8_t port_value = (p == A) ? cia->port_a_value : cia->port_b_value;
    // For Port B, use IDDRB which includes PBON-forced outputs
    // For Port A, use regular DDRA
    uint8_t output_mask = (p == A) ? cia->reg[DDRA] : cia->reg[IDDRB_OFFSET];
    
    // For input pins, call the read callback to get external device state
    // External devices (keyboard, joystick) can pull lines LOW
    if (p == A && cia->port_a_read_callback) {
        // Callback receives current port output and returns modified value
        // It can pull any input lines LOW (0) that are pressed
        port_value = cia->port_a_read_callback(cia->port_a_read_context, port_value);
    } else if (p == B && cia->port_b_read_callback) {
        port_value = cia->port_b_read_callback(cia->port_b_read_context, port_value);
    }
    
    // For Port B with PBON-forced output: timer output overrides register value.
    // PBON makes PB6/PB7 show the timer output (pulse or toggle), not reg[PRB].
    // Compute directly from current ICR/toggle state to avoid stale port_b_value.
    if (p == B) {
        uint8_t pbon_mask = 0;
        uint8_t pbon_value = 0;
        if (cia->reg[CRA] & CRA_PBON) {
            pbon_mask |= PB6_MASK;
            if ((cia->reg[CRA] & CRA_OUTMODE) == 0) {
                // Pulse mode: PB6 HIGH for exactly one phi2 cycle on timer A underflow.
                // Use timer_underflowed (one-cycle flag cleared each tick), not ICR
                // (which persists until read).
                if (cia->timer_underflowed & (1 << A))
                    pbon_value |= PB6_MASK;
            } else {
                // Toggle mode: PB6 follows flip-flop
                pbon_value |= (cia->pb67_toggle & PB6_MASK);
            }
        }
        if (cia->reg[CRB] & CRB_PBON) {
            pbon_mask |= PB7_MASK;
            if ((cia->reg[CRB] & CRB_OUTMODE) == 0) {
                // Pulse mode: PB7 HIGH for exactly one phi2 cycle on timer B underflow
                if (cia->timer_underflowed & (1 << B))
                    pbon_value |= PB7_MASK;
            } else {
                // Toggle mode: PB7 follows flip-flop
                pbon_value |= (cia->pb67_toggle & PB7_MASK);
            }
        }
        uint8_t normal_mask = output_mask & ~pbon_mask;  // Regular output bits
        return (port_value & ~output_mask)         // Input bits (from pin/callback)
             | (cia->reg[PRB] & normal_mask)       // Normal output bits (from register)
             | (pbon_value & pbon_mask);            // PBON bits (timer output, live)
    }
    
    // Return combination: input bits from port_value, output bits from register
    return (port_value & ~output_mask) | (cia->reg[PRA + p] & output_mask);
}

void mos6526_update_internal_data_direction_port_b(mos6526_t* cia, uint8_t port_b_output_mask) {
    // "PB On/Off
    //  A control bit allows the timer output to appear on
    // a PORT B output line (PB6 for TIMER A and PB7
    // for TIMER B). This function overrides the DDRB
    // control bit and forces the appropriate PB line to an
    // output."
    if ((cia->reg[CRA] & CRA_PBON) > 0)
        port_b_output_mask = port_b_output_mask | PB6_MASK;

    // "CRB[..]1 controls the output of TIMER B on PB7"
    if ((cia->reg[CRB] & CRB_PBON) > 0)
        // Override PB7 when CRB has PBON flag set
        port_b_output_mask = port_b_output_mask | PB7_MASK;

    cia->reg[IDDRB_OFFSET] = port_b_output_mask;
}

// TIMER A/B handling

void mos6526_reload_timer(mos6526_t* cia, uint32_t t) { // t:A or B
    uint32_t i = t * 2; // Turn A or B into TA_LO / TB_LO offsets
    cia->reg[TA_LO + i] = cia->reg[TIMER_OFFSET + TA_LO + i];
    cia->reg[TA_HI + i] = cia->reg[TIMER_OFFSET + TA_HI + i];
}

void mos6526_check_reload_timer(mos6526_t* cia, uint32_t t) { // t:A or B
    // " The timer latch is loaded into the timer on any
    // timer underflow, on a force load or following a write
    // to the high byte of the prescaler while the timer is
    // stopped. If the timer is running, a write to the high
    // byte will load the timer latch, but not reload the
    // counter."
    // Check only the specific timer's START bit (generic bit works for both timers)
    if ((cia->reg[CRA + t] & CR_START) == 0)
        mos6526_reload_timer(cia, t);
}

void mos6526_decrease_timer(mos6526_t* cia, uint32_t t) { // t:A or B
    uint32_t i = t * 2; // Turn A or B into TA_LO / TB_LO offsets
    uint16_t timer = (cia->reg[TA_HI + i] << 8) | cia->reg[TA_LO + i];

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
    bool can_count = (t == A) ? cia->delay_line.Check(ta_count_pipe) : cia->delay_line.Check(tb_count_pipe);
    uint64_t pipe_bits = (t == A) ? cia->delay_line.Read(ta_count_pipe) : cia->delay_line.Read(tb_count_pipe);
    bool count_active = (pipe_bits >> 1) & 1;  // bit 1 = reference's pip[1]

    // =========================================================================
    // Phase 1: Decrement counter if pipeline says so
    // =========================================================================
    if (can_count) {
        timer--;
        cia->reg[TA_LO + i] = (uint8_t)(timer & 0xFF);
        cia->reg[TA_HI + i] = (uint8_t)(timer >> 8);
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
        cia->timer_underflowed |= 1 << t;
        
        // Set ICR flag, but for Timer B check the "Timer B Bug" first
        if (t == A || !cia->icr_read_this_cycle) {
            cia->reg[ICR] |= (uint8_t)(ICR_TA + t);
        }
        mos6526_reload_timer(cia, t);
        
        // Clear ONLY bit 1 of count pipeline to create 1-dead-cycle gap after reload.
        // Per chips_mos6526.hpp: _M6526_PIP_CLR(t->pip, M6526_PIP_TIMER_COUNT, 1)
        // Only clears the underflow-gate bit, leaving other pipeline bits intact.
        // This ensures pipeline-driven cascade/CNT modes work correctly.
        uint64_t bits = pipe_bits & ~(uint64_t(1) << 1);  // Clear bit 1 only
        if (t == A)
            cia->delay_line.Inject(ta_count_pipe, bits);
        else
            cia->delay_line.Inject(tb_count_pipe, bits);
        
        // Toggle PB6/PB7 flip-flop on timer underflow
        uint8_t toggle_bit = (t == A) ? PB6_MASK : PB7_MASK;
        cia->pb67_toggle ^= toggle_bit;
        
        // Serial output: Timer A underflow toggles CNT in SPMODE=output
        // Per CIA datasheet: "In the output mode, TIMER A is used for the baud
        // rate generator. Data is shifted out on the SP pin at 1/2 the underflow
        // rate of TIMER A." Each underflow toggles CNT; falling edge shifts a bit.
        if (t == A && (cia->reg[CRA] & CRA_SPMODE)) {
            if (cia->serial_shift > 0) {
                // Toggle CNT flip-flop
                bool was_high = cia->cnt_output_state;
                cia->cnt_output_state = !cia->cnt_output_state;
                // Falling edge (HIGH→LOW) clocks the shift register
                if (was_high) {
                    mos6526_serial_output(cia);
                }
            } else {
                // "If no further data is to be transmitted, after the 8th CNT
                // pulse, CNT will return high and SP will remain at the level
                // of the last data bit transmitted."
                cia->cnt_output_state = true;
            }
        }
        
        // RUNMODE: 0 = continuous (keep running), 1 = one-shot (stop after underflow)
        if ((cia->reg[CRA + t] & CR_RUNMODE) != 0) {
            cia->reg[CRA + t] &= ~CR_START;
        }
    }
}

// CONTROL REGISTER (CRA/CRB) handling

void mos6526_write_control_register(mos6526_t* cia, uint32_t c, uint8_t v) { // c:A or B
    uint8_t old_crx = cia->reg[CRA + c]; // c=B:CRB
    
    // CRA write logging disabled for performance

    if (c == A) {
        // TODO : Should toggling 50/60Hz reset the cycle counter?
        //if (c == A && (old_crv & CRA_TODIN) != (v & CRA_TODIN))
        //    cia->tod_cycles = 0;

        // Detect a change in the Serial Port input/output bit
        if ((old_crx & CRA_SPMODE) != (v & CRA_SPMODE)) {
            // Reset the shift register and serial state
            cia->reg[SHIFT_OFFSET] = 0;
            cia->serial_shift = 0;
            cia->cnt_output_state = true;  // CNT returns to idle HIGH
            cia->sp_output_bit = false;
        }
        
        // Phase 7: Detect INMODE change (Timer A: PHI2 ↔ CNT switching)
        // Phase 7: Detect INMODE change (Timer A: PHI2 ↔ CNT switching)
        // Per CIA6526.txt lines 210-215: 2-cycle delay when switching timer input
        if ((old_crx & CRA_INMODE) != (v & CRA_INMODE)) {
            // Timer A input mode changed - inject switching delay
            cia->delay_line.Inject(cnt_switch_a_pipe);
        }
    } else { // c == B
        // "CRB
        //   7   TODIN   1 = writing to TOD registers sets ALARM.
        //               0 = writing to TOD registers sets TOD clock."
        cia->write_tod_delta = ((v & CRB_ALARM) > 0) ? ALARM_OFFSET : 0;
        
        // Phase 7: Detect INMODE change (Timer B: PHI2/CNT/Timer A mode switching)
        // Per CIA6526.txt lines 210-215: 2-cycle delay when switching timer input
        if ((old_crx & CRB_INMODE) != (v & CRB_INMODE)) {
            // Timer B input mode changed - inject switching delay
            cia->delay_line.Inject(cnt_switch_b_pipe);
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
            cia->delay_line.Inject(ta_count_pipe, false);
            cia->delay_line.Clear(oneshot_a_pipe);
        }
    } else { // c == B
        if (!(v & CRB_START)) {
            // Timer stopped - clear injection point of countdown pipeline
            // and clear oneshot signals
            cia->delay_line.Inject(tb_count_pipe, false);
            cia->delay_line.Clear(oneshot_b_pipe);
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
            cia->delay_line.Inject(ta_load_pipe);
        else
            cia->delay_line.Inject(tb_load_pipe);
        
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
        cia->pb67_toggle |= toggle_bit;
        
        // NOTE: Do NOT force-load timer from latch on START rising edge.
        // Per chips_mos6526.hpp reference: writing START=1 just starts counting
        // from the current counter value. The counter is only loaded from latch on:
        //   1. Timer underflow (automatic)
        //   2. Force LOAD bit (bit 4 of CRA/CRB)
        //   3. Writing HI byte while timer is stopped
        
        // "the frequency counter is being reset to 0 when the clock was stopped and is
        // restarted (->hzsync0.prg, hzsync1.prg)"
        cia->tod_cycles = 0;
    }

    cia->reg[CRA + c] = v;

    int old_pbon = old_crx & CR_PBON; // Generic bit works for both timers
    int new_pbon = v & CR_PBON;
    // Detect PBON bit change from high to low:
    if (old_pbon > new_pbon) {
        // Re-initialize this port B bit to 1. This solves $"{VICE_testprogs}CIA/pb6pb7/main.prg",
        // which expects 0x3F to restore to 0xFF once the CRA/CRB PBON bits are cleared.
        cia->port_b_value |= ((c == A) ? PB6_MASK : PB7_MASK);
    }

    if (old_pbon != new_pbon)
        mos6526_update_internal_data_direction_port_b(cia, cia->reg[DDRB]);
}

// TIME OF DAY (TOD) handling

// "Since a carry from one stage to the next can occur at any time with respect to read
// operation, a latching function is included to keep all Time Of Day information constant
// during a read sequence. All four TOD registers latch on a read of Hours and remain latched
// until after a read of 10ths of seconds. The TOD clock continues to count when the output
// registers are latched. If only one register is to be read, there is no carry problem and
// the register can be read "on the fly", provided that any read of Hours is followed by
// a read of 10ths of seconds to disable the latching."
uint8_t mos6526_latch_read_tod_hr(mos6526_t* cia) {
    cia->read_tod_delta = CLOCK_OFFSET;
    cia->reg[CLOCK_OFFSET + TOD_10THS] = cia->reg[TOD_10THS];
    cia->reg[CLOCK_OFFSET + TOD_SEC] = cia->reg[TOD_SEC];
    cia->reg[CLOCK_OFFSET + TOD_MIN] = cia->reg[TOD_MIN];
    return cia->reg[CLOCK_OFFSET + TOD_HR] = cia->reg[TOD_HR];
}

uint8_t mos6526_unlatch_read_tod_10ths(mos6526_t* cia) {
    cia->read_tod_delta = 0;
    return cia->reg[CLOCK_OFFSET + TOD_10THS];
}

uint8_t mos6526_write_tod_hr(mos6526_t* cia, uint8_t v) {
    // When writing 12 hours (assuming more, too) flips the given AM/PM bit
    if ((v & TOD_HR_MASK) >= 0x12) // Note the BCD encoding!
        v ^= TOD_HR_PM;

    return v;
}

void mos6526_check_alarm_interrupt(mos6526_t* cia) {
    // Are time of day and alarm time equal?
    // Note, this must be checked BEFORE increasing any TOD register, so that
    // a preceding TOD reset to zero will hit such an alarm (as it should)

    // Per chips_mos6526.hpp lines 503-512: Only trigger interrupt on RISING EDGE
    // This prevents retriggering the alarm interrupt every cycle when alarm condition stays true
    bool alarm_active = (
        cia->reg[TOD_10THS] == cia->reg[ALARM_OFFSET + TOD_10THS] &&
        cia->reg[TOD_SEC] == cia->reg[ALARM_OFFSET + TOD_SEC] &&
        cia->reg[TOD_MIN] == cia->reg[ALARM_OFFSET + TOD_MIN] &&
        cia->reg[TOD_HR] == cia->reg[ALARM_OFFSET + TOD_HR]
    );

    // Only set interrupt flag on rising edge (alarm goes from false to true)
    if (alarm_active && !cia->prev_alarm_state) {
        cia->reg[ICR] |= ICR_ALRM;
    }

    // Store current alarm state for next cycle's edge detection
    cia->prev_alarm_state = alarm_active;
}

static uint8_t bcd_inc(mos6526_t* cia, uint32_t r) { // r:TOD_SEC,TOD_MIN or TOD_HR
    uint8_t v = ++cia->reg[r]; // Increment the TOD register
    if ((v & 0x0F) > 9) { // Did low BCD nibble overflow? TODO : Verify; Should this be == 0x0A?
        v += 6; // Carry over to a high nibble increase TODO : Verify; Should this also do & 0xF0?
        cia->reg[r] = v; // Update the TOD register too
    }
    return v; // Return the result, so that caller can immediately check and handle upper-bound
}

void mos6526_increase_tod_and_check_alarm(mos6526_t* cia) {
    // Instead of detecting pulses on TOD pin (which happens only
    // 50 or 60 times per second) count cycles.
    if (cia->tod_cycles++ < cia->cycles_tod[(cia->reg[CRA] & CRA_TODIN) >> 7])
        return;

    cia->tod_cycles = 0;
    mos6526_check_alarm_interrupt(cia);

    if (++cia->reg[TOD_10THS] <= 9)
        return;

    cia->reg[TOD_10THS] = 0;
    // Note : Invalid BCD-encoded register values are treated as if they ARE valid;
    // Only when they overflow, does a reset happen which makes them valid BCD again.
    if (bcd_inc(cia, TOD_SEC) <= 0x59) // Note the BCD encoding!
        return;

    cia->reg[TOD_SEC] = 0;
    if (bcd_inc(cia, TOD_MIN) <= 0x59) // Note the BCD encoding!
        return;

    cia->reg[TOD_MIN] = 0;
    // Hour increments are somewhat special (besides their BCD encoding);
    // 0x11 (11 AM) must not become 0x12 (12 AM) but 0x92 (12 PM)
    // 0x12 (12 AM) must not become 0x91 ( 1 PM) but 0x01 ( 1 AM)
    // 0x91 (11 PM) must not become 0x92 (12 PM) but 0x12 (12 AM)
    // 0x92 (12 PM) must not become 0x01 ( 1 AM) but 0x81 (01 PM)
    // So, after increment, check the masked hours:
    // * when below 12, there's no change
    // * when equal to 12, swap the AM/PM state
    // * when exceeding 12, reset to 1
    uint8_t hr_new = bcd_inc(cia, TOD_HR);
    int hr_HR = hr_new & TOD_HR_MASK;
    if (hr_HR < 0x12) // Note the BCD encoding!
        return;

    int hr_PM = hr_new & TOD_HR_PM;
    if (hr_HR == 0x12) // Note the BCD encoding!
        hr_PM ^= TOD_HR_PM;
    else
        hr_HR = 1;

    cia->reg[TOD_HR] = (uint8_t)(hr_PM | hr_HR);
}

// SERIAL DATA REGISTER (SDR) handling

void mos6526_write_serial_data_register(mos6526_t* cia, uint8_t v) {
    cia->reg[SDR] = v;
    // "Transmission will start following a write to the Serial Data
    // Register (provided TIMER A is running and in continuous mode)."
    // Only start transmission in SPMODE=output (CRA bit 6 set)
    if ((cia->reg[CRA] & (CRA_SPMODE | CRA_START | CRA_RUNMODE)) == (CRA_SPMODE | CRA_START)) {
        // "If the microprocessor stays one byte ahead of the
        // shift register, transmission will be continuous."
        // Double-buffering: adding 8 allows the current byte to finish
        // before the new byte starts (detected by serial_shift & 7 == 0)
        if (cia->serial_shift < 8)
            cia->serial_shift += 8;
    }
}

void mos6526_serial_output(mos6526_t* cia) {
    // Called on internal CNT falling edge (generated by Timer A underflow toggle).
    // Data is shifted out on SP at 1/2 the Timer A underflow rate because:
    //   - Each Timer A underflow toggles CNT (HIGH→LOW or LOW→HIGH)
    //   - One bit shifts out on each falling edge of CNT
    //   - 2 underflows per bit = 1/2 the underflow rate

    // "The data in the Serial Data Register will be loaded
    // into the shift register, then shift out to the SP pin
    // when a CNT pulse occurs."
    // Load at byte boundaries (serial_shift is a multiple of 8)
    if ((cia->serial_shift & 7) == 0)
        cia->reg[SHIFT_OFFSET] = cia->reg[SDR];

    // "SDR data is shifted out MSB first and serial input data
    // should also appear in this format."
    int current_bit = (--cia->serial_shift) & 7;
    cia->sp_output_bit = (cia->reg[SHIFT_OFFSET] >> current_bit) & 1;

    if (current_bit == 0) {
        // "After 8 CNT pulses, an interrupt is generated
        // to indicate more data can be sent."
        cia->reg[ICR] |= ICR_SP;
        // "If the Serial Data Register was loaded with new
        // information prior to this interrupt, the new data
        // will automatically be loaded into the shift register
        // and transmission will continue."
    }
}

void mos6526_serial_input(mos6526_t* cia, bus_state_t bus_state) {
    // "In input mode, data on the SP pin is
    // shifted into the shift register on the rising edge of
    // the signal applied to the CNT pin."
    // NOTE: Per datasheet this should trigger on CNT rising edge.
    // The caller may use either edge depending on compatibility needs.
    bool sp_bit = BUS_GET_BIT(bus_state, BUS_SP_BIT);
    cia->reg[SHIFT_OFFSET] = (cia->reg[SHIFT_OFFSET] << 1) | (sp_bit ? 1 : 0);

    if (++cia->serial_shift >= 8) {
        // "After 8 CNT pulses, the data in the shift register is dumped
        // into the Serial Data Register and an interrupt is generated."
        cia->reg[SDR] = cia->reg[SHIFT_OFFSET];
        cia->reg[SHIFT_OFFSET] = 0;
        cia->serial_shift = 0;
        // SDR full or empty, so full byte was transferred
        cia->reg[ICR] |= ICR_SP;
    }
}

// The CIA 1 registers are repeated each 16 bytes in the area $dc00-$dcff
// The CIA 2 registers are repeated each 16 bytes in the area $dd00-$ddff
bus_state_t mos6526_registers_read(void* context, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)context;
    uint16_t full_addr = BUS_GET_ADDR(bus_state);
    uint8_t reg = full_addr & CIA_REGS_MASK;
    
    // CIA register read logging disabled for now
    
    switch (reg) {
        // Read ports
        case PRA:
            BUS_SET_DATA(bus_state, mos6526_read_port_data(cia, A));
            break;
        case PRB:
            BUS_SET_DATA(bus_state, mos6526_read_port_data(cia, B));
            break;
        case DDRA:
            BUS_SET_DATA(bus_state, cia->reg[DDRA]);
            break;
        case DDRB:
            // Note : Assume this always excludes the optional PBON output mask? (If not, use IDDRB!)
            BUS_SET_DATA(bus_state, cia->reg[DDRB]);
            break;
        // Read timers - return current counter value
        case TA_LO:
            BUS_SET_DATA(bus_state, cia->reg[TA_LO]);
            break;
        case TA_HI:
            BUS_SET_DATA(bus_state, cia->reg[TA_HI]);
            break;
        case TB_LO:
            BUS_SET_DATA(bus_state, cia->reg[TB_LO]);
            break;
        case TB_HI:
            BUS_SET_DATA(bus_state, cia->reg[TB_HI]);
            break;
        // Read TOD registers
        case TOD_10THS: {
            uint8_t tod_value = (cia->read_tod_delta > 0) ? mos6526_unlatch_read_tod_10ths(cia) : cia->reg[TOD_10THS];
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
            uint8_t tod_hr_value = (cia->read_tod_delta > 0) ? cia->reg[CLOCK_OFFSET + TOD_HR] : mos6526_latch_read_tod_hr(cia);
            BUS_SET_DATA(bus_state, tod_hr_value);
            break;
        }
        // Read control registers
        case SDR:
            BUS_SET_DATA(bus_state, cia->reg[SDR]);
            break;
        case ICR: {
            uint8_t icr_value = mos6526_read_and_clear_interrupt_control_register(cia);
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

bus_state_t mos6526_registers_write(void* context, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)context;
    uint8_t reg = BUS_GET_ADDR(bus_state) & CIA_REGS_MASK;
    uint8_t value = BUS_GET_DATA(bus_state);
    
    // CIA register write logging - disabled for performance
    
    switch (reg) {
        // Write ports
        case PRA:
            cia->reg[PRA] = value;
            mos6526_update_output_port(cia, A, value);
            // Hardware: CIA2 Data Port A bits 0-1 control VIC-II memory bank selection
            // Note: VIC-II will monitor CIA2 writes at $DD00 directly in its tick function
            // This eliminates the need for callbacks and global state
            break;
        case PRB:
            cia->reg[PRB] = value;
            mos6526_update_output_port_b(cia, value);
            break;
        case DDRA:
            mos6526_write_data_direction_port(cia, A, value);
            // CRITICAL: ALWAYS trigger callback when DDRA changes
            // mos6526_write_data_direction_port() already updated port_a_value correctly
            // Don't call mos6526_update_output_port() as it would overwrite the value!
            if (cia->port_a_change_callback) {
                cia->port_a_change_callback(cia->port_a_callback_context, cia->port_a_value);
            }
            break;
        case DDRB:
            mos6526_write_data_direction_port(cia, B, value);
            mos6526_update_internal_data_direction_port_b(cia, value);
            mos6526_update_output_port_b(cia, cia->reg[PRB]);
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
            mos6526_check_reload_timer(cia, A);
            break;
        case TB_LO:
            cia->reg[TIMER_OFFSET + TB_LO] = value;
            // Per CIA6526 datasheet: Writing to timer registers updates LATCH only
            break;
        case TB_HI:
            cia->reg[TIMER_OFFSET + TB_HI] = value;
            // Writing to HI byte while stopped triggers reload (latch → counter)
            mos6526_check_reload_timer(cia, B);
            break;
        // Write TOD registers / ALARM latches
        case TOD_10THS:
            cia->reg[cia->write_tod_delta + TOD_10THS] = value;
            mos6526_check_alarm_interrupt(cia);
            cia->is_running_tod = true;
            break;
        case TOD_SEC:
            cia->reg[cia->write_tod_delta + TOD_SEC] = value;
            break;
        case TOD_MIN:
            cia->reg[cia->write_tod_delta + TOD_MIN] = value;
            break;
        case TOD_HR:
            cia->reg[cia->write_tod_delta + TOD_HR] = mos6526_write_tod_hr(cia, value);
            cia->is_running_tod = false;
            break;
        // Write control registers
        case SDR:
            mos6526_write_serial_data_register(cia, value);
            break;
        case ICR:
            mos6526_write_interrupt_control_register(cia, value);
            break;
        case CRA:
            mos6526_write_control_register(cia, A, value);
            break;
        case CRB:
            mos6526_write_control_register(cia, B, value);
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
bus_state_t mos6526_tick_phi2(void* chip, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)chip;
    
    // =========================================================================
    // INTERRUPT LINE ASSERTION (from PREVIOUS cycle's pending)
    // =========================================================================
    // 1-cycle delay: ICR_IRQ set at end of previous cycle → assert now
    if (cia->pending_bus_lines) {
        bus_state &= ~cia->pending_bus_lines;
    }

    // When PB6 and PB7 should pulse, clear them (the chance for a read was in previous cycle)
    uint8_t port_b_pulse_clear_mask = 0xFF;
    if ((cia->reg[CRA] & CRA_PBON) > 0)
        if ((cia->reg[CRA] & CRA_OUTMODE) == 0)
            port_b_pulse_clear_mask &= ~PB6_MASK;

    if ((cia->reg[CRB] & CRB_PBON) > 0)
        if ((cia->reg[CRB] & CRB_OUTMODE) == 0)
            port_b_pulse_clear_mask &= ~PB7_MASK;

    if (port_b_pulse_clear_mask != 0xFF)
        cia->port_b_value &= port_b_pulse_clear_mask;

    // =========================================================================
    // Clear one-cycle underflow pulse flags from previous cycle
    // =========================================================================
    cia->timer_underflowed = 0;

    // =========================================================================
    // PIN STATE AND EDGE DETECTION
    // =========================================================================
    bool cnt_pin = BUS_GET_BIT(bus_state, BUS_CNT_BIT);
    bool flag_pin = BUS_GET_BIT(bus_state, BUS_FLAG_BIT);
    
    bool prev_cnt = BUS_GET_BIT(cia->prev_bus_state, BUS_CNT_BIT);
    bool prev_flag = BUS_GET_BIT(cia->prev_bus_state, BUS_FLAG_BIT);
    
    bool cnt_is_positive_edge = (cnt_pin && !prev_cnt);
    bool cnt_is_negative_edge = (!cnt_pin && prev_cnt);
    bool flag_is_negative_edge = (!flag_pin && prev_flag);
    
    cia->prev_bus_state = bus_state;

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
    mos6526_decrease_timer(cia, A);
    mos6526_decrease_timer(cia, B);

    // =========================================================================
    // LOAD PIPELINE CHECK (from force load or HI byte write while stopped)
    // =========================================================================
    // Per chips_mos6526.hpp: TIMER_LOAD[0] → counter = latch, CLR COUNT[1]
    // Runs regardless of timer state (force load works even when timer is stopped).
    // Must run AFTER decrease_timer so underflow reload happens first.
    // Clear only bit 1 of count pipeline (matching reference), not all bits.
    if (cia->delay_line.Check(ta_load_pipe)) {
        mos6526_reload_timer(cia, A);
        uint64_t bits = cia->delay_line.Read(ta_count_pipe);
        bits &= ~(uint64_t(1) << 1);  // Clear bit 1 only
        cia->delay_line.Inject(ta_count_pipe, bits);
    }
    if (cia->delay_line.Check(tb_load_pipe)) {
        mos6526_reload_timer(cia, B);
        uint64_t bits = cia->delay_line.Read(tb_count_pipe);
        bits &= ~(uint64_t(1) << 1);  // Clear bit 1 only
        cia->delay_line.Inject(tb_count_pipe, bits);
    }

    if (cia->is_running_tod)
        mos6526_increase_tod_and_check_alarm(cia);

    // Serial I/O:
    // - Output mode (SPMODE=1): Handled internally by Timer A underflow toggling
    //   the CNT flip-flop in mos6526_decrease_timer(). No external CNT needed.
    // - Input mode (SPMODE=0): External CNT drives the shift register clock.
    //   Per datasheet: "data on the SP pin is shifted into the shift register
    //   on the rising edge of the signal applied to the CNT pin."
    if ((cia->reg[CRA] & CRA_SPMODE) == 0 && cnt_is_negative_edge) {
        mos6526_serial_input(cia, bus_state);
    }
    
    // Drive CIA serial output pins onto bus for external visibility (user port)
    if (cia->reg[CRA] & CRA_SPMODE) {
        // SPMODE=output: CIA drives CNT and SP pins
        if (!cia->cnt_output_state) {
            bus_state &= ~BUS_BIT(BUS_CNT_BIT);  // Pull CNT LOW
        }
        if (cia->sp_output_bit) {
            bus_state |= BUS_BIT(BUS_SP_BIT);     // Drive SP HIGH
        } else {
            bus_state &= ~BUS_BIT(BUS_SP_BIT);    // Drive SP LOW
        }
    }

    // FLAG negative edge detection
    if (flag_is_negative_edge) {
        cia->reg[ICR] |= ICR_FLG;
    }

    // =========================================================================
    // INTERRUPT PROCESSING
    // =========================================================================
    mos6526_check_interrupt_mask(cia);
    
    // Update pending_bus_lines for NEXT cycle's assertion
    if (cia->reg[ICR] & ICR_IRQ) {
        cia->pending_bus_lines = BUS_BIT(cia->configured_interrupt_bit);
    } else {
        cia->pending_bus_lines = 0;
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
    if ((cia->reg[CRA] & CRA_INMODE) == 0) {
        ta_active = true;  // PHI2 mode: always active
    } else {
        ta_active = cnt_is_positive_edge;  // CNT mode: active on positive edge
    }
    if (ta_active && (cia->reg[CRA] & CRA_START)) {
        cia->delay_line.Inject(ta_count_pipe);
    } else {
        // Clear injection point only (LSB), matching reference CLR(pip, COUNT, 2)
        // Lets in-flight pipeline bits drain naturally
        cia->delay_line.Inject(ta_count_pipe, false);
    }
    
    // Timer B counter pipeline
    // Timer B INMODE: bits 5-6 of CRB (00=PHI2, 01=CNT, 10=Timer A, 11=Timer A+CNT)
    bool tb_active = false;
    uint8_t crb_inmode = cia->reg[CRB] & CRB_INMODE;
    switch (crb_inmode) {
        case 0x00:  // PHI2
            tb_active = true;
            break;
        case 0x20:  // CNT
            tb_active = cnt_is_positive_edge;
            break;
        case 0x40:  // Timer A cascade
            tb_active = (cia->timer_underflowed & (1 << A)) != 0;
            break;
        case 0x60:  // Timer A + CNT
            tb_active = ((cia->timer_underflowed & (1 << A)) != 0) && cnt_pin;
            break;
    }
    if (tb_active && (cia->reg[CRB] & CRB_START)) {
        cia->delay_line.Inject(tb_count_pipe);
    } else {
        // Clear injection point only (LSB), matching reference CLR(pip, COUNT, 2)
        cia->delay_line.Inject(tb_count_pipe, false);
    }
    
    // Inject one-shot mode state each cycle (only while timer is running)
    if ((cia->reg[CRA] & (CR_START | CR_RUNMODE)) == (CR_START | CR_RUNMODE)) {
        cia->delay_line.Feed(oneshot_a_pipe);
    }
    if ((cia->reg[CRB] & (CR_START | CR_RUNMODE)) == (CR_START | CR_RUNMODE)) {
        cia->delay_line.Feed(oneshot_b_pipe);
    }
    
    // Shift all pipeline signals towards MSB (output position)
    cia->delay_line.Shift();

    return bus_state;
}

/**
 * CIA tick PHI1 phase - called AFTER CPU PHI2 and memory service.
 * Handles delayed mask transfer and cleanup. Timer counting has already
 * happened in phi2, so register reads during memory service see post-decrement values.
 */
bus_state_t mos6526_tick_phi1(void* chip, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)chip;
    
    // Transfer delayed mask to active mask at end of cycle
    cia->interrupt_mask = cia->interrupt_mask_delayed;
    
    // Clear Timer B Bug flag for next cycle
    cia->icr_read_this_cycle = false;
    
    return bus_state;
}

/**
 * Legacy single-phase CIA tick (calls phi2 + phi1 in sequence).
 * Used for backward compatibility with non-C64 systems.
 */
bus_state_t mos6526_tick(void* chip, bus_state_t bus_state) {
    bus_state = mos6526_tick_phi2(chip, bus_state);
    bus_state = mos6526_tick_phi1(chip, bus_state);
    return bus_state;
}
