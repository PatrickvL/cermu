#pragma once

#include "../../core/chip.h"
//#include "../../core/bus_cycle_interface.h"
#include <stdint.h>
#include "../../core/system_lines.h" // For bus_state_t
#include <stdint.h>
#include <stdbool.h>
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
#define CIA_REGS_BITS 4
#define CIA_REGS_SIZE (1 << CIA_REGS_BITS) // 16
#define CIA_REGS_MASK (CIA_REGS_SIZE - 1)  // 15

// Constants
#define A 0
#define B 1
#define PB6_MASK (1 << 6)
#define PB7_MASK (1 << 7)
#define MASK5 0x1F

// Additional Reg offsets above the 0..15 register range:
#define TIMER_OFFSET (16 - TA_LO) // Delta on TA_LO to TB_HI so Timer write latch resides at 16..19
#define CLOCK_OFFSET (20 - TOD_10THS) // Delta on TOD_10THS to TOD_HR so TOD read latch resides at 20..23
#define ALARM_OFFSET (24 - TOD_10THS) // Delta on TOD_10THS to TOD_HR so Alarm write latch resides at 24..27
#define SHIFT_OFFSET 28 // Delta on SDR so Serial Data Shift register resides at 28
#define IDDRB_OFFSET 29 // Internal Data Direction of Port B (a version of DDRB which includes the PBON mask)

typedef struct mos6526_s {
    chip_descriptor_t* desc;
    int interrupt_line; // BUS_MASK_IRQ for CIA1, BUS_MASK_NMI for CIA2
    
    // CIA ports, timers, alarm, registers, latches, interrupt and other status variables.
    uint8_t port_a_value;
    uint8_t port_b_value;
    int cycles_tod[2]; // Assigned once in constructor
    uint8_t reg[CIA_REGS_SIZE + 4 + 4 + 4 + 1 + 1]; // Registers, plus TIMER, CLOCK, ALARM, SDR and DDRB latches
    uint32_t read_tod_delta;
    uint32_t write_tod_delta;
    bool is_running_tod;
    int tod_cycles;
    int serial_shift;
    uint8_t interrupt_mask;
    
    // Bus line control for interrupt delay implementation
    // This mask is applied at the START of each tick to pull lines LOW (assert)
    // Updated at the END of the tick based on pending interrupts
    // Implements the required 1-cycle delay for interrupt assertion
    bus_state_t pending_bus_lines;  // Lines to assert in NEXT cycle
    
    // Callback for port A output changes (used by CIA2 for VIC-II bank switching)
    void (*port_a_change_callback)(void* context, uint8_t port_a_output);
    void* port_a_callback_context;
    
    // Callbacks for port input reads (used by CIA1 for keyboard matrix scanning)
    // These callbacks allow external devices (keyboard, joystick) to pull port lines LOW
    // Called when CIA reads from port to get external device state
    uint8_t (*port_a_read_callback)(void* context, uint8_t port_a_output);
    void* port_a_read_context;
    uint8_t (*port_b_read_callback)(void* context, uint8_t port_b_output);
    void* port_b_read_context;
} mos6526_t;

// MOS6526 CIA Register Definitions
namespace MOS6526 {
    // Technical register indices (in decimal) and masks (in hexadecimal)
    constexpr uint8_t PRA = 0;             // $dc00 Peripheral Data Reg A Monitoring/control of the 8 data lines of Port A.
    constexpr uint8_t PRB = 1;             // $dc01 Peripheral Data Reg B Monitoring/control of the 8 data lines of Port B.
    constexpr uint8_t DDRA = 2;            // $dc02 Data Direction Register A Bit X: 0=Input (read only), 1=Output (read and write)
    constexpr uint8_t DDRB = 3;            // $dc03 Data Direction Register B Bit X: 0=Input (read only), 1=Output (read and write)
    constexpr uint8_t TA_LO = 4;           // $dc04 Timer A Low Register Read: actual value Timer A (Low Byte) Writing: Set latch of Timer A (Low Byte)
    constexpr uint8_t TA_HI = 5;           // $dc05 Timer A High Register Read: actual value Timer A (High Byte) Writing: Set latch of timer A (High Byte) - if the timer is stopped, the high-byte will automatically be re-set as well
    constexpr uint8_t TB_LO = 6;           // $dc06 Timer B Low Register Read: actual value Timer B (Low Byte) Writing: Set latch of Timer B (Low Byte)
    constexpr uint8_t TB_HI = 7;           // $dc07 Timer B High Register Read: actual value Timer B (High Byte) Writing: Set latch of timer B (High Byte) - if the timer is stopped, the high-byte will automatically be re-set as well
    constexpr uint8_t TOD_10THS = 8;       // $dc08 Real Time Clock 10ths Of Seconds
    constexpr uint8_t TOD_10THS_MASK = 0x0F; // $dc08 Real Time Clock 10ths Of Seconds Bit 0..3: Tenth seconds in BCD-format($0-$9) Bit 4..7: always 0
    constexpr uint8_t TOD_SEC = 9;         // $dc09 Real Time Clock Seconds
    constexpr uint8_t TOD_SEC_MASK = 0x7F; // $dc0a Real Time Clock Seconds Bit 0..3: Single seconds in BCD-format( $0-$9) Bit 4..6: Ten seconds in BCD-format ($0-$5) Bit 7: always 0
    constexpr uint8_t TOD_MIN = 10;        // $dc0a Real Time Clock Minutes
    constexpr uint8_t TOD_MIN_MASK = 0x7F; // $dc0a Real Time Clock Minutes Bit 0..3: Single minutes in BCD-format( $0-$9) Bit 4..6: Ten minutes in BCD-format ($0-$5) Bit 7: always 0
    constexpr uint8_t TOD_HR = 11;         // $dc0b Real Time Clock Hours
    constexpr uint8_t TOD_HR_PM = 0x80;    // $dc0b Real Time Clock Hours - Bit 7 Read: Differentiation AM/PM, 0=AM, 1=PM Writing into this register stops TOD, until register 8 (TOD 10THS) will be read.
    constexpr uint8_t TOD_HR_MASK = 0x1F;  // $dc0b Real Time Clock Hours - Bit 0..3: Single hours in BCD-format($0-$9) Bit 4..6: Ten hours in BCD-format ($0-$5)
    constexpr uint8_t SDR = 12;            // $dc0c Serial Data Register
    constexpr uint8_t ICR = 13;            // $dc0d Interrupt Control Register
    constexpr uint8_t ICR_IRQ = 0x80;      // $dc0d Interrupt Control Register Bit 7 Read: 1= IRQ An interrupt occurred, so at least one bit of INT MASK and INT DATA is set in both registers.
    constexpr uint8_t ICR_S_C = 0x80;      // $dc0d Interrupt Control Register Bit 7 Write: Source bit. 0 = set bits 0..4 are clearing the according mask bit. 1 = set bits 0..4 are setting the according mask bit. If all bits 0..4 are cleared, there will be no change to the mask.
    constexpr uint8_t ICR_UNUSED = 0x60;   // $dc0d Interrupt Control Register Bit 5..6: Always 0
    constexpr uint8_t ICR_FLG = 0x10;      // $dc0d Interrupt Control Register Bit 4: 1 = IRQ Signal occurred at FLAG-pin (cassette port Data input, serial bus SRQ IN)
    constexpr uint8_t ICR_SP = 0x08;       // $dc0d Interrupt Control Register Bit 3: 1 = SDR full or empty, so full byte was transferred, depending of operating mode serial bus
    constexpr uint8_t ICR_ALRM = 0x04;     // $dc0d Interrupt Control Register Bit 2: 1 = Time of day and alarm time is equal
    constexpr uint8_t ICR_TB = 0x02;       // $dc0d Interrupt Control Register Bit 1: 1 = Underflow Timer B
    constexpr uint8_t ICR_TA = 0x01;       // $dc0d Interrupt Control Register Bit 0: 1 = Underflow Timer A
    constexpr uint8_t CRA = 14;            // $dc0e Control Register A
    constexpr uint8_t CRA_TODIN = 0x80;    // $dc0e Control Register A : Bit 7: Real Time Clock, 0 = 60 Hz, 1 = 50 Hz "Clock required 1:50Hz/0:60Hz on TOD pin for accurate time"
    constexpr uint8_t CRA_SPMODE = 0x40;   // $dc0e Control Register A : Serial Port Bit 6: Direction of the serial shift register, 0 = SP-pin is input (read), 1 = SP-pin is output (write)
    constexpr uint8_t CRA_INMODE = 0x20;   // $dc0e Control Register A : Timer A Bit 5: 0 = Timer counts system cycles, 1 = Timer counts positive slope at CNT-pin
    constexpr uint8_t CRA_LOAD = 0x10;     // $dc0e Control Register A : Timer A Bit 4: 1 = Load latch into the timer once.
    constexpr uint8_t CRA_RUNMODE = 0x08;  // $dc0e Control Register A : Timer A Bit 3: 0 = Timer-restart after underflow (latch will be reloaded), 1 = Timer stops after underflow.
    constexpr uint8_t CRA_OUTMODE = 0x04;  // $dc0e Control Register A : Timer A Bit 2: 0 = Through a timer underflow, bit 6 of port B will get high for one cycle , 1 = Through a timer underflow, bit 6 of port B will be inverted
    constexpr uint8_t CRA_PBON = 0x02;     // $dc0e Control Register A : Timer A Bit 1: 1 = Indicates a timer underflow at port B in bit 6.
    constexpr uint8_t CRA_START = 0x01;    // $dc0e Control Register A : Timer A Bit 0: 0 = Stop timer; 1 = Start timer
    constexpr uint8_t CRB = 15;            // $dc0f Control Register B
    constexpr uint8_t CRB_ALARM = 0x80;    // $dc0f Control Register B : Bit 7: 0 = Writing into the TOD registers sets the clock time, 1 = Writing into the TOD registers sets the alarm time.
    constexpr uint8_t CRB_INMODE = 0x60;   // $dc0f Control Register B : Timer B Bit 5..6: Counts 00:phi pulses/01:+CNT transitions/10:TimerA underflows/11:10 while CNT is high
    constexpr uint8_t CRB_LOAD = 0x10;     // $dc0f Control Register B : Timer B Bit 4: 1 = Load latch into the timer once.
    constexpr uint8_t CRB_RUNMODE = 0x08;  // $dc0f Control Register B : Timer B Bit 3: 0 = Timer-restart after underflow (latch will be reloaded), 1 = Timer stops after underflow.
    constexpr uint8_t CRB_OUTMODE = 0x04;  // $dc0f Control Register B : Timer B Bit 2: 0 = Through a timer underflow, bit 7 of port B will get high for one cycle , 1 = Through a timer underflow, bit 7 of port B will be inverted
    constexpr uint8_t CRB_PBON = 0x02;     // $dc0f Control Register B : Timer B Bit 1: 1 = Indicates a timer underflow at port B in bit 7.
    constexpr uint8_t CRB_START = 0x01;    // $dc0f Control Register B : Timer B Bit 0: 0 = Stop timer; 1 = Start timer
}

// Using declarations to maintain compatibility in MOS6526 implementation files
using namespace MOS6526;

// Function declarations
void mos6526_reset(mos6526_t* cia);

// Register I/O functions (used directly in chip descriptor)
bus_state_t mos6526_registers_read(void* context, bus_state_t bus_state);
bus_state_t mos6526_registers_write(void* context, bus_state_t bus_state);

// Main CIA tick function - entry point for cycle processing
bus_state_t mos6526_tick(void* chip, bus_state_t bus_state);

#ifdef IMGUI_VERSION
// GUI function declarations
void mos6526_render_debug_window(void* chip, bool* show_window);
void mos6526_render_settings_window(void* chip, bool* show_window);
#endif

extern chip_descriptor_t mos6526_descriptor;


