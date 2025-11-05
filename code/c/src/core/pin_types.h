/*
 * pin_types.h - Pin type definitions and enumerations
 * 
 * Core pin type definitions used across all IC packages and chip types.
 * Includes pin types, labels, sides, and state structures.
 */

#ifndef PIN_TYPES_H
#define PIN_TYPES_H

#include <cstdint>
#include <string>

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

// Pin label enumeration for 65xx CPU family and common pins
enum class PinLabel {
    // Power pins
    VDD,          // +5V power supply
    VSS,          // Ground (0V)
    VCC,          // Legacy +5V naming (prefer VDD)
    GND,          // Legacy ground naming (prefer VSS)
    
    // Clock pins
    PHI0,         // Φ0 - Clock input
    PHI1,         // Φ1 - Inverted clock output
    PHI2,         // Φ2 - Primary clock output
    
    // Address bus pins (A0-A23 for full 24-bit addressing)
    A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15,
    A16, A17, A18, A19, A20, A21, A22, A23,
    
    // Data bus pins (D0-D15 for 16-bit data)
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15,
    
    // Control signals
    RW,           // Read/Write control
    SYNC,         // Synchronization output
    RDY,          // Ready input/output
    AEC,          // Address Enable Control (6510)
    BE,           // Bus Enable (65C02/65C816)
    BA,           // Bus Available
    
    // Interrupt pins
    IRQ,          // Interrupt Request (active low)
    NMI,          // Non-Maskable Interrupt (active low)
    RES,          // Reset (active low)
    ABORT,        // Abort (65C816, active low)
    
    // Special pins
    SO,           // Set Overflow (active low)
    VP,           // Vector Pull (65C02/65C816)
    VPB,          // Vector Pull Bar (alternate naming)
    VPA,          // Valid Program Address (65C816)
    VDA,          // Valid Data Address (65C816)
    ML,           // Memory Lock (65C02/65C816, active low)
    E,            // Emulation mode (65C816)
    MX,           // Memory/Index size status (65C816)
    
    // I/O Port pins (6510 specific)
    P0, P1, P2, P3, P4, P5, P6, P7,
    
    // GPIO pins (general purpose)
    PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7,
    PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7,
    PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7,
    PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7,
    
    // Peripheral pins (for microcontroller variants)
    UART_TX, UART_RX,
    SPI_CLK, SPI_MOSI, SPI_MISO, SPI_CS,
    PWM0, PWM1, PWM2, PWM3,
    
    // Common control pins
    CS,           // Chip Select
    CS0, CS1, CS2, // Multiple chip selects
    OE,           // Output Enable
    WE,           // Write Enable
    
    // Video chip pins (VIC-II, etc.)
    LUMA,         // Luminance output
    CHROMA,       // Chrominance output
    HSYNC,        // Horizontal sync
    VSYNC,        // Vertical sync
    CSYNC,        // Composite sync
    DOT_CLK,      // Dot clock
    COLOR_CLK,    // Color clock
    LIGHT_PEN,    // Light pen input
    CAS,          // Column Address Strobe
    RAS,          // Row Address Strobe
    MUX,          // Address multiplexer
    
    // Audio chip pins (SID, etc.)
    AUDIO_OUT,    // Audio output
    AUDIO_IN,     // Audio input
    FILTER_OUT,   // Filter output
    FILTER_IN,    // Filter input
    OSC1, OSC2, OSC3, // Oscillator outputs
    NOISE,        // Noise output
    
    // CIA/Timer chip pins
    CNT,          // Counter input
    SP,           // Serial port
    TOD,          // Time of day clock
    FLAG,         // Flag input
    
    // Memory chip pins
    DQ0, DQ1, DQ2, DQ3, DQ4, DQ5, DQ6, DQ7, // Data I/O
    MA0, MA1, MA2, MA3, MA4, MA5, MA6, MA7,  // Memory address
    MA8, MA9, MA10, MA11, MA12, MA13, MA14, MA15,
    
    // Logic chip pins
    Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7,         // Outputs
    I0, I1, I2, I3, I4, I5, I6, I7,         // Inputs
    Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7,         // Logic outputs
    S0, S1, S2, S3,                         // Select lines
    G,            // Gate/Enable
    
    // Test and configuration pins
    TEST,         // Test mode pin
    NC,           // No Connect
    
    // Analog pins
    VREF,         // Voltage Reference
    AIN0, AIN1, AIN2, AIN3, AIN4, AIN5, AIN6, AIN7,
    AOUT0, AOUT1,
    
    // Crystal/oscillator pins
    XTAL1, XTAL2,
    OSC_IN, OSC_OUT,
    
    // Unknown/custom pin
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

// Pin definition structure with enum-based labels for performance
struct ChipPin {
    uint8_t pin_number;      // Physical pin number (or grid position for BGA)
    PinLabel label;          // Pin label enum for fast comparisons
    uint8_t bit_index;       // Bit index within bus (for address/data pins)
    bool invert_logic;       // True if pin is active-low
    const char* group_name;  // Bus/group name (e.g., "ADDR", "DATA", "PORTA")
    const char* alt_function;// Alternate function name
    bool is_differential_pos;// True if positive side of differential pair
    bool is_differential_neg;// True if negative side of differential pair
    
    // Derived property - get pin type from label
    PinType get_pin_type() const;
};

// Pin state for real-time visualization
struct PinState {
    uint8_t pin_number;      // Pin number to match ChipPin
    bool is_active;          // Current pin state
    bool is_output;          // True if pin is output, false if input
    uint8_t value;           // For multi-bit values or analog levels (0-255)
    bool is_tristate;        // True if pin is in high-impedance state
    bool has_pullup;         // Pin has pull-up resistor
    bool has_pulldown;       // Pin has pull-down resistor
    bool is_valid;           // True if pin state is valid/available
    float analog_voltage;    // For analog pins (0.0 - Vcc)
    bool is_pwm;             // True if pin is PWM output
    float pwm_duty_cycle;    // PWM duty cycle (0.0 - 1.0)
};

// BGA grid position (for BGA/LGA packages)
struct BGAPosition {
    uint8_t row;    // Row (A, B, C, ...)
    uint8_t col;    // Column (1, 2, 3, ...)
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Helper functions for pin derivation
uint8_t get_bit_index_from_label(PinLabel label);
bool get_invert_logic_from_label(PinLabel label);
const char* pin_type_to_group_name(PinType type);

// Enum-to-string conversion functions
const char* pin_label_to_string(PinLabel label);
std::string pin_label_to_display_string(PinLabel label); // With Unicode symbols

// String-to-enum conversion (for backwards compatibility)
PinLabel string_to_pin_label(const char* label_str);

// Derive pin type from pin label
PinType pin_label_to_pin_type(PinLabel label);

#endif // PIN_TYPES_H