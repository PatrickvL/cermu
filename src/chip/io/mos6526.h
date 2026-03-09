#pragma once

#include "../../core/chip.h"
//#include "../../core/bus_cycle_interface.h"
#include <cstdint>
#include "../../core/system_lines.h" // For bus_state_t
#include "../../core/ioport.h"       // For io_port<Mask>
#include <cstdint>


#include "../../utils/shift_register.hpp" // For delay line implementation

// CIA MOS 6526 DIP has 40 pins; Pinout :
enum mos6526_pin_t {
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
};

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

// ============================================================================
// MOS6526 CIA REGISTER TABLE — single source of truth (address constants only)
// Bitmask constants remain separate below.
// ============================================================================

// X(addr, symbol, description)
#define CIA_REG_TABLE(X) \
    X(0x00, PRA,       "Port A data")           \
    X(0x01, PRB,       "Port B data")           \
    X(0x02, DDRA,      "Port A direction")      \
    X(0x03, DDRB,      "Port B direction")      \
    X(0x04, TA_LO,     "Timer A low")           \
    X(0x05, TA_HI,     "Timer A high")          \
    X(0x06, TB_LO,     "Timer B low")           \
    X(0x07, TB_HI,     "Timer B high")          \
    X(0x08, TOD_10THS, "TOD tenths of sec")     \
    X(0x09, TOD_SEC,   "TOD seconds")           \
    X(0x0A, TOD_MIN,   "TOD minutes")           \
    X(0x0B, TOD_HR,    "TOD hours")             \
    X(0x0C, SDR,       "Serial data")           \
    X(0x0D, ICR,       "Interrupt control")     \
    X(0x0E, CRA,       "Control reg A")         \
    X(0x0F, CRB,       "Control reg B")

#define CIA_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry CIA_REG_INFO[] = { CIA_REG_TABLE(CIA_X_INFO_) };
#undef CIA_X_INFO_

struct mos6526_t : public ChipBase {
    mos6526_t() : ChipBase(ChipInfo{"MOS6526", "MOS Technology"}) {
        category_ = "I/O";
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    uint8_t configured_interrupt_bit = 0; // BUS_IRQ_BIT for CIA1, BUS_NMI_BIT for CIA2
    
    // CIA ports, timers, alarm, registers, latches, interrupt and other status variables.
    uint8_t port_a_value = 0;
    uint8_t port_b_value = 0;
    int cycles_tod[2] = {}; // Assigned once in constructor
    uint8_t reg[CIA_REGS_SIZE + 4 + 4 + 4 + 1 + 1] = {}; // Registers, plus TIMER, CLOCK, ALARM, SDR and DDRB latches

    // Generic IO port views for DDR/data/pins mechanics
    // Register indices: PRA=0, PRB=1, DDRA=2, DDRB=3 (constants defined in MOS6526 namespace below)
    io_port<0xFF> port_a{reg[2], reg[0], port_a_value};  // DDR→DDRA, data→PRA, pins→port_a_value
    io_port<0xFF> port_b{reg[3], reg[1], port_b_value};  // DDR→DDRB, data→PRB, pins→port_b_value

    uint32_t read_tod_delta = 0;
    uint32_t write_tod_delta = 0;
    bool is_running_tod = false;
    int tod_cycles = 0;
    int tod_tick_counter = 0;  // Power-line divider: counts 50/60Hz ticks, fires TOD at 10Hz
    
    // --- Serial shift register (VICE-style delay pipeline) ---
    uint32_t sdr_delay = 0;        // VICE-style software delay line for SDR timing
    uint16_t shifter = 0;          // 16-bit shift register (output bit at bit 8)
    uint8_t sr_bits = 0;           // Shifts remaining: 16→0 (pairs of half-cycles)
    bool sdr_valid = false;        // SDR has new data buffered for next byte
    bool cnt_output_state = false; // CNT output flip-flop (toggled by Timer A underflow, delayed)
    bool sp_output_bit = false;    // Current SP output bit value (driven during serial output)
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

    // Native 16-bit timer counters — avoids per-cycle byte reassembly/split.
    // Canonical source of truth; byte registers synced lazily on CPU read.
    uint16_t timer_counter_[2] = {0xFFFF, 0xFFFF};
    
    // PB6/PB7 toggle flip-flops (per CIA6526.txt lines 104-110)
    // Set HIGH on rising edge of START bit, toggle on each underflow
    // Used when PBON=1 and OUTMODE=1 (toggle mode)
    uint8_t pb67_toggle = 0;  // Bit 6 = PB6 toggle state, Bit 7 = PB7 toggle state
    
    // Bus line control for interrupt delay implementation
    // This mask is applied at the START of each tick to pull lines LOW (assert)
    // Updated at the END of the tick based on pending interrupts
    // Implements the required 1-cycle delay for interrupt assertion
    bus_state_t pending_bus_lines = 0;  // Lines to assert in NEXT cycle
    // bus_snapshot_ is inherited from ChipBase — used for edge detection
    // and GUI pin rendering.  No local shadow needed.
    
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
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override;
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif

    // --- Public methods ---
    void reset();
    bus_state_t tick(bus_state_t bus_state);
    bus_state_t tick_phi2(bus_state_t bus_state);
    bus_state_t tick_phi1(bus_state_t bus_state);

    // Static methods for function pointer table compatibility (io_page_handlers_t)
    static bus_state_t registers_read(void* context, bus_state_t bus_state);
    static bus_state_t registers_write(void* context, bus_state_t bus_state);

private:
    // --- Internal helpers ---
    uint8_t read_and_clear_interrupt_control_register();
    void check_interrupt_mask();
    void write_interrupt_control_register(uint32_t v);
    void write_data_direction_port(uint32_t p, uint8_t v);
    void update_output_port(uint32_t p, uint8_t v);
    void update_output_port_b(uint8_t v);
    uint8_t read_port_data(uint32_t p);
    void update_internal_data_direction_port_b(uint8_t port_b_output_mask);
    void reload_timer(uint32_t t);
    void check_reload_timer(uint32_t t);
    void decrease_timer(uint32_t t);
    void write_control_register(uint32_t c, uint8_t v);
    uint8_t latch_read_tod_hr();
    uint8_t unlatch_read_tod_10ths();
    uint8_t write_tod_hr(uint8_t v);
    void check_alarm_interrupt();
    uint8_t bcd_inc(uint32_t r);
    void increase_tod_and_check_alarm();
    void write_serial_data_register(uint8_t v);
    void process_sdr_pipeline();  // Process SDR delay pipeline each tick

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using CI = const mos6526_t;
        static constexpr const char* icr_labels[] = {
            "IRQ", "unused", "unused", "FLG", "SP", "ALRM", "TB", "TA"
        };

        debug_registry_.set_registers(reg, CIA_REGS_SIZE, CIA_REG_INFO);

        // --- Data Ports ---
        // PRA=reg[0], PRB=reg[1], DDRA=reg[2], DDRB=reg[3]
        debug_registry_.category("Data Ports")
            .port("Port A",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[0]; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[2]; })
            .port("Port B",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[1]; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[3]; });

        // --- Timers ---
        // Timer latches: TIMER_OFFSET = 16 - 4 = 12, so latch A at reg[16..17], latch B at reg[18..19]
        debug_registry_.category("Timers")
            .timer("Timer A",
                   +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->timer_counter_[0]; },
                   +[](const ChipBase* c) -> uint32_t {
                       return (static_cast<CI*>(c)->reg[17] << 8) | static_cast<CI*>(c)->reg[16]; },
                   +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[14] & 0x01; })
            .timer("Timer B",
                   +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->timer_counter_[1]; },
                   +[](const ChipBase* c) -> uint32_t {
                       return (static_cast<CI*>(c)->reg[19] << 8) | static_cast<CI*>(c)->reg[18]; },
                   +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[15] & 0x01; });

        // --- TOD Clock ---
        // TOD_10THS=8, TOD_SEC=9, TOD_MIN=10, TOD_HR=11
        debug_registry_.category("Time of Day Clock", false)
            .value("TOD 10ths",   static_cast<uint16_t>(8))
            .value("TOD Seconds", static_cast<uint16_t>(9))
            .value("TOD Minutes", static_cast<uint16_t>(10))
            .value("TOD Hours",   static_cast<uint16_t>(11));

        // --- Interrupt Control ---
        // ICR=reg[13]
        debug_registry_.category("Interrupt Control")
            .bitfield("ICR", +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[13]; }, 8, icr_labels)
            .value("Interrupt Mask", +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->interrupt_mask; })
            .flag("IRQ Active", +[](const ChipBase* c) -> uint32_t { return (static_cast<CI*>(c)->reg[13] & 0x80) ? 1u : 0u; });

        // --- Serial Data ---
        // SDR=reg[12]
        debug_registry_.category("Serial Data", false)
            .value("SDR", static_cast<uint16_t>(12));

        // --- Control Registers ---
        // CRA=reg[14], CRB=reg[15]
        debug_registry_.category("Control Registers", false)
            .value("CRA", static_cast<uint16_t>(14))
            .value("CRB", static_cast<uint16_t>(15));
    }
#endif
};

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

    // MOS6526 CIA Register Definitions — address constants from X-macro
    #define CIA_X_CONST_(a, s, l) constexpr uint8_t s = a;
    CIA_REG_TABLE(CIA_X_CONST_)
    #undef CIA_X_CONST_

    // Bitmask constants (not part of the X-macro address table)
    constexpr uint8_t TOD_10THS_MASK = 0x0F; // TOD 10ths: BCD mask
    constexpr uint8_t TOD_SEC_MASK = 0x7F;   // TOD Seconds: BCD mask
    constexpr uint8_t TOD_MIN_MASK = 0x7F;   // TOD Minutes: BCD mask
    constexpr uint8_t TOD_HR_PM = 0x80;      // TOD Hours: AM/PM bit
    constexpr uint8_t TOD_HR_MASK = 0x1F;    // TOD Hours: BCD mask

    constexpr uint8_t ICR_IRQ = 0x80;        // ICR bit 7: IRQ occurred
    constexpr uint8_t ICR_S_C = 0x80;        // ICR bit 7 write: source bit
    constexpr uint8_t ICR_UNUSED = 0x60;     // ICR bits 5-6: always 0
    constexpr uint8_t ICR_FLG = 0x10;        // ICR bit 4: FLAG pin
    constexpr uint8_t ICR_SP = 0x08;         // ICR bit 3: serial data
    constexpr uint8_t ICR_ALRM = 0x04;       // ICR bit 2: alarm
    constexpr uint8_t ICR_TB = 0x02;         // ICR bit 1: Timer B underflow
    constexpr uint8_t ICR_TA = 0x01;         // ICR bit 0: Timer A underflow
    
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


