#pragma once
/*
 * fam65xx_feat_io_port.hpp - Shared I/O Port Feature Component
 *
 * This file provides reusable I/O port functionality for processors that support
 * memory-mapped I/O ports (primarily MOS 6510 and derivatives).
 *
 * FEATURES PROVIDED:
 * - Memory-mapped I/O port at addresses $0000 (DDR) and $0001 (Data)
 * - Configurable external pin state and pull-up/pull-down behavior
 * - Callback system for external system integration
 * - C64-compatible I/O port bit definitions for memory banking
 * - Access tracking and statistics
 *
 * USAGE PATTERN:
 * - Template parameter determines if feature is enabled at compile time
 * - Only processors with IO_PORT feature flag instantiate this component
 * - Provides both direct API and memory-mapped register interface
 *
 * EXTRACTED FROM: mos6510.hpp (original implementation)
 * GENERALIZED FOR: Any processor that needs I/O port functionality
 */

#include "fam65xx_processor_traits.hpp"
#include <cstdint>
#include <cstring>

#ifdef __cplusplus

namespace fam65xx_features {

// ============================================================================
// I/O PORT BIT DEFINITIONS - Commodore 64 Compatible
// ============================================================================

enum IOPortBits {
    LORAM = 0x01,       // RAM/ROM at $A000-$BFFF and $E000-$FFFF
    HIRAM = 0x02,       // RAM/ROM at $A000-$BFFF and $E000-$FFFF
    CHAREN = 0x04,      // Character ROM at $D000-$DFFF
    CASS_DATA = 0x08,   // Cassette data output
    CASS_SWITCH = 0x10, // Cassette switch sense
    CASS_MOTOR = 0x20,  // Cassette motor control
    UNUSED6 = 0x40,     // Unused bit 6
    UNUSED7 = 0x80      // Unused bit 7
};

// ============================================================================
// I/O PORT CALLBACK SYSTEM
// ============================================================================

// Callback structure for external system integration
struct IOPortCallbacks {
    // Called when CPU writes to I/O port direction register ($00)
    void (*on_direction_write)(void* user_data, uint8_t value);
    
    // Called when CPU writes to I/O port data register ($01)
    void (*on_data_write)(void* user_data, uint8_t value, uint8_t direction_mask);
    
    // Called when CPU reads from I/O port data register ($01)
    // Should return the current port state (external pins + internal data)
    uint8_t (*on_data_read)(void* user_data, uint8_t internal_data, uint8_t direction_mask);
    
    // User-provided context pointer
    void* user_data;
    
    // Initialize with null callbacks
    IOPortCallbacks() : on_direction_write(nullptr), on_data_write(nullptr), 
                       on_data_read(nullptr), user_data(nullptr) {}
};

// ============================================================================
// I/O PORT STATE STRUCTURE
// ============================================================================

struct IOPortState {
    uint8_t ddr;        // Data Direction Register ($0000) - 1=output, 0=input
    uint8_t data;       // Port Data Register ($0001) - output values
    uint8_t external;   // External input state from physical pins
    uint8_t pullup;     // Pull-up resistor configuration
    uint8_t floating;   // Floating pin behavior configuration
    
    // Initialize with C64-compatible defaults
    IOPortState() : ddr(0x00), data(0x37), external(0xFF), pullup(0xFF), floating(0x00) {}
    
    // Reset to power-on state
    void reset() {
        ddr = 0x00;         // All pins input by default
        data = 0x37;        // C64 startup state (LORAM|HIRAM|CHAREN off, others on)
        external = 0xFF;    // External pins pulled high
        pullup = 0xFF;      // All pins have pull-ups
        floating = 0x00;    // No floating pins
    }
};

// ============================================================================
// I/O PORT FEATURE TEMPLATE
// ============================================================================

template<typename ProcessorTag>
class IOPortFeature {
private:
    // Feature is only enabled if processor has IO_PORT capability
    static constexpr bool enabled = fam65xx_core::has_feature<ProcessorTag>(fam65xx_core::ProcessorFeatures::IO_PORT);
    
    IOPortState state;
    IOPortCallbacks callbacks;
    uint32_t access_count;
    
    // Internal helper to calculate effective port value
    uint8_t calculate_effective_value() const {
        // For output pins (DDR=1): use internal data value
        // For input pins (DDR=0): use external pin state with pull-up behavior
        uint8_t output_mask = state.ddr;
        uint8_t input_mask = ~state.ddr;
        
        uint8_t output_bits = state.data & output_mask;
        uint8_t input_bits = state.external & input_mask;
        
        // Apply pull-up behavior to floating input pins
        uint8_t pullup_bits = state.pullup & input_mask & ~state.floating;
        input_bits |= pullup_bits;
        
        return output_bits | input_bits;
    }
    
public:
    // ========================================================================
    // CONSTRUCTOR AND LIFECYCLE
    // ========================================================================
    
    IOPortFeature() : access_count(0) {
        if constexpr (enabled) {
            state.reset();
        }
    }
    
    void reset() {
        if constexpr (enabled) {
            state.reset();
            access_count = 0;
        }
    }
    
    // ========================================================================
    // CONFIGURATION AND CALLBACKS
    // ========================================================================
    
    void set_callbacks(const IOPortCallbacks& cb) {
        if constexpr (enabled) {
            callbacks = cb;
        }
    }
    
    void clear_callbacks() {
        if constexpr (enabled) {
            callbacks = IOPortCallbacks();
        }
    }
    
    void configure_pullups(uint8_t pullup_mask) {
        if constexpr (enabled) {
            state.pullup = pullup_mask;
        }
    }
    
    void configure_floating(uint8_t floating_mask) {
        if constexpr (enabled) {
            state.floating = floating_mask;
        }
    }
    
    // ========================================================================
    // REGISTER ACCESS INTERFACE
    // ========================================================================
    
    // Write to Data Direction Register ($0000)
    void write_ddr(uint8_t value) {
        if constexpr (enabled) {
            state.ddr = value;
            access_count++;
            
            // Notify external system of direction change
            if (callbacks.on_direction_write) {
                callbacks.on_direction_write(callbacks.user_data, value);
            }
        }
    }
    
    // Write to Port Data Register ($0001)
    void write_data(uint8_t value) {
        if constexpr (enabled) {
            state.data = value;
            access_count++;
            
            // Notify external system of data write
            if (callbacks.on_data_write) {
                callbacks.on_data_write(callbacks.user_data, value, state.ddr);
            }
        }
    }
    
    // Read from Data Direction Register ($0000)
    uint8_t read_ddr() const {
        if constexpr (enabled) {
            return state.ddr;
        }
        return 0xFF;
    }
    
    // Read from Port Data Register ($0001)
    uint8_t read_data() {
        if constexpr (enabled) {
            access_count++;
            
            // If callback is available, use it for external pin state
            if (callbacks.on_data_read) {
                return callbacks.on_data_read(callbacks.user_data, state.data, state.ddr);
            }
            
            // Fallback: calculate effective value internally
            return calculate_effective_value();
        }
        return 0xFF;
    }
    
    // ========================================================================
    // MEMORY-MAPPED INTERFACE (for memory system integration)
    // ========================================================================
    
    // Handle memory-mapped register access
    void write_register(uint16_t addr, uint8_t value) {
        if constexpr (enabled) {
            switch (addr) {
                case 0x0000:
                    write_ddr(value);
                    break;
                case 0x0001:
                    write_data(value);
                    break;
                // Ignore writes to other addresses
            }
        }
    }
    
    uint8_t read_register(uint16_t addr) {
        if constexpr (enabled) {
            switch (addr) {
                case 0x0000:
                    return read_ddr();
                case 0x0001:
                    return read_data();
                default:
                    return 0xFF; // Invalid I/O address
            }
        }
        return 0xFF;
    }
    
    // Check if address is in I/O port range
    static constexpr bool is_io_address(uint16_t addr) {
        return (addr == 0x0000) || (addr == 0x0001);
    }
    
    // ========================================================================
    // EXTERNAL PIN INTERFACE
    // ========================================================================
    
    void set_external_pins(uint8_t value) {
        if constexpr (enabled) {
            state.external = value;
        }
    }
    
    uint8_t get_external_pins() const {
        if constexpr (enabled) {
            return state.external;
        }
        return 0xFF;
    }
    
    // Set individual external pin
    void set_external_pin(uint8_t pin_mask, bool value) {
        if constexpr (enabled) {
            if (value) {
                state.external |= pin_mask;
            } else {
                state.external &= ~pin_mask;
            }
        }
    }
    
    // Get individual external pin state
    bool get_external_pin(uint8_t pin_mask) const {
        if constexpr (enabled) {
            return (state.external & pin_mask) != 0;
        }
        return true;
    }
    
    // ========================================================================
    // C64-SPECIFIC MEMORY BANKING HELPERS
    // ========================================================================
    
    bool is_basic_rom_enabled() const {
        if constexpr (enabled) {
            uint8_t port_value = calculate_effective_value();
            return (port_value & (LORAM | HIRAM)) == (LORAM | HIRAM);
        }
        return false;
    }
    
    bool is_kernal_rom_enabled() const {
        if constexpr (enabled) {
            uint8_t port_value = calculate_effective_value();
            return (port_value & HIRAM) != 0;
        }
        return false;
    }
    
    bool is_char_rom_enabled() const {
        if constexpr (enabled) {
            uint8_t port_value = calculate_effective_value();
            return (port_value & CHAREN) == 0;
        }
        return false;
    }
    
    bool is_io_enabled() const {
        if constexpr (enabled) {
            uint8_t port_value = calculate_effective_value();
            return (port_value & CHAREN) != 0;
        }
        return true;
    }
    
    // Get complete memory configuration
    uint8_t get_memory_config() const {
        if constexpr (enabled) {
            return calculate_effective_value() & 0x07; // Lower 3 bits determine config
        }
        return 0x07; // Default to all enabled
    }
    
    // ========================================================================
    // STATISTICS AND DEBUGGING
    // ========================================================================
    
    uint32_t get_access_count() const {
        if constexpr (enabled) {
            return access_count;
        }
        return 0;
    }
    
    void reset_access_count() {
        if constexpr (enabled) {
            access_count = 0;
        }
    }
    
    // Get complete I/O port state for debugging
    IOPortState get_state() const {
        if constexpr (enabled) {
            return state;
        }
        return IOPortState();
    }
    
    // Feature availability check
    static constexpr bool is_enabled() {
        return enabled;
    }
    
    // ========================================================================
    // TICK INTEGRATION (for CPU integration)
    // ========================================================================
    
    // Called during CPU tick to handle any I/O port specific timing
    template<typename BusState>
    BusState handle_tick(BusState pins) {
        if constexpr (enabled) {
            // I/O port doesn't need special tick handling in basic implementation
            // Advanced implementations could handle timing-sensitive operations here
        }
        return pins;
    }
    
    // Handle memory access interception
    template<typename BusState>
    bool intercept_memory_access(uint16_t addr, BusState& pins, bool is_write) {
        if constexpr (enabled) {
            if (is_io_address(addr)) {
                // This is an I/O port access
                if (is_write) {
                    // Extract data from pins and write to register
                    uint8_t data = static_cast<uint8_t>(pins & 0xFF); // Simplified extraction
                    write_register(addr, data);
                } else {
                    // Read from register and update pins
                    uint8_t data = read_register(addr);
                    pins = (pins & ~0xFF) | data; // Simplified pin update
                }
                return true; // Access was intercepted
            }
        }
        return false; // Access not intercepted
    }
};

// ============================================================================
// C API COMPATIBILITY LAYER
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// C-compatible I/O port state structure
typedef struct {
    uint8_t ddr;
    uint8_t data;
    uint8_t external;
    uint8_t pullup;
    uint8_t floating;
    uint32_t access_count;
} io_port_state_c_t;

// C-compatible callback structure
typedef struct {
    void (*on_direction_write)(void* user_data, uint8_t value);
    void (*on_data_write)(void* user_data, uint8_t value, uint8_t direction_mask);
    uint8_t (*on_data_read)(void* user_data, uint8_t internal_data, uint8_t direction_mask);
    void* user_data;
} io_port_callbacks_c_t;

#ifdef __cplusplus
}
#endif

} // namespace fam65xx_features

#endif // __cplusplus