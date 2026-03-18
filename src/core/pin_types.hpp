/*
 * pin_types.h - Pin type definitions and enumerations
 * 
 * Core pin type definitions used across all IC packages and chip types.
 * Includes pin types, labels, sides, and state structures.
 */

#pragma once

#include <cstdint>
#include <string>
#include "core/system_lines.hpp"

// ============================================================================
// PIN TYPES AND ENUMERATIONS
// ============================================================================

// Pin types for visual categorization and color coding
enum class PinType {
    POWER,        // VCC, VSS, VDD, GND, AVDD, DVDD, etc.
    CLOCK,        // Clock inputs/outputs (φ0, φ1, φ2, CLK, XTAL, etc.)
    ADDRESS,      // Address bus lines (A0-A23, ADDR, etc.)
    DATA,         // Data bus lines (D0-D15, DATA, etc.)
    CONTROL,      // Control signals (RW, SYNC, RDY, AEC, CS, OE, WE, etc.)
    INTERRUPT,    // Interrupt lines (IRQ, NMI, RES, INT, INTR, etc.)
    SPECIAL,      // Special purpose (SO, BE, ML, NC, TEST, etc.)
    IO_PORT,      // I/O port lines (P0-P7, PA0-PA7, GPIO, etc.)
    PORT_A,       // Port A specific GPIO (PA0-PA7)
    PORT_B,       // Port B specific GPIO (PB0-PB7)
    PORT_C,       // Port C specific GPIO (PC0-PC7)
    PORT_D,       // Port D specific GPIO (PD0-PD7)
    ANALOG,       // Analog signals (AIN, AOUT, VREF, etc.)
    DIFFERENTIAL, // Differential pairs (TX+/TX-, RX+/RX-, CLK+/CLK-)
    VIDEO,        // Video signals (LUMA, CHROMA, SYNC, etc.)
    AUDIO,        // Audio signals (AUDIO_OUT, OSC, FILTER, etc.)
    MEMORY,       // Memory interface (DQ, MA, CAS, RAS, etc.)
    LOGIC,        // Logic signals (Q, Y, S, G, etc.)
    SERIAL,       // Serial communication (UART, SPI, etc.)
    TIMER,        // Timer/counter signals (CNT, TOD, FLAG, etc.)
    NO_CONNECT    // Explicitly no-connect pins
};

// Pin label enumeration for 65xx CPU family and common pins.
//
// Active-low (inverted) labels appear first, prefixed with underscore.
// Detect active-low via: label < PinLabel::ACTIVE_LOW_END
//
enum class PinLabel {
    // ================================================================
    // Active-low (inverted) pins — sorted alphabetically.
    // These correspond to signals that are default-high / active-low.
    // The string representation omits the underscore; the display
    // layer prepends "/" or overbar based on notation style.
    // ================================================================
    _ABORT,       // /ABORT — abort (65C816)
    _AEC,         // /AEC — address enable control (active-low form)
    _AS,          // /AS — address strobe (M68K)
    _BASIC,       // /BASIC — BASIC ROM select
    _BERR,        // /BERR — bus error (M68K)
    _BG,          // /BG — bus grant (M68K)
    _BGACK,       // /BGACK — bus grant acknowledge (M68K)
    _BR,          // /BR — bus request (M68K)
    _BUSAK,       // /BUSAK — bus acknowledge (Z80)
    _BUSRQ,       // /BUSRQ — bus request (Z80)
    _CAS,         // /CAS — column address strobe
    _CASRAM_PLA,  // /CASRAM — CAS RAM (PLA output F0)
    _CE,          // /CE — chip enable
    _CHAREN,      // /CHAREN — character ROM enable
    _CHAROM,      // /CHAROM — character ROM select
    _CS,          // /CS — chip select
    _CS0,         // /CS0 — chip select 0
    _CS1,         // /CS1 — chip select 1
    _CS2,         // /CS2 — chip select 2
    _DTACK,       // /DTACK — data transfer acknowledge (M68K)
    _EXROM,       // /EXROM — external ROM
    _GAME,        // /GAME — game line
    _HALT,        // /HALT — halt (Z80)
    _HIRAM,       // /HIRAM — high RAM
    _INT,         // /INT — interrupt (Z80)
    _IO,          // /I/O — I/O area select
    _IORQ,        // /IORQ — I/O request (Z80)
    _IPL0,        // /IPL0 — interrupt priority level 0 (M68K)
    _IPL1,        // /IPL1 — interrupt priority level 1 (M68K)
    _IPL2,        // /IPL2 — interrupt priority level 2 (M68K)
    _IRQ,         // /IRQ — interrupt request
    _IRQA,        // /IRQA — interrupt request A (PIA 6820/6821)
    _IRQB,        // /IRQB — interrupt request B (PIA 6820/6821)
    _KERNAL,      // /KERNAL — KERNAL ROM select
    _LDS,         // /LDS — lower data strobe (M68K)
    _LORAM,       // /LORAM — low RAM
    _M1,          // /M1 — machine cycle 1 (Z80)
    _ML,          // /ML — memory lock (65C02/65C816)
    _MREQ,        // /MREQ — memory request (Z80)
    _NMI,         // /NMI — non-maskable interrupt
    _OE,          // /OE — output enable
    _PARD,        // /PARD — peripheral address read (Ricoh 5A22 B-bus)
    _PAWR,        // /PAWR — peripheral address write (Ricoh 5A22 B-bus)
    _Q7,          // /Q7 — complement output (shift register)
    _RAS,         // /RAS — row address strobe
    _RD,          // /RD — read strobe
    _RES,         // /RES — reset
    _RFSH,        // /RFSH — refresh (Z80)
    _ROMH,        // /ROMH — ROM high
    _ROML,        // /ROML — ROM low
    _ROMSEL,      // /ROMSEL — ROM select (Ricoh 5A22 cartridge chip select)
    _SO,          // /SO — set overflow
    _UDS,         // /UDS — upper data strobe (M68K)
    _VA14,        // /VA14 — video address 14 (inverted form)
    _VMA,         // /VMA — valid memory address (M68K)
    _VP,          // /VP — vector pull (active-low)
    _VPA,         // /VPA — valid peripheral address (M68K)
    _VPB,         // /VPB — vector pull bar (active-low)
    _WAIT,        // /WAIT — wait (Z80)
    _WE,          // /WE — write enable
    _WR,          // /WR — write strobe (Ricoh 5A22 A-bus)
    _WRAM,        // /WRAM — work RAM chip select (Ricoh 5A22)

    // Sentinel — all labels below this point are active-high.
    ACTIVE_LOW_END,

    // ================================================================
    // Active-high pins — normal polarity, sorted within each group.
    // ================================================================

    // Power pins
    GND,          // Ground (legacy naming, prefer VSS)
    VCC,          // +5V power (legacy naming, prefer VDD)
    VDD,          // +5V power supply
    VSS,          // Ground (0V)

    // Clock pins
    CLK,          // Generic clock input
    CPUCLK,       // CPU clock output (Ricoh 5A22)
    E_CLK,        // Enable clock output (M68K 6800 peripheral compat)
    M2,           // Derived clock output (2A03)
    PHI0,         // Φ0 — clock input
    PHI1,         // Φ1 — inverted clock output
    PHI2,         // Φ2 — primary clock output
    SYSCLK,       // System master clock input (Ricoh 5A22, 21.477 MHz)

    // Address bus pins (A0-A23, must remain sequential)
    A0, A1, A2, A3, A4, A5, A6, A7,
    A8, A9, A10, A11, A12, A13, A14, A15,
    A16, A17, A18, A19, A20, A21, A22, A23,

    // Data bus pins (D0-D15, must remain sequential)
    D0, D1, D2, D3, D4, D5, D6, D7,
    D8, D9, D10, D11, D12, D13, D14, D15,

    // Control signals
    AEC,          // Address Enable Control (6510)
    ALE,          // Address Latch Enable
    ARDY,         // Port A Ready output (Z80 PIO)
    ASTB,         // Port A Strobe input (Z80 PIO)
    BA,           // Bus Available
    B_ASEL,       // B/A̅ select — port select (Z80 PIO)
    BC1,          // Bus Control 1 (AY-3-8910)
    BC2,          // Bus Control 2 (AY-3-8910)
    BDIR,         // Bus Direction (AY-3-8910)
    BE,           // Bus Enable (65C02/65C816)
    BRDY,         // Port B Ready output (Z80 PIO)
    BSTB,         // Port B Strobe input (Z80 PIO)
    CAS,          // Column Address Strobe (active-high form)
    CS,           // Chip Select (active-high form)
    CS0,          // Chip Select 0 (active-high form)
    CS1,          // Chip Select 1 (active-high form)
    CS2,          // Chip Select 2 (active-high form)
    CS3,          // Chip Select 3 (active-high form, TIA)
    C_DSEL,       // C/D̅ select — control/data (Z80 PIO/CTC)
    CURSOR,       // Cursor output (MC6845 CRTC)
    DE,           // Display Enable (MC6845 CRTC)
    DUMP,         // Paddle dump/discharge (TIA)
    ENABLE,       // Enable clock input (6800 bus family)
    LPSTB,        // Light Pen STroBe (MC6845 CRTC)
    MUX,          // Address multiplexer
    OE,           // Output Enable (active-high form)
    P_S,          // Parallel/Serial control (shift register latch)
    RAS,          // Row Address Strobe (active-high form)
    RD,           // Read strobe (active-high form)
    RDY,          // Ready input/output
    RS,           // Register Select
    RS0,          // Register Select 0 (PIA 6820/6821)
    RS1,          // Register Select 1 (PIA 6820/6821)
    RW,           // Read/Write control
    SYNC,         // Synchronization output
    WE,           // Write Enable (active-high form)

    // Interrupt pins (active-high forms — active-low in _-prefixed)
    ABORT,        // Abort (65C816, active-high form)
    IEI,          // Interrupt Enable In (Z80 daisy chain)
    IEO,          // Interrupt Enable Out (Z80 daisy chain)
    IRQ,          // Interrupt Request (active-high form)
    NMI,          // Non-Maskable Interrupt (active-high form)
    RES,          // Reset (active-high form)

    // Function code pins (M68K)
    FC0,          // Function code 0 (M68K)
    FC1,          // Function code 1 (M68K)
    FC2,          // Function code 2 (M68K)

    // Special pins
    E,            // Emulation mode (65C816)
    ML,           // Memory Lock (active-high form)
    MX,           // Memory/Index size status (65C816)
    SO,           // Set Overflow (active-high form)
    VDA,          // Valid Data Address (65C816)
    VP,           // Vector Pull (active-high form)
    VPA,          // Valid Program Address (65C816)
    VPB,          // Vector Pull Bar (active-high form)

    // I/O Port pins (6510 specific, must remain sequential)
    P0, P1, P2, P3, P4, P5, P6, P7,

    // GPIO pins (must remain sequential within each port)
    PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7,
    PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7,
    PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7,
    PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7,

    // Peripheral pins (microcontroller variants)
    PWM0, PWM1, PWM2, PWM3,
    SPI_CLK,      // SPI clock
    SPI_CS,       // SPI chip select
    SPI_MISO,     // SPI data in
    SPI_MOSI,     // SPI data out
    UART_RX,      // UART receive
    UART_TX,      // UART transmit

    // Video chip pins
    CHROMA,       // Chrominance output
    COLU,         // Color/Luminance output (TIA)
    COLOR,        // Color signal output (VIC-II)
    COLOR_CLK,    // Color clock
    COMP_BLK,     // Composite blank (TIA)
    CSYNC,        // Composite sync
    DOT_CLK,      // Dot clock
    HSYNC,        // Horizontal sync
    LIGHT_PEN,    // Light pen input
    LUMA,         // Luminance output
    VOUT,         // Composite video output
    VSYNC,        // Vertical sync

    // Raster address pins (MC6845 CRTC, must remain sequential)
    RA0, RA1, RA2, RA3, RA4,

    // Audio chip pins
    AUD0,         // Audio output 0 (TIA)
    AUD1,         // Audio output 1 (TIA)
    AUDIO_IN,     // Audio input
    AUDIO_OUT,    // Audio output
    FILTER_IN,    // Filter input
    FILTER_OUT,   // Filter output
    NOISE,        // Noise output
    OSC1,         // Oscillator output 1
    OSC2,         // Oscillator output 2
    OSC3,         // Oscillator output 3
    SND1,         // Sound output 1 (Ricoh 2A03)
    SND2,         // Sound output 2 (Ricoh 2A03)
    SOUND,        // Sound output (VIC-I/II composite audio)

    // AY-3-8910 audio output pins
    CHANNEL_A,    // Analog Channel A output (AY-3-8910)
    CHANNEL_B,    // Analog Channel B output (AY-3-8910)
    CHANNEL_C,    // Analog Channel C output (AY-3-8910)

    // Spectrum/general audio pins
    EAR,          // Tape EAR input
    MIC,          // Tape MIC output
    SPEAKER,      // Speaker output

    // CIA/Timer chip pins
    CNT,          // Counter input
    FLAG,         // Flag input
    PC,           // Peripheral Control output
    SDR,          // Serial Data Register
    SP,           // Serial port
    TOD,          // Time of day clock

    // VIA handshake pins (MOS 6522)
    CA1,          // Port A control line 1
    CA2,          // Port A control line 2
    CB1,          // Port B control line 1
    CB2,          // Port B control line 2

    // Z80 CTC pins (must remain sequential within sub-groups)
    CLK_TRG0,     // Clock/Trigger 0 input (Z80 CTC)
    CLK_TRG1,     // Clock/Trigger 1 input (Z80 CTC)
    CLK_TRG2,     // Clock/Trigger 2 input (Z80 CTC)
    CLK_TRG3,     // Clock/Trigger 3 input (Z80 CTC)
    ZC_TO0,       // Zero Count/Timer Output 0 (Z80 CTC)
    ZC_TO1,       // Zero Count/Timer Output 1 (Z80 CTC)
    ZC_TO2,       // Zero Count/Timer Output 2 (Z80 CTC)
    ZC_TO3,       // Zero Count/Timer Output 3 (Z80 CTC)

    // Memory chip pins (DQ/MA must remain sequential)
    CASRAM,       // CAS for RAM
    DQ0, DQ1, DQ2, DQ3, DQ4, DQ5, DQ6, DQ7,
    MA0, MA1, MA2, MA3, MA4, MA5, MA6, MA7,
    MA8, MA9, MA10, MA11, MA12, MA13, MA14, MA15,

    // SID-specific pins
    CAP1A,        // Filter capacitor 1A
    CAP1B,        // Filter capacitor 1B
    CAP2A,        // Filter capacitor 2A
    CAP2B,        // Filter capacitor 2B
    EXT_IN,       // External audio input
    POTX,         // Paddle X input
    POTY,         // Paddle Y input

    // TIA-specific input pins (must remain sequential)
    INPT0, INPT1, INPT2, INPT3, INPT4, INPT5,

    // PLA specific pins (active-high forms; PLA outputs often
    // active-low — use _-prefixed labels for those)
    BASIC,        // BASIC ROM select (active-high form)
    CASRAM_PLA,   // CAS RAM — PLA specific (active-high form)
    CHAREN,       // Character ROM enable (active-high form)
    CHAROM,       // Character ROM select (active-high form)
    EXROM,        // External ROM (active-high form)
    GAME,         // Game line (active-high form)
    GRW,          // Graphics Read/Write
    HIRAM,        // High RAM (active-high form)
    IO,           // I/O select (active-high form)
    KERNAL,       // KERNAL ROM select (active-high form)
    LORAM,        // Low RAM (active-high form)
    ROMH,         // ROM High (active-high form)
    ROML,         // ROM Low (active-high form)
    VA12,         // Video address 12
    VA13,         // Video address 13
    VA14,         // Video address 14 (active-high form)

    // Keyboard matrix pins (TED 7360, must remain sequential)
    K0, K1, K2, K3, K4, K5, K6, K7,

    // NES-specific pins (Ricoh 2A03 / 2C02)
    AD1,          // Multiplexed address/data 1 (2A03)
    AD2,          // Multiplexed address/data 2 (2A03)
    EXT0, EXT1, EXT2, EXT3, // PPU extension port (2C02)
    IN0,          // Controller data input 0 (2A03)
    IN1,          // Controller data input 1 (2A03)
    OUT0,         // Controller strobe 0 (2A03)
    OUT1,         // Controller strobe 1 (2A03)
    OUT2,         // Controller strobe 2 (2A03)

    // SNES-specific pins (Ricoh 5A22)
    HBLANK,       // Horizontal blank output (5A22)
    JOY1,         // Joypad 1 serial data input (5A22)
    JOY2,         // Joypad 2 serial data input (5A22)
    JOYCLK,       // Joypad clock output (5A22)
    JOYLAT,       // Joypad latch output (5A22)
    JOYRD,        // Joypad auto-read strobe (5A22)
    REFRESH,      // WRAM refresh output (5A22)
    VBLANK,       // Vertical blank output (5A22)

    // MC6847 VDG pins
    AG,           // Alpha/Graphics mode select (MC6847)
    AS,           // Alpha/Semigraphics mode select (MC6847)
    CSS,          // Color Set Select (MC6847)
    FS,           // Field Sync output (MC6847)
    GM0,          // Graphics Mode 0 (MC6847)
    GM1,          // Graphics Mode 1 (MC6847)
    GM2,          // Graphics Mode 2 (MC6847)
    INV,          // Invert (MC6847)
    INTEXT,       // Internal/External character generator (MC6847)

    // Shift register pins
    DS,           // Data Serial input (shift register)

    // Logic chip pins (must remain sequential within sub-groups)
    G,            // Gate/Enable
    I0, I1, I2, I3, I4, I5, I6, I7,
    Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7,
    S0, S1, S2, S3,
    Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7,

    // Test and configuration
    NC,           // No Connect
    TEST,         // Test mode pin

    // Analog pins (AIN must remain sequential)
    AIN0, AIN1, AIN2, AIN3, AIN4, AIN5, AIN6, AIN7,
    AOUT0,        // Analog output 0
    AOUT1,        // Analog output 1
    VREF,         // Voltage reference

    // Crystal/oscillator pins
    OSC_IN,       // Oscillator input
    OSC_OUT,      // Oscillator output
    XTAL1,        // Crystal 1
    XTAL2,        // Crystal 2

    // Power variant (TIA analog section)
    VTIA,         // TIA-specific analog supply voltage

    // Unknown/custom pin — must be last
    UNKNOWN
};

// Pin side enumeration for package layout
enum class PinSide {
    LEFT,         // Left side pins (top to bottom)
    RIGHT,        // Right side pins (top to bottom) 
    TOP,          // Top side pins (left to right)
    BOTTOM        // Bottom side pins (left to right)
};

// ============================================================================
// PIN STRUCTURES
// ============================================================================

// Die pin definition structure with enum-based labels for performance
struct ChipPin {
    uint8_t pin_number;           // Physical pin number (or grid position for BGA)
    PinLabel label;               // Pin label enum for fast comparisons
    const char* alt_function;     // Alternate function name
    bool is_differential_pos;     // True if positive side of differential pair
    bool is_differential_neg;     // True if negative side of differential pair
    
    // Derived properties - computed from label
    PinType get_pin_type() const;
    uint8_t get_bit_index() const;
    bool get_invert_logic() const;
    const char* get_group_name() const;
};

// Pin signal state for real-time visualization
struct PinSignalState {
    uint8_t pin_number;           // Pin number to match ChipPin
    bool signal_level;            // Current signal level (high/low)
    bool drive_direction;         // True if pin drives output, false if receives input
    uint8_t signal_value;         // For multi-bit values or analog levels (0-255)
    bool high_impedance;          // True if pin is in high-impedance (Hi-Z) state
    bool has_pullup;              // Pin has pull-up resistor
    bool has_pulldown;            // Pin has pull-down resistor
    bool signal_valid;            // True if pin signal state is valid/available
    float analog_voltage;         // For analog pins (0.0 - VCC)
    bool is_pwm;                  // True if pin is PWM output
    float pwm_duty_cycle;         // PWM duty cycle (0.0 - 1.0)
};

// BGA grid position (for BGA/LGA packages)
struct BGAPosition {
    uint8_t row;    // Row (A, B, C, ...)
    uint8_t col;    // Column (1, 2, 3, ...)
};

// ============================================================================
// MODERN PIN DEFINITION MACROS (ENUM-BASED)
// ============================================================================

// Macro to create a ChipPin with simplified structure (no derivable fields)
#define PIN(num, lbl_enum) \
    {num, PinLabel::lbl_enum, nullptr, false, false}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Unified pin creation function (replaces all make_*_pin functions)
inline ChipPin make_pin(uint8_t num, PinLabel label = PinLabel::NC, const char* alt_function = nullptr) {
    return ChipPin{num, label, alt_function, false, false};
}

// Helper functions for pin derivation
uint8_t get_bit_index_from_label(PinLabel label);
bool get_invert_logic_from_label(PinLabel label);
const char* pin_type_to_group_name(PinType type);

// Enum-to-string conversion functions
const char* pin_label_to_string(PinLabel label);
std::string pin_label_to_display_string(PinLabel label); // With Unicode symbols

// Derive pin type from pin label
PinType pin_label_to_pin_type(PinLabel label);

// ============================================================================
// PIN-TO-BUS-BIT MAPPING
// ============================================================================

// Describes how a PinLabel maps to a bus_state bit for signal extraction.
struct PinBusMapping {
    int bus_bit;     // Bus bit index, or -1 if no direct mapping
    bool is_input;   // true = input to chip (from bus perspective)
    bool invert;     // true = signal level is inverted from bus bit (active-low)
};

// Maps a PinLabel to its bus_state bit and signal characteristics.
// Returns bus_bit = -1 for pins handled separately (ADDRESS, DATA, POWER,
// CLOCK, NC) or for labels with no bus mapping.
constexpr PinBusMapping get_pin_bus_mapping(PinLabel label) {
    switch (label) {
    // Control signals (active-high)
    case PinLabel::RW:     return { BUS_RW_BIT,    false, false };
    case PinLabel::SYNC:   return { BUS_SYNC_BIT,  false, false };
    case PinLabel::RDY:    return { BUS_RDY_BIT,   true,  false };
    case PinLabel::AEC:    return { BUS_AEC_BIT,   false, false };
    case PinLabel::BE:     return { BUS_BE_BIT,    true,  false };
    case PinLabel::BA:     return { BUS_BA_BIT,    false, false };
    // Interrupt signals (active-low, all inputs)
    case PinLabel::_IRQ:   return { BUS_IRQ_BIT,   true,  true };
    case PinLabel::_NMI:   return { BUS_NMI_BIT,   true,  true };
    case PinLabel::_RES:   return { BUS_RES_BIT,   true,  true };
    case PinLabel::_ABORT: return { BUS_ABORT_BIT, true,  true };
    // Special signals
    case PinLabel::_SO:    return { BUS_SO_BIT,    true,  true  };
    case PinLabel::_VP:    return { BUS_VP_BIT,    false, false };
    case PinLabel::_VPB:   return { BUS_VP_BIT,    false, false };
    case PinLabel::_ML:    return { BUS_ML_BIT,    false, true  };
    default:               return { -1,            false, false };
    }
}
