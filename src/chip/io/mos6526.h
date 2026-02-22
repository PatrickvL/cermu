#pragma once

#include "../../core/chip.h"
//#include "../../core/bus_cycle_interface.h"
#include <stdint.h>
#include "../../core/system_lines.h" // For bus_state_t
#include <stdint.h>
#include <stdbool.h>
#include <stdbool.h>
#include "../../utils/shift_register.hpp" // For delay line implementation

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

typedef struct mos6526_s : public ChipBase {
    uint8_t configured_interrupt_bit = 0; // BUS_IRQ_BIT for CIA1, BUS_NMI_BIT for CIA2
    
    // CIA ports, timers, alarm, registers, latches, interrupt and other status variables.
    uint8_t port_a_value = 0;
    uint8_t port_b_value = 0;
    int cycles_tod[2] = {}; // Assigned once in constructor
    uint8_t reg[CIA_REGS_SIZE + 4 + 4 + 4 + 1 + 1] = {}; // Registers, plus TIMER, CLOCK, ALARM, SDR and DDRB latches
    uint32_t read_tod_delta = 0;
    uint32_t write_tod_delta = 0;
    bool is_running_tod = false;
    int tod_cycles = 0;
    int serial_shift = 0;
    bool cnt_output_state = false;  // CNT flip-flop for serial output mode (toggled by Timer A underflow)
    bool sp_output_bit = false;     // Current SP output bit value (driven during serial output)
    uint8_t interrupt_mask = 0;
    uint8_t interrupt_mask_delayed = 0;  // 1-cycle delay for interrupt mask updates (IMR → IMR1)
    
    // TOD alarm state for edge detection (prevents retriggering alarm every cycle)
    // Per chips_mos6526.hpp: Only trigger alarm interrupt on rising edge
    bool prev_alarm_state = false;
    
    // Timer B Bug: Track ICR reads to block Timer B interrupt generation
    // Per chips_mos6526.hpp lines 476-477: "Timer B Bug" implementation
    bool icr_read_this_cycle = false;
    
    // Timer underflow event tracking (one-cycle pulse, NOT the persistent ICR flag)
    // Used for Timer B cascade mode (counts Timer A underflows) and PB6/PB7 pulse output.
    // Set TRUE during the late tick when underflow occurs, cleared at start of next late tick.
    uint8_t timer_underflowed = 0;
    
    // PB6/PB7 toggle flip-flops (per CIA6526.txt lines 104-110)
    // Set HIGH on rising edge of START bit, toggle on each underflow
    // Used when PBON=1 and OUTMODE=1 (toggle mode)
    uint8_t pb67_toggle = 0;  // Bit 6 = PB6 toggle state, Bit 7 = PB7 toggle state
    
    // Bus line control for interrupt delay implementation
    // This mask is applied at the START of each tick to pull lines LOW (assert)
    // Updated at the END of the tick based on pending interrupts
    // Implements the required 1-cycle delay for interrupt assertion
    bus_state_t pending_bus_lines = 0;  // Lines to assert in NEXT cycle
    
    // Previous bus state for edge detection
    // Stored at end of each tick to detect signal transitions in next cycle
    // This is the standard pattern all chips should use for edge detection
    bus_state_t prev_bus_state = 0;
    
    // Callback for port A output changes (used by CIA2 for VIC-II bank switching)
    void (*port_a_change_callback)(void* context, uint8_t port_a_output) = nullptr;
    void* port_a_callback_context = nullptr;
    
    // Callbacks for port input reads (used by CIA1 for keyboard matrix scanning)
    // These callbacks allow external devices (keyboard, joystick) to pull port lines LOW
    // Called when CIA reads from port to get external device state
    uint8_t (*port_a_read_callback)(void* context, uint8_t port_a_output) = nullptr;
    void* port_a_read_context = nullptr;
    uint8_t (*port_b_read_callback)(void* context, uint8_t port_b_output) = nullptr;
    void* port_b_read_context = nullptr;

    // Multi-cycle delay line using StaticShiftRegister for cycle-accurate timing
    // Configuration: TA_COUNT(3), TB_COUNT(3), TA_LOAD(2), TB_LOAD(2),
    //                ONESHOT_A(2), ONESHOT_B(2), CNT_SWITCH_A(2), CNT_SWITCH_B(2)
    // Count pipes use 3 bits (matching chips_mos6526.hpp reference: 2-cycle delay).
    // Pipe indices: 0=TA_COUNT, 1=TB_COUNT, 2=TA_LOAD, 3=TB_LOAD,
    //               4=ONESHOT_A, 5=ONESHOT_B, 6=CNT_SWITCH_A, 7=CNT_SWITCH_B
    using DelayLine = StaticShiftRegister<uint64_t, 3, 3, 2, 2, 2, 2, 2, 2>;
    DelayLine delay_line;

    // --- ChipBase interface ---
    ChipIdentity chip_identity() const override;
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;
} mos6526_t;

namespace MOS6526 {
    // Zero-storage pipe type-tags for delay line access (compile-time only)
    // These are passed to Inject/Check/Clear methods for type-safe pipe access
    inline constexpr mos6526_t::DelayLine::Pipe<0> ta_count_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<1> tb_count_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<2> ta_load_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<3> tb_load_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<4> oneshot_a_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<5> oneshot_b_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<6> cnt_switch_a_pipe;
    inline constexpr mos6526_t::DelayLine::Pipe<7> cnt_switch_b_pipe;

    // MOS6526 CIA Register Definitions
    
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
    constexpr uint8_t CRB = 15;            // $dc0f Control Register B
    
    // Generic Control Register bit definitions (shared between CRA and CRB)
    constexpr uint8_t CR_LOAD = 0x10;      // Bit 4: 1 = Load latch into the timer once (strobe)
    constexpr uint8_t CR_RUNMODE = 0x08;   // Bit 3: 0 = continuous mode, 1 = one-shot mode (stop after underflow)
    constexpr uint8_t CR_OUTMODE = 0x04;   // Bit 2: 0 = pulse mode (high for one cycle), 1 = toggle mode (invert on underflow)
    constexpr uint8_t CR_PBON = 0x02;      // Bit 1: 1 = Timer output appears on PB6/PB7
    constexpr uint8_t CR_START = 0x01;     // Bit 0: 0 = Stop timer, 1 = Start timer
    
    // Control Register A specific bits
    constexpr uint8_t CRA_TODIN = 0x80;    // Bit 7: TOD frequency, 0 = 60 Hz, 1 = 50 Hz
    constexpr uint8_t CRA_SPMODE = 0x40;   // Bit 6: Serial port direction, 0 = input, 1 = output
    constexpr uint8_t CRA_INMODE = 0x20;   // Bit 5: Timer A input, 0 = PHI2, 1 = CNT pin
    constexpr uint8_t CRA_LOAD = CR_LOAD;  // Bit 4: Load latch (alias to generic)
    constexpr uint8_t CRA_RUNMODE = CR_RUNMODE;  // Bit 3: Run mode (alias to generic)
    constexpr uint8_t CRA_OUTMODE = CR_OUTMODE;  // Bit 2: Output mode (alias to generic)
    constexpr uint8_t CRA_PBON = CR_PBON;  // Bit 1: PB6 output enable (alias to generic)
    constexpr uint8_t CRA_START = CR_START;  // Bit 0: Start/stop (alias to generic)
    
    // Control Register B specific bits
    constexpr uint8_t CRB_ALARM = 0x80;    // Bit 7: TOD mode, 0 = set time, 1 = set alarm
    constexpr uint8_t CRB_INMODE = 0x60;   // Bit 5-6: Timer B input mode (00=PHI2, 01=CNT, 10=Timer A, 11=Timer A+CNT)
    constexpr uint8_t CRB_LOAD = CR_LOAD;  // Bit 4: Load latch (alias to generic)
    constexpr uint8_t CRB_RUNMODE = CR_RUNMODE;  // Bit 3: Run mode (alias to generic)
    constexpr uint8_t CRB_OUTMODE = CR_OUTMODE;  // Bit 2: Output mode (alias to generic)
    constexpr uint8_t CRB_PBON = CR_PBON;  // Bit 1: PB7 output enable (alias to generic)
    constexpr uint8_t CRB_START = CR_START;  // Bit 0: Start/stop (alias to generic)
}

// Using declarations to maintain compatibility in MOS6526 implementation files
using namespace MOS6526;

// Export generic control register bits to global scope for convenience
using MOS6526::CR_START;
using MOS6526::CR_LOAD;
using MOS6526::CR_RUNMODE;
using MOS6526::CR_OUTMODE;
using MOS6526::CR_PBON;

// Function declarations
void mos6526_reset(mos6526_t* cia);

// Register I/O functions (used directly in chip descriptor)
bus_state_t mos6526_registers_read(void* context, bus_state_t bus_state);
bus_state_t mos6526_registers_write(void* context, bus_state_t bus_state);

// Split CIA tick into two phases for cycle-accurate timer reads:
// - tick_phi2: Apply pending interrupts (before CPU samples IRQ/NMI)
// - tick_phi1: Timer counting, interrupt generation (after CPU register reads)
// This ensures CPU reads see the pre-decrement timer value (matching real hardware).
bus_state_t mos6526_tick_phi2(void* chip, bus_state_t bus_state);
bus_state_t mos6526_tick_phi1(void* chip, bus_state_t bus_state);

// Legacy single-phase tick (calls early+late in sequence, for non-C64 systems)
bus_state_t mos6526_tick(void* chip, bus_state_t bus_state);

#ifdef IMGUI_VERSION
// Legacy GUI wrappers (C-linkage, for c64.cpp test path)
void mos6526_render_debug_window(void* chip, bool* show_window);
void mos6526_render_settings_window(void* chip, bool* show_window);
#endif

// Typed lifecycle functions
mos6526_t* mos6526_create();
void mos6526_destroy(mos6526_t* cia);


