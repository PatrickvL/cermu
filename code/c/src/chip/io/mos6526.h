#ifndef MOS6526_H
#define MOS6526_H

#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include <stdint.h>
#include <stdbool.h>

// CIA MOS 6526 DIP has 40 pins; Pinout :
typedef enum {
    PIN_VSS = 1, PIN_CNT = 40,
    PIN_PA0 = 2, PIN_SP = 39,
    PIN_PA1 = 3, PIN_RS0 = 38,
    PIN_PA2 = 4, PIN_RS1 = 37,
    PIN_PA3 = 5, PIN_RS2 = 36,
    PIN_PA4 = 6, PIN_RS3 = 35,
    PIN_PA5 = 7, PIN_RES = 34,
    PIN_PA6 = 8, PIN_DB0 = 33,
    PIN_PA7 = 9, PIN_DB1 = 32,
    PIN_PB0 = 10, PIN_DB2 = 31,
    PIN_PB1 = 11, PIN_DB3 = 30,
    PIN_PB2 = 12, PIN_DB4 = 29,
    PIN_PB3 = 13, PIN_DB5 = 28,
    PIN_PB4 = 14, PIN_DB6 = 27,
    PIN_PB5 = 15, PIN_DB7 = 26,
    PIN_PB6 = 16, PIN_PHI2 = 25,
    PIN_PB7 = 17, PIN_FLAG = 24,
    PIN_PC = 18, PIN_CS = 23,
    PIN_TOD = 19, PIN_R_W = 22,
    PIN_VCC = 20, PIN_IRQ = 21
} mos6526_pin_t;

// Register dimensions
#define REGS_BITS 4
#define REGS_SIZE (1 << REGS_BITS) // 16
#define REGS_MASK (REGS_SIZE - 1)  // 15

// Constants
#define A 0
#define B 1
#define PB6_MASK (1 << 6)
#define PB7_MASK (1 << 7)
#define MASK5 0x1F

// Additional Reg offsets above the 0..15 register range:
#define TIMER_OFFSET 16 // Delta on TA_LO to TB_HI so Timer write latch resides at 16..19
#define CLOCK_OFFSET 20 // Delta on TOD_10THS to TOD_HR so TOD read latch resides at 20..23
#define ALARM_OFFSET 24 // Delta on TOD_10THS to TOD_HR so Alarm write latch resides at 24..27
#define SHIFT_OFFSET 28 // Delta on SDR so Serial Data Shift register resides at 28
#define IDDRB_OFFSET 29 // Internal Data Direction of Port B (a version of DDRB which includes the PBON mask)

typedef struct mos6526_s {
    chip_descriptor_t* desc;
    
    // CIA ports, timers, alarm, registers, latches, interrupt and other status variables.
    uint8_t port_a_value;
    uint8_t port_b_value;
    int cycles_tod[2]; // Assigned once in constructor
    uint8_t reg[REGS_SIZE + 4 + 4 + 4 + 1 + 1]; // Registers, plus TIMER, CLOCK, ALARM, SDR and DDRB latches
    bool delayed_irq;
    uint32_t read_tod_delta;
    uint32_t write_tod_delta;
    bool is_running_tod;
    int tod_cycles;
    int serial_shift;
    uint8_t interrupt_mask;
    
    bus_cycle_ops_t bus_interface;
} mos6526_t;

// Technical register indices (in decimal) and masks (in hexadecimal)
#define PRA 0             // $dc00 Peripheral Data Reg A Monitoring/control of the 8 data lines of Port A.
#define PRB 1             // $dc01 Peripheral Data Reg B Monitoring/control of the 8 data lines of Port B.
#define DDRA 2            // $dc02 Data Direction Register A Bit X: 0=Input (read only), 1=Output (read and write)
#define DDRB 3            // $dc03 Data Direction Register B Bit X: 0=Input (read only), 1=Output (read and write)
#define TA_LO 4           // $dc04 Timer A Low Register Read: actual value Timer A (Low Byte) Writing: Set latch of Timer A (Low Byte)
#define TA_HI 5           // $dc05 Timer A High Register Read: actual value Timer A (High Byte) Writing: Set latch of timer A (High Byte) - if the timer is stopped, the high-byte will automatically be re-set as well
#define TB_LO 6           // $dc06 Timer B Low Register Read: actual value Timer B (Low Byte) Writing: Set latch of Timer B (Low Byte)
#define TB_HI 7           // $dc07 Timer B High Register Read: actual value Timer B (High Byte) Writing: Set latch of timer B (High Byte) - if the timer is stopped, the high-byte will automatically be re-set as well
#define TOD_10THS 8       // $dc08 Real Time Clock 10ths Of Seconds
#define TOD_10THS_MASK 0x0F // $dc08 Real Time Clock 10ths Of Seconds Bit 0..3: Tenth seconds in BCD-format($0-$9) Bit 4..7: always 0
#define TOD_SEC 9         // $dc09 Real Time Clock Seconds
#define TOD_SEC_MASK 0x7F // $dc0a Real Time Clock Seconds Bit 0..3: Single seconds in BCD-format( $0-$9) Bit 4..6: Ten seconds in BCD-format ($0-$5) Bit 7: always 0
#define TOD_MIN 10        // $dc0a Real Time Clock Minutes
#define TOD_MIN_MASK 0x7F // $dc0a Real Time Clock Minutes Bit 0..3: Single minutes in BCD-format( $0-$9) Bit 4..6: Ten minutes in BCD-format ($0-$5) Bit 7: always 0
#define TOD_HR 11         // $dc0b Real Time Clock Hours
#define TOD_HR_PM 0x80    // $dc0b Real Time Clock Hours - Bit 7 Read: Differentiation AM/PM, 0=AM, 1=PM Writing into this register stops TOD, until register 8 (TOD 10THS) will be read.
#define TOD_HR_MASK 0x1F  // $dc0b Real Time Clock Hours - Bit 0..3: Single hours in BCD-format($0-$9) Bit 4..6: Ten hours in BCD-format ($0-$5)
#define SDR 12            // $dc0c Serial Data Register
#define ICR 13            // $dc0d Interrupt Control Register
#define ICR_IRQ 0x80      // $dc0d Interrupt Control Register Bit 7 Read: 1= IRQ An interrupt occurred, so at least one bit of INT MASK and INT DATA is set in both registers.
#define ICR_S_C 0x80      // $dc0d Interrupt Control Register Bit 7 Write: Source bit. 0 = set bits 0..4 are clearing the according mask bit. 1 = set bits 0..4 are setting the according mask bit. If all bits 0..4 are cleared, there will be no change to the mask.
#define ICR_UNUSED 0x60   // $dc0d Interrupt Control Register Bit 5..6: Always 0
#define ICR_FLG 0x10      // $dc0d Interrupt Control Register Bit 4: 1 = IRQ Signal occurred at FLAG-pin (cassette port Data input, serial bus SRQ IN)
#define ICR_SP 0x08       // $dc0d Interrupt Control Register Bit 3: 1 = SDR full or empty, so full byte was transferred, depending of operating mode serial bus
#define ICR_ALRM 0x04     // $dc0d Interrupt Control Register Bit 2: 1 = Time of day and alarm time is equal
#define ICR_TB 0x02       // $dc0d Interrupt Control Register Bit 1: 1 = Underflow Timer B
#define ICR_TA 0x01       // $dc0d Interrupt Control Register Bit 0: 1 = Underflow Timer A
#define CRA 14            // $dc0e Control Register A
#define CRA_TODIN 0x80    // $dc0e Control Register A : Bit 7: Real Time Clock, 0 = 60 Hz, 1 = 50 Hz "Clock required 1:50Hz/0:60Hz on TOD pin for accurate time"
#define CRA_SPMODE 0x40   // $dc0e Control Register A : Serial Port Bit 6: Direction of the serial shift register, 0 = SP-pin is input (read), 1 = SP-pin is output (write)
#define CRA_INMODE 0x20   // $dc0e Control Register A : Timer A Bit 5: 0 = Timer counts system cycles, 1 = Timer counts positive slope at CNT-pin
#define CRA_LOAD 0x10     // $dc0e Control Register A : Timer A Bit 4: 1 = Load latch into the timer once.
#define CRA_RUNMODE 0x08  // $dc0e Control Register A : Timer A Bit 3: 0 = Timer-restart after underflow (latch will be reloaded), 1 = Timer stops after underflow.
#define CRA_OUTMODE 0x04  // $dc0e Control Register A : Timer A Bit 2: 0 = Through a timer underflow, bit 6 of port B will get high for one cycle , 1 = Through a timer underflow, bit 6 of port B will be inverted
#define CRA_PBON 0x02     // $dc0e Control Register A : Timer A Bit 1: 1 = Indicates a timer underflow at port B in bit 6.
#define CRA_START 0x01    // $dc0e Control Register A : Timer A Bit 0: 0 = Stop timer; 1 = Start timer
#define CRB 15            // $dc0f Control Register B
#define CRB_ALARM 0x80    // $dc0f Control Register B : Bit 7: 0 = Writing into the TOD registers sets the clock time, 1 = Writing into the TOD registers sets the alarm time.
#define CRB_INMODE 0x60   // $dc0f Control Register B : Timer B Bit 5..6: Counts 00:phi pulses/01:+CNT transitions/10:TimerA underflows/11:10 while CNT is high
#define CRB_LOAD 0x10     // $dc0f Control Register B : Timer B Bit 4: 1 = Load latch into the timer once.
#define CRB_RUNMODE 0x08  // $dc0f Control Register B : Timer B Bit 3: 0 = Timer-restart after underflow (latch will be reloaded), 1 = Timer stops after underflow.
#define CRB_OUTMODE 0x04  // $dc0f Control Register B : Timer B Bit 2: 0 = Through a timer underflow, bit 7 of port B will get high for one cycle , 1 = Through a timer underflow, bit 7 of port B will be inverted
#define CRB_PBON 0x02     // $dc0f Control Register B : Timer B Bit 1: 1 = Indicates a timer underflow at port B in bit 7.
#define CRB_START 0x01    // $dc0f Control Register B : Timer B Bit 0: 0 = Stop timer; 1 = Start timer

// Function declarations
void mos6526_reset(mos6526_t* cia);

// Main cycle function with unified bus state threading
bus_state_t mos6526_advance_cycle(mos6526_t* cia, bus_state_t bus_state);

// Unified register I/O
bus_state_t mos6526_read(mos6526_t* cia, bus_state_t bus_state);
bus_state_t mos6526_write(mos6526_t* cia, bus_state_t bus_state);

// PORT/PERIPHERAL DATA / DATA DIRECTION handling
void mos6526_write_data_direction_port(mos6526_t* cia, uint32_t p, uint8_t v);
void mos6526_update_output_port_b(mos6526_t* cia, uint8_t v);
void mos6526_update_output_port(mos6526_t* cia, uint32_t p, uint8_t v);
uint8_t mos6526_read_port_data(mos6526_t* cia, uint32_t p);
void mos6526_update_internal_data_direction_port_b(mos6526_t* cia, uint8_t port_b_output_mask);

// TIMER A/B handling
void mos6526_check_reload_timer(mos6526_t* cia, uint32_t t);
void mos6526_reload_timer(mos6526_t* cia, uint32_t t);
void mos6526_decrease_timer(mos6526_t* cia, uint32_t t, bool cnt_is_positive_edge, int in_mode);

// TIME OF DAY (TOD) handling
uint8_t mos6526_latch_read_tod_hr(mos6526_t* cia);
uint8_t mos6526_unlatch_read_tod_10ths(mos6526_t* cia);
uint8_t mos6526_write_tod_hr(mos6526_t* cia, uint8_t v);
void mos6526_check_alarm_interrupt(mos6526_t* cia);
void mos6526_increase_tod_and_check_alarm(mos6526_t* cia);

// SERIAL DATA REGISTER (SDR) handling
void mos6526_write_serial_data_register(mos6526_t* cia, uint8_t v);
void mos6526_serial_output(mos6526_t* cia);
void mos6526_serial_input(mos6526_t* cia);

// INTERRUPT CONTROL REGISTER (ICR) handling
uint8_t mos6526_read_and_clear_interrupt_control_register(mos6526_t* cia);
void mos6526_write_interrupt_control_register(mos6526_t* cia, uint32_t v);
void mos6526_check_interrupt_mask(mos6526_t* cia);

// CONTROL REGISTER (CRA/CRB) handling
void mos6526_write_control_register(mos6526_t* cia, uint32_t c, uint8_t v);

// Function declarations
void mos6526_cycle(mos6526_t* cia);

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI function declarations
void mos6526_render_debug_window(void* chip, bool* show_window);
void mos6526_render_settings_window(void* chip, bool* show_window);
#endif

extern chip_descriptor_t mos6526_descriptor;

#endif // MOS6526_H
