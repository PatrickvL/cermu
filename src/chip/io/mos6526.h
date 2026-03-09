#pragma once

#include "io_chip_base.h"
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
// MOS6526 CIA UNIFIED DECLARATION TABLE — single source of truth
// Bitmask constants remain separate below (in MOS6526 namespace).
// ============================================================================

// REG(offset, symbol, description)
// FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)
#define CIA_DECL(REG, FLD, CMP) \
    REG(0x00, PRA,       "Port A data")                                         \
    REG(0x01, PRB,       "Port B data")                                         \
    REG(0x02, DDRA,      "Port A direction")                                    \
    REG(0x03, DDRB,      "Port B direction")                                    \
    REG(0x04, TA_LO,     "Timer A low")                                         \
    REG(0x05, TA_HI,     "Timer A high")                                        \
    REG(0x06, TB_LO,     "Timer B low")                                         \
    REG(0x07, TB_HI,     "Timer B high")                                        \
    REG(0x08, TOD_10THS, "TOD tenths of sec")                                   \
    REG(0x09, TOD_SEC,   "TOD seconds")                                         \
    REG(0x0A, TOD_MIN,   "TOD minutes")                                         \
    REG(0x0B, TOD_HR,    "TOD hours")                                           \
    REG(0x0C, SDR,       "Serial data")                                         \
    REG(0x0D, ICR,       "Interrupt control")                                   \
      FLD(ICR, ICR_IRQ_BIT,  7:7, "IRQ active",    Flag, 0, 0)                 \
      FLD(ICR, ICR_FLG_BIT,  4:4, "FLAG pin",      Flag, 0, 0)                 \
      FLD(ICR, ICR_SP_BIT,   3:3, "Serial",        Flag, 0, 0)                 \
      FLD(ICR, ICR_ALRM_BIT, 2:2, "Alarm",         Flag, 0, 0)                 \
      FLD(ICR, ICR_TB_BIT,   1:1, "Timer B",       Flag, 0, 0)                 \
      FLD(ICR, ICR_TA_BIT,   0:0, "Timer A",       Flag, 0, 0)                 \
    REG(0x0E, CRA,       "Control reg A")                                       \
      FLD(CRA, CRA_TODIN_B,   7:7, "TOD freq (1=50Hz)",   Flag, 0, 0)          \
      FLD(CRA, CRA_SPMODE_B,  6:6, "SP mode (1=out)",     Flag, 0, 0)          \
      FLD(CRA, CRA_INMODE_A,  5:5, "TA input (1=CNT)",    Flag, 0, 0)          \
      FLD(CRA, CRA_RUNMODE_A, 3:3, "Run mode (1=oneshot)", Flag, 0, 0)         \
      FLD(CRA, CRA_OUTMODE_A, 2:2, "Out mode (1=toggle)",  Flag, 0, 0)         \
      FLD(CRA, CRA_PBON_A,    1:1, "PB6 output enable",   Flag, 0, 0)          \
      FLD(CRA, CRA_START_A,   0:0, "Timer A start",       Flag, 0, 0)          \
    REG(0x0F, CRB,       "Control reg B")                                       \
      FLD(CRB, CRB_ALARM_B,   7:7, "TOD alarm mode",      Flag, 0, 0)          \
      FLD(CRB, CRB_INMODE_B,  6:5, "TB input mode",       Value, 0, 0)         \
      FLD(CRB, CRB_RUNMODE_B, 3:3, "Run mode (1=oneshot)", Flag, 0, 0)         \
      FLD(CRB, CRB_OUTMODE_B, 2:2, "Out mode (1=toggle)",  Flag, 0, 0)         \
      FLD(CRB, CRB_PBON_B,    1:1, "PB7 output enable",   Flag, 0, 0)          \
      FLD(CRB, CRB_START_B,   0:0, "Timer B start",       Flag, 0, 0)

// Backward compat: old REG_TABLE is just the REG rows from the DECL
#define CIA_REG_TABLE(X) CIA_DECL(X, DECL_FLD_NOP, DECL_CMP_NOP)

// --- Extract file-scope address constants for FLD extractors ---
#define CIA_X_REG_CONST_(a, s, l) static constexpr uint8_t CIA_REG_##s = a;
CIA_DECL(CIA_X_REG_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
#undef CIA_X_REG_CONST_

#define CIA_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry CIA_REG_INFO[] = { CIA_DECL(CIA_X_INFO_, DECL_FLD_NOP, DECL_CMP_NOP) };
#undef CIA_X_INFO_

// --- Extract field info array ---
#define CIA_X_FLD_INFO_(reg, fld, hilo, desc, kind, ds, dm) \
    { #fld, desc, CIA_REG_##reg, BF_LO(hilo), BF_WIDTH(hilo), DataKind::kind, (uint8_t)(ds), (uint16_t)(dm) },
static constexpr FieldEntry CIA_FLD_INFO[] = {
    CIA_DECL(DECL_REG_NOP, CIA_X_FLD_INFO_, DECL_CMP_NOP)
};
#undef CIA_X_FLD_INFO_
static constexpr size_t CIA_NUM_FIELDS = sizeof(CIA_FLD_INFO) / sizeof(CIA_FLD_INFO[0]);

// --- Extract declaration order array ---
#define CIA_X_ORD_REG_(a, s, l)                                                { DeclRowType::Reg, (uint16_t)(a) },
#define CIA_X_ORD_FLD_(r, f, hilo, d, k, ds, dm)                              { DeclRowType::Field, 0 },
#define CIA_X_ORD_CMP_(s, d, k, b, ds, dm, r1, h1, d1, r2, h2, d2)           { DeclRowType::Compound, 0 },
static constexpr DeclOrderEntry CIA_DECL_ORDER_RAW[] = {
    CIA_DECL(CIA_X_ORD_REG_, CIA_X_ORD_FLD_, CIA_X_ORD_CMP_)
};
#undef CIA_X_ORD_REG_
#undef CIA_X_ORD_FLD_
#undef CIA_X_ORD_CMP_
static constexpr auto CIA_DECL_ORDER = assign_decl_indices(CIA_DECL_ORDER_RAW);

struct mos6526_t : public IoChipBase {
    mos6526_t() : IoChipBase(ChipInfo{"MOS6526", "MOS Technology"}) {
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

        debug_registry_.set_registers(reg, CIA_REGS_SIZE, CIA_REG_INFO);
        debug_registry_.set_decl_order(CIA_DECL_ORDER.data(), CIA_DECL_ORDER.size(),
                                       CIA_FLD_INFO, CIA_NUM_FIELDS,
                                       nullptr, 0, nullptr);

        // ICR/CRA/CRB bitfields and TOD/SDR register values are in the DECL walk.
        // Port visualization, live timers, and internal interrupt mask remain.

        // --- Data Ports ---
        debug_registry_.category("Data Ports")
            .port("Port A",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[0]; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[2]; })
            .port("Port B",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[1]; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->reg[3]; });

        // --- Timers (live counters/latches — not in register mirror) ---
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

        // --- Interrupt (internal delayed mask not in register) ---
        debug_registry_.category("Interrupt Control")
            .value("Interrupt Mask", +[](const ChipBase* c) -> uint32_t { return static_cast<CI*>(c)->interrupt_mask; });
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


