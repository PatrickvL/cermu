/*
 * chip_layout.h - Generic chip layout declarations for IC packages
 * 
 * Support for:
 * - Multiple orientation markers (notch, dot, chamfer, bar, triangle)
 * - Various package types (DIP, SOIC, PLCC, QFP, QFN, BGA, TO, SOT)
 * - Chip markings (part number, manufacturer, date code, lot)
 * - Flexible pin notation styles
 * - Bus grouping and differential pairs
 * - Thermal pads and exposed pads
 */

#ifndef CHIP_LAYOUT_H
#define CHIP_LAYOUT_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <string>

// ============================================================================
// PIN TYPES AND STRUCTURES
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

// Package type enumeration
enum class PackageType {
    DIP,          // Dual In-line Package (through-hole) - typically 8, 14, 16, 18, 20, 22, 24, 28, 40, 48, 64 pins
    SOIC,         // Small Outline IC - typically 8, 14, 16, 20, 24, 28, 32 pins
    SOP,          // Small Outline Package - typically 8, 14, 16, 20, 24, 28 pins
    SSOP,         // Shrink Small Outline Package - typically 14, 16, 20, 24, 28, 38, 48, 56 pins
    TSSOP,        // Thin Shrink Small Outline Package - typically 14, 16, 20, 24, 28, 38, 48, 56, 64 pins
    PLCC,         // Plastic Leaded Chip Carrier - typically 20, 28, 44, 52, 68, 84 pins
    QFP,          // Quad Flat Package - typically 32, 44, 52, 64, 80, 100, 128, 144, 176, 208 pins
    LQFP,         // Low-profile Quad Flat Package - typically 32, 44, 48, 64, 80, 100, 128, 144, 176 pins
    TQFP,         // Thin Quad Flat Package - typically 32, 44, 48, 64, 80, 100, 144 pins
    QFN,          // Quad Flat No-leads - typically 16, 20, 24, 28, 32, 40, 44, 48, 56, 64, 68, 76 pins
    DFN,          // Dual Flat No-leads - typically 6, 8, 10, 12, 14, 16 pins
    BGA,          // Ball Grid Array - typically 64, 100, 144, 169, 256, 324, 400, 484, 676, 900+ pins
    LGA,          // Land Grid Array - typically 64, 100, 144, 225, 256, 400, 484+ pins
    SIP,          // Single In-line Package - typically 3, 4, 5, 6, 7, 8, 9, 10 pins
    TO220,        // Transistor Outline 220 (power package) - typically 3, 5, 7 pins
    TO92,         // Transistor Outline 92 - typically 3 pins
    SOT23,        // Small Outline Transistor 23 - typically 3, 5, 6, 8 pins
    SOT223,       // Small Outline Transistor 223 - typically 3, 4, 5, 6, 8 pins
    CUSTOM        // Custom package type - variable pin count
};

// Orientation marker types
enum class OrientationMarker {
    NONE,         // No orientation marker
    NOTCH,        // U-shaped cutout at top center
    DOT,          // Dot/dimple near pin 1
    CHAMFER,      // Beveled/cut corner near pin 1
    BAR,          // Bar/stripe indicating pin 1 side
    TRIANGLE,     // Triangle pointing to pin 1
    CIRCLE,       // Circle at pin 1 location
    NOTCH_AND_DOT,// Both notch and dot (common)
    CUSTOM        // Custom marker
};

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
//    bool is_high;            // Pin logic level (true = high, false = low)
    bool is_tristate;        // True if pin is in high-impedance state
    bool has_pullup;         // Pin has pull-up resistor
    bool has_pulldown;       // Pin has pull-down resistor
    bool is_valid;           // True if pin state is valid/available
    float analog_voltage;    // For analog pins (0.0 - Vcc)
    bool is_pwm;             // True if pin is PWM output
    float pwm_duty_cycle;    // PWM duty cycle (0.0 - 1.0)
};

// Package layout configuration
struct PackageLayout {
    float width;                     // Package width (mil)
    float height;                    // Package height (mil)
    PackageType package_type;        // Package type
    OrientationMarker marker;        // Orientation marker type
    float pin_pitch;                 // Distance between pins (mil)
    bool has_thermal_pad;            // Package has thermal pad/exposed pad
    bool has_center_slug;            // Package has center slug/tab
    float thermal_pad_size;          // Size of thermal pad (relative to package)
};

// Chip markings/labels
struct ChipMarkings {
    const char* part_number;         // Part number (e.g., "MOS6502", "74HC00")
    const char* manufacturer;        // Manufacturer name (e.g., "MOS Technology", "Texas Instruments")
    const char* package_variant;     // Package variant (e.g., "C", "N", "W")
    const char* date_code;           // Date/lot code
    const char* lot_number;          // Lot/batch number  
    const char* custom_text;         // Custom text/description
    bool show_part_number;           // Display part number on chip
    bool show_manufacturer;          // Display manufacturer on chip
    bool show_package_variant;       // Display package variant
    bool show_date_code;             // Display date code
};

// Complete pin layout structure
struct PinLayout { // TODO : Rename to ChipLayout
    PackageLayout package;           // Package dimensions and characteristics
    std::vector<ChipPin> left_pins;  // Left side pins (top to bottom)
    std::vector<ChipPin> right_pins; // Right side pins (top to bottom)
    std::vector<ChipPin> top_pins;   // Top side pins (left to right) 
    std::vector<ChipPin> bottom_pins;// Bottom side pins (left to right)
    std::vector<ChipPin> grid_pins;  // Grid pins for BGA packages
    ChipMarkings markings;           // Chip text markings
    
    // Calculate total number of pins from all sides and grid
    size_t get_total_pins() const {
        return left_pins.size() + right_pins.size() + top_pins.size() + bottom_pins.size() + grid_pins.size();
    }
    
    // Generate package name from type and pin count
    std::string get_package_name() const;
};

// BGA grid position (for BGA/LGA packages)
struct BGAPosition {
    uint8_t row;    // Row (A, B, C, ...)
    uint8_t col;    // Column (1, 2, 3, ...)
};

// ============================================================================
// HELPER FUNCTIONS FOR COMMON PACKAGE TYPES
// ============================================================================

// Create standard DIP package layouts
PinLayout create_dip40_layout();
PinLayout create_dip28_layout();
PinLayout create_dip24_layout();
PinLayout create_dip20_layout();
PinLayout create_dip18_layout();
PinLayout create_dip16_layout();
PinLayout create_dip14_layout();
PinLayout create_dip8_layout();

// Create SOIC/SOP package layouts
PinLayout create_soic8_layout();
PinLayout create_soic14_layout();
PinLayout create_soic16_layout();
PinLayout create_soic28_layout();

// Create PLCC package layouts (pins on all 4 sides)
PinLayout create_plcc28_layout();
PinLayout create_plcc44_layout();
PinLayout create_plcc68_layout();

// Create QFP package layouts (pins on all 4 sides)
PinLayout create_qfp32_layout();
PinLayout create_qfp44_layout();
PinLayout create_qfp64_layout();
PinLayout create_qfp100_layout();
PinLayout create_qfp144_layout();

// Create QFN package layouts
PinLayout create_qfn16_layout();
PinLayout create_qfn24_layout();
PinLayout create_qfn32_layout();
PinLayout create_qfn48_layout();

// Create BGA package layouts
PinLayout create_bga64_layout();
PinLayout create_bga100_layout();
PinLayout create_bga256_layout();

// Create power package layouts
PinLayout create_to220_layout();   // 3-pin power regulator
PinLayout create_to92_layout();    // 3-pin small transistor
PinLayout create_sot23_layout();   // 3/5/6-pin small transistor
PinLayout create_sot223_layout();  // Power package

// Create SIP layouts
PinLayout create_sip8_layout();
PinLayout create_sip9_layout();

// Helper functions for building custom layouts
PinLayout create_custom_dip(uint8_t total_pins, const char* part_name = nullptr);
PinLayout create_custom_qfp(uint8_t total_pins, const char* part_name = nullptr);
PinLayout create_custom_bga(uint8_t rows, uint8_t cols, const char* part_name = nullptr);

// Pin definition helpers
ChipPin make_power_pin(uint8_t num, PinLabel label);
ChipPin make_ground_pin(uint8_t num, PinLabel label = PinLabel::GND);
ChipPin make_address_pin(uint8_t num, PinLabel label, uint8_t bit);
ChipPin make_data_pin(uint8_t num, PinLabel label, uint8_t bit);
ChipPin make_control_pin(uint8_t num, PinLabel label, bool active_low = false);
ChipPin make_clock_pin(uint8_t num, PinLabel label);
ChipPin make_interrupt_pin(uint8_t num, PinLabel label, bool active_low = true);
ChipPin make_gpio_pin(uint8_t num, PinLabel label, const char* port = nullptr);
ChipPin make_analog_pin(uint8_t num, PinLabel label);
ChipPin make_differential_pin(uint8_t num, PinLabel label, bool positive);
ChipPin make_nc_pin(uint8_t num);

// Enum-to-string conversion functions
const char* pin_label_to_string(PinLabel label);
std::string pin_label_to_display_string(PinLabel label); // With Unicode symbols
const std::string get_package_type_string(PackageType package_type);

// String-to-enum conversion (for backwards compatibility)
PinLabel string_to_pin_label(const char* label_str);

// Derive pin type from pin label
PinType pin_label_to_pin_type(PinLabel label);

#endif // CHIP_LAYOUT_H