#include "mos6526.h" // cia
//#include "../../systems/c64/c64_bus.h" // for BUS_MASK_IRQ
#include "../../core/system_lines.h"
#include <string.h>
#include <stdlib.h>
#include <cstdio>

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
    cia->pending_bus_lines = 0;  // No pending interrupt assertions
    
    // Initialize delay line (multi-cycle signal propagation)
    cia->delay_line.Clear();
    
    // Initialize PB6/PB7 toggle flip-flops (cleared on reset per CIA6526.txt line 107)
    cia->pb67_toggle = 0;
    
    // Initialize previous bus state for edge detection
    // CNT and FLAG pins have internal pull-ups, so they start HIGH
    cia->prev_bus_state = BUS_BIT(BUS_CNT_BIT) | BUS_BIT(BUS_FLAG_BIT);
    
    // Call port A change callback with initial value (all high due to pull-ups)
    if (cia->port_a_change_callback) {
        cia->port_a_change_callback(cia->port_a_callback_context, cia->port_a_value);
    }
}

void* mos6526_system_create(chip_descriptor_t* desc) {
    mos6526_t* cia = (mos6526_t*)calloc(1, sizeof(mos6526_t));
    if (!cia) return NULL;
    cia->desc = desc;
    cia->configured_interrupt_bit = BUS_IRQ_BIT; // Default to IRQ; caller must set to NMI for CIA2
    // Constructor equivalent - set up cycles for TOD
    // Used when CRA_TODIN = 0 (60 Hz TOD pin input pulses)
    cia->cycles_tod[0] = 1000000 / 60; // Assuming 1MHz CPU clock
    // Used when CRA_TODIN = 1 (50 Hz TOD pin input pulses)
    cia->cycles_tod[1] = 1000000 / 50;
    
    // Initialize port read callbacks to NULL
    cia->port_a_read_callback = NULL;
    cia->port_a_read_context = NULL;
    cia->port_b_read_callback = NULL;
    cia->port_b_read_context = NULL;
    
    // Initialize port change callback to NULL
    cia->port_a_change_callback = NULL;
    cia->port_a_callback_context = NULL;
    
    mos6526_reset(cia);
    return cia;
}

void mos6526_system_destroy(void* chip) {
    free(chip);
}

// INTERRUPT CONTROL REGISTER (ICR) handling

uint8_t mos6526_read_and_clear_interrupt_control_register(mos6526_t* cia) {
    // "The interrupt DATA register is cleared" (the /IRQ line
    // does NOT return high following a read of the DATA register!)
    uint8_t v = cia->reg[ICR];
    
    // "interrupt can be prevented by reading the ICR at the time of the underflow."
    cia->reg[ICR] = 0;
    
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
    
    bool was_irq_set = (cia->reg[ICR] & ICR_IRQ) != 0;
    
    if (masked_interrupts > 0) {
        // "Any interrupt which is enabled by the MASK register will
        // set the IR bit (MSB) of the DATA register and bring
        // the /IRQ pin low."
        //
        // With the new pull-up resistor model, we simply set ICR_IRQ.
        // The next mos6526_tick() will see this and assert the interrupt line.
        // No need for delayed_irq flag - the interrupt persists until ICR is read.
        cia->reg[ICR] |= ICR_IRQ;
    } else {
        // CRITICAL FIX: Clear ICR_IRQ when no masked interrupts are pending
        // This handles the case where the interrupt mask is cleared while ICR_TA/TB bits are still set
        // Without this, ICR_IRQ would stay set forever after mask is cleared
        if (was_irq_set) {
            cia->reg[ICR] &= ~ICR_IRQ;
        }
    }
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
    if ((v & ICR_S_C) == 0)
        // 0 = set bits 0..4 are clearing the according mask bit.
        cia->interrupt_mask &= ~bits;
    else
        // 1 = set bits 0..4 are setting the according mask bit."
        cia->interrupt_mask |= bits;

    // "When a condition in the ICR is true, setting the corresponding bit in the IMR must also set the interrupt."
    // "Clearing the bit in the IMR may not clear the interrupt."
    mos6526_check_interrupt_mask(cia);
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

void mos6526_decrease_timer(mos6526_t* cia, uint32_t t, bool cnt_is_positive_edge, int in_mode, bool cnt_pin) { // t:A or B
    // Phase 3: Check delay line for countdown signal (4-cycle delay from input to actual decrement)
    // Per CIA6526.txt lines 13-63: Timer countdown uses 4-cycle delay pipeline
    bool count_ready = (t == A) ? cia->delay_line.Check(ta_count_pipe) : cia->delay_line.Check(tb_count_pipe);
    
    // Only decrement timer if the countdown signal has propagated through the delay line
    if (!count_ready)
        return;  // Countdown signal not ready yet
    
    // Note : CRA 5 INMODE mask is 1 bit (will only ever hit cases 0 and 1)
    // "CRB 5,6 INMODE
    // Bits CRB5 and CRB6 select one of four input modes for TIMER B as:
    bool count_timer = false;
    switch (in_mode) {
        // 0 = TIMER A counts phi2 pulses
        // 0 0 TIMER B counts phi2 pulses
        case (0x00 << 5):  // 0b00 = 0x00
            count_timer = true;  // Always count in PHI2 mode (already injected in control register)
            break;
        // 1 = TIMER A counts positive CNT transitions.
        // 0 1 TIMER B counts positive CNT transitions.
        case (0x01 << 5):  // 0b01 = 0x01
            count_timer = cnt_is_positive_edge;
            break;
        // 1 0 TIMER B counts TIMER A underflow pulses.
        case (0x02 << 5):  // 0b10 = 0x02
            count_timer = (cia->reg[ICR] & ICR_TA) > 0;
            break;
        // 1 1 TIMER B counts TIMER A underflow pulses while CNT is high.
        // Phase 6: Check CNT pin state for mode 0x60
        default:
            count_timer = ((cia->reg[ICR] & ICR_TA) > 0) && cnt_pin;
            break;
    }
    if (!count_timer)
        return;

    uint32_t i = t * 2; // Turn A or B into TA_LO / TB_LO offsets
    uint16_t timer = (cia->reg[TA_HI + i] << 8) | cia->reg[TA_LO + i];
    
    // Decrement timer (automatically wraps at 16 bits: $0000 - 1 = $FFFF)
    timer--;
    
    // Check for underflow AFTER decrementing (timer wraps to $FFFF)
    // CIA timers underflow when they decrement from $0000, wrapping to $FFFF
    if (timer == 0xFFFF) {
        // Timer underflowed - reload from latch
        // Set ICR flag
        cia->reg[ICR] |= (uint8_t)(ICR_TA + t); // t=A:ICR_TA, t=B:ICR_TB
        mos6526_reload_timer(cia, t);
        
        // Phase 5: Toggle PB6/PB7 flip-flop on timer underflow
        // Per CIA6526.txt lines 104-110: Each underflow toggles the flip-flop
        // Only affects output when PBON=1 and OUTMODE=1 (toggle mode)
        uint8_t toggle_bit = (t == A) ? PB6_MASK : PB7_MASK;
        cia->pb67_toggle ^= toggle_bit;  // XOR to toggle
        
        // "In one-shot mode, the timer will count down from
        // latched value to zero, generate the interrupt, reload
        // the latched value, then stop. In continuous mode,
        // the timer will count from latched value to zero,
        // generate interrupt, reload the latched value and
        // repeat the procedure continuously."
        // RUNMODE: 0 = continuous (keep running), 1 = one-shot (stop after underflow)
        if ((cia->reg[CRA + t] & CR_RUNMODE) != 0) {
            // Stop timer (Clear START bit) - one-shot mode only
            cia->reg[CRA + t] &= ~CR_START;
            
            // IMPORTANT: Do NOT clear ICR bit here!
            // Per CIA6526.txt documentation: "Only reading the ICR will clear it."
            // The ICR_TA/ICR_TB bit represents the interrupt condition and must persist
            // until software reads the ICR register to acknowledge the interrupt.
        }
        // else: Continuous mode - timer keeps running with reloaded value
    } else {
        // Store decremented value
        cia->reg[TA_LO + i] = (uint8_t)(timer & 0xFF);
        cia->reg[TA_HI + i] = (uint8_t)(timer >> 8);
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
            // Reset the shift register
            cia->reg[SHIFT_OFFSET] = 0;
            // TODO : What to do with SerialShift?
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
    
    // Set clock in PHI2 mode (START=1, appropriate INMODE bits=0)
    bool running_phi2_mode = false;
    if (c == A) {
        // Timer A: INMODE is single bit (0x20), 0=PHI2, 1=CNT
        running_phi2_mode = ((v & (CRA_START | CRA_INMODE)) == CRA_START);
        if (running_phi2_mode) {
            // Timer is running and counting PHI2 cycles
            cia->delay_line.Inject(ta_count_pipe);
        } else {
            // Timer stopped or using external clock - clear countdown signals
            cia->delay_line.Clear(ta_count_pipe);
        }
        
        // Set one-shot mode state
        if ((v & CR_RUNMODE) != 0) {
            cia->delay_line.Inject(oneshot_a_pipe);
        } else {
            cia->delay_line.Clear(oneshot_a_pipe);
        }
    } else { // c == B
        // Timer B: INMODE is 2 bits (0x60), 0=PHI2, 1=CNT, 2=Timer A, 3=Timer A+CNT
        running_phi2_mode = ((v & (CRB_START | CRB_INMODE)) == CRB_START);
        
        if (running_phi2_mode) {
            // Timer is running and counting PHI2 cycles
            cia->delay_line.Inject(tb_count_pipe);
        } else {
            // Timer stopped or using external clock - clear countdown signals
            cia->delay_line.Clear(tb_count_pipe);
        }
        
        // Set one-shot mode state
        if ((v & CR_RUNMODE) != 0) {
            cia->delay_line.Inject(oneshot_b_pipe);
        } else {
            cia->delay_line.Clear(oneshot_b_pipe);
        }
    }

    if ((v & CR_LOAD) > 0) {
            // "Force Load
            //  A strobe bit allows the timer latch to be loaded
            // into the timer counter at any time, whether the timer
            // is running or not."
            // Per CIA6526.cpp lines 410-412: Inject LOAD signal into delay pipeline
            if (c == A) {
                cia->delay_line.Inject(ta_load_pipe);
            } else {
                cia->delay_line.Inject(tb_load_pipe);
            }
            // Note: Actual reload happens 2 cycles later when signal reaches MSB
            
            // "  4    LOAD   1 = FORCE LOAD (this is a STROBE input, there is no data storage, bit 4 will
            //                    always read back a zero and writing a zero has no effect)."
            v &= ~CR_LOAD; // Clear the LOAD strobe bit
            
            // CRITICAL FIX: Clear the interrupt flag when manually reloading the timer
            // This prevents spurious interrupts when restarting a timer that had previously underflowed
            // The KERNAL uses this pattern: timer underflows → disable mask → reload timer → re-enable mask
            // Without clearing the flag here, the old ICR_TA bit causes immediate re-interrupt
            uint8_t timer_flag = (c == A) ? ICR_TA : ICR_TB;
            if (cia->reg[ICR] & timer_flag) {
                cia->reg[ICR] &= ~timer_flag;
            }
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
        
        // Reload timer from latch so it starts with the programmed value
        // This is critical for tests that set up a specific count period
        // Per CIA6526.cpp lines 278-282: Inject LOAD signal into delay pipeline
        if (c == A) {
            cia->delay_line.Inject(ta_load_pipe);
        } else {
            cia->delay_line.Inject(tb_load_pipe);
        }
        
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
    if (cia->reg[TOD_10THS] == cia->reg[ALARM_OFFSET + TOD_10THS] &&
        cia->reg[TOD_SEC] == cia->reg[ALARM_OFFSET + TOD_SEC] &&
        cia->reg[TOD_MIN] == cia->reg[ALARM_OFFSET + TOD_MIN] &&
        cia->reg[TOD_HR] == cia->reg[ALARM_OFFSET + TOD_HR])
        cia->reg[ICR] |= ICR_ALRM;
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
    if ((cia->reg[CRA] & (CRA_START | CRA_RUNMODE)) == CRA_START) {
        // "If the microprocessor stays one byte ahead of the
        // shift register, transmission will be continuous."
        if (cia->serial_shift < 8)
            // TODO : Is this correct?
            cia->serial_shift += 8;
    }
}

void mos6526_serial_output(mos6526_t* cia) {
    // TODO : "In the output mode, TIMER A is used for
    // the baud rate generator. Data is shifted out on the
    // SP pin at 1/2 the underflow rate of TIMER A."

    // "If no further data is to be transmitted, after the 8th CNT
    // pulse, CNT will return high and SP will remain at the level
    // of the last data bit transmitted."
    if (cia->serial_shift == 0)
        return; // TODO : Is this correct?

    // "The data in the Serial Data Register will be loaded
    // into the shift register, then shift out to the SP pin
    // when a CNT pulse occurs."
    if (cia->serial_shift == 8)
        cia->reg[SHIFT_OFFSET] = cia->reg[SDR];

    // "SDR data is shifted out MSB first and serial input data
    // should also appear in this format."
    int current_bit = (--cia->serial_shift) & 7;
    // SP.Value = (cia->reg[SHIFT_OFFSET] >> current_bit) & 1;
    if (current_bit == 0) {
        // "After 8 CNT pulses, an interrupt is generated
        // to indicate more data can be sent."
        cia->reg[ICR] |= ICR_SP; // TODO: Verify
        // "If the Serial Data Register was loaded with new
        // information prior to this interrupt, the new data
        // will automatically be loaded into the shift register
        // and transmission will continue."
    }
}

void mos6526_serial_input(mos6526_t* cia) {
    // "In input mode, data on the SP pin is
    // shifted into the shift register on the rising edge of
    // the signal applied to the CNT pin."
    // cia->reg[SHIFT_OFFSET] |= (SP.Value << cia->serial_shift);
    if (cia->serial_shift++ == 0) {
        // "After 8 CNT pulses, the data in the shift register is dumped
        // into the Serial Data Register and an interrupt is generated."
        cia->reg[SDR] = cia->reg[SHIFT_OFFSET];
        cia->reg[SHIFT_OFFSET] = 0;
        // SDR full or empty, so full byte was transferred,
        // depending of operating mode serial bus
        cia->reg[ICR] |= ICR_SP; // TODO: Verify
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

chip_descriptor_t mos6526_descriptor = {
    .description = "MOS6526 CIA Complex Interface Adapter",
    .create = mos6526_system_create,
    .destroy = mos6526_system_destroy,
    .bus_attach = NULL,
    .bank_change = NULL,
#ifdef IMGUI_VERSION
    .render_debug_window = mos6526_render_debug_window,
    .render_settings_window = mos6526_render_settings_window
#endif
};

/**
 * Main CIA tick function - handles all CIA cycle processing including timers,
 * TOD clock, serial I/O, and interrupt generation with 1-cycle delay.
 *
 * @param chip Pointer to CIA chip instance
 * @param bus_state Current bus state
 * @return Updated bus state with interrupt lines updated
 */
bus_state_t mos6526_tick(void* chip, bus_state_t bus_state) {
    mos6526_t* cia = (mos6526_t*)chip;
    
    // =========================================================================
    // CONTINUOUS INTERRUPT LINE ASSERTION MODEL
    // =========================================================================
    // Real CIA hardware continuously asserts (pulls LOW) the IRQ/NMI line via an
    // open-collector output as long as ICR_IRQ is set. This happens on EVERY cycle,
    // not just once.
    //
    // The 1-cycle delay (CIA6526.txt lines 119-120) is handled by the fact that
    // ICR_IRQ is set at the END of cycle N (in mos6526_check_interrupt_mask),
    // and then this assertion happens at the START of cycle N+1.
    //
    // IRQ/NMI are active-LOW: bit=1 means inactive (HIGH), bit=0 means asserted (LOW)
    //
    // CRITICAL: The CIA must assert the interrupt line on EVERY cycle where ICR_IRQ
    // is set, because the system tick function resets the bus state to default_state
    // at the start of each cycle. Without continuous assertion, the interrupt line
    // would only be LOW for one cycle and then return HIGH.
    
    // Check if there was a pending interrupt from previous cycle and apply it now
    // This implements the 1-cycle delay: interrupt events from cycle N-1 are
    // asserted on the bus starting in cycle N
    if (cia->pending_bus_lines) {
        bus_state &= ~cia->pending_bus_lines;
    }

    // When PB6 and PB7 should pulse, clear them (the chance for a read was in previous cycle)
    uint8_t port_b_pulse_clear_mask = 0xFF; // Keep all bits initially
    if ((cia->reg[CRA] & CRA_PBON) > 0)
        if ((cia->reg[CRA] & CRA_OUTMODE) == 0) // pulse timer mode
            port_b_pulse_clear_mask &= ~PB6_MASK; // Clear bit 6 in mask

    if ((cia->reg[CRB] & CRB_PBON) > 0)
        if ((cia->reg[CRB] & CRB_OUTMODE) == 0) // pulse timer mode
            port_b_pulse_clear_mask &= ~PB7_MASK; // Clear bit 7 in mask

    // Slight optimization: only apply mask when needed (avoiding relatively slow port access)
    if (port_b_pulse_clear_mask != 0xFF)
        cia->port_b_value &= port_b_pulse_clear_mask;

    // =========================================================================
    // PIN STATE AND EDGE DETECTION (using bus state pattern)
    // =========================================================================
    // Sample current pin states from bus (with internal pull-ups, pins are HIGH unless externally pulled LOW)
    // In a real C64, these pins connect to external devices:
    // - CNT: Connected to cassette port, serial bus, user port
    // - FLAG: Connected to cassette port (CIA1), RS-232 (CIA2)
    // For now, simulate as pulled HIGH (no external devices pulling them LOW)
    bool cnt_pin = BUS_GET_BIT(bus_state, BUS_CNT_BIT);
    bool flag_pin = BUS_GET_BIT(bus_state, BUS_FLAG_BIT);
    
    // Detect edge transitions by comparing current bus state with previous bus state
    bool prev_cnt = BUS_GET_BIT(cia->prev_bus_state, BUS_CNT_BIT);
    bool prev_flag = BUS_GET_BIT(cia->prev_bus_state, BUS_FLAG_BIT);
    
    bool cnt_is_positive_edge = (cnt_pin && !prev_cnt);    // LOW->HIGH transition
    bool cnt_is_negative_edge = (!cnt_pin && prev_cnt);    // HIGH->LOW transition
    bool flag_is_negative_edge = (!flag_pin && prev_flag); // HIGH->LOW transition
    
    // Store current bus state for edge detection in next cycle (standard chip pattern)
    cia->prev_bus_state = bus_state;


    // =========================================================================
    // PHASE 4: DELAY LINE SHIFTING (Multi-cycle signal propagation)
    // =========================================================================
    // Per CIA6526.txt lines 13-84: Timer operations use delay pipelines
    // Each cycle, shift the delay line left by 1 to advance all signals
    // Continuous signals (PHI2 countdown, one-shot mode) are preserved by re-injecting
    // each cycle when the condition is true
    
    // Shift all pipelines by 1 cycle
    cia->delay_line.Shift();
    
    // Timer A: Re-inject countdown signal if running in PHI2 mode
    if ((cia->reg[CRA] & CRA_START) && ((cia->reg[CRA] & CRA_INMODE) == 0)) {
        // Timer A running in PHI2 mode - keep injecting countdown signals
        cia->delay_line.Inject(ta_count_pipe);
    }
    
    // Timer B: Re-inject countdown signal if running in PHI2 mode
    if ((cia->reg[CRB] & CRB_START) && ((cia->reg[CRB] & CRB_INMODE) == 0)) {
        // Timer B running in PHI2 mode (00) - keep injecting countdown signals
        cia->delay_line.Inject(tb_count_pipe);
    }
    
    // Re-inject one-shot mode state each cycle (continuous state signals)
    if (cia->reg[CRA] & CR_RUNMODE) {
        cia->delay_line.Inject(oneshot_a_pipe);
    }
    if (cia->reg[CRB] & CR_RUNMODE) {
        cia->delay_line.Inject(oneshot_b_pipe);
    }

    // =========================================================================
    // TIMER COUNTDOWN
    // =========================================================================
    bool timer_a_running = (cia->reg[CRA] & CRA_START) > 0;
    bool timer_b_running = (cia->reg[CRB] & CRB_START) > 0;
    
    if (timer_a_running)
        mos6526_decrease_timer(cia, A, cnt_is_positive_edge, cia->reg[CRA] & CRA_INMODE, cnt_pin);

    if (timer_b_running)
        mos6526_decrease_timer(cia, B, cnt_is_positive_edge, cia->reg[CRB] & CRB_INMODE, cnt_pin);

    if (cia->is_running_tod)
        mos6526_increase_tod_and_check_alarm(cia);

    // "CRA:
    //  6   SPMODE  1 = SERIAL PORT output (CNT sources shift clock).
    //              0 = SERIAL PORT input (external shift clock required)."
    // "Data shifted out becomes valid on the falling edge on CNT and
    // remains valid until the next falling edge."
    if (cnt_is_negative_edge) {
        if ((cia->reg[CRA] & CRA_SPMODE) > 0)
            mos6526_serial_output(cia);
        else
            mos6526_serial_input(cia);
    }

    // "/FLAG is negative edge sensitive input"
    // CIA 1 : IRQ Signal occurred at FLAG-pin (cassette port Data input, serial bus SRQ IN)
    // CIA 2 : NMI Signal occurred at FLAG-pin (RS-232 data received)
    // "Any negative transition on /FLAG will set the /FLAG interrupt bit."
    if (flag_is_negative_edge) {
        cia->reg[ICR] |= ICR_FLG;
    }

    // Check interrupt mask FIRST to update ICR_IRQ based on current cycle's events
    // (timer underflows, etc.)
    mos6526_check_interrupt_mask(cia);
    
    // Update pending_bus_lines for NEXT cycle based on the updated ICR_IRQ state
    // This implements the required 1-cycle delay for interrupt assertion
    if (cia->reg[ICR] & ICR_IRQ) {
        // Interrupt pending - assert line in NEXT cycle
        cia->pending_bus_lines = BUS_BIT(cia->configured_interrupt_bit);
    } else {
        // No interrupt - clear pending for next cycle
        cia->pending_bus_lines = 0;
    }
    
    return bus_state;
}

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "mos6526_gui.h"
#endif
