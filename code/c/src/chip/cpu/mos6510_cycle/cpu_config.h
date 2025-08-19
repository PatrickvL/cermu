#ifndef MOS6510_CYCLE_CPU_CONFIG_H
#define MOS6510_CYCLE_CPU_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// 650x CPU Family Variant Selection
// Based on mos6510_emulator_spec.md lines 462-482
typedef enum {
    CPU_6502 = 0,  // Original MOS 6502 - no I/O ports, standard vectors
    CPU_6507 = 1,  // Atari 2600 variant - reduced address space (A0-A12)
    CPU_6510 = 2,  // Commodore 64 variant - 6-bit I/O port, AEC pin
    CPU_8502 = 3   // Commodore 128 variant - HMOS, enhanced features, 2MHz
} cpu_variant_t;

// CPU Configuration Structure
// Implements the complete 650x family configuration as specified
typedef struct {
    // Family variant selection
    cpu_variant_t cpu_variant;
    
    // Feature enables
    bool has_io_port;           // 6510/8502 I/O port at $00/$01
    bool has_aec_pin;           // 6510/8502 AEC support for bus sharing
    uint8_t address_lines;      // 16 for 6502/6510/8502, 13 for 6507
    bool supports_illegal_ops;  // Enable illegal opcodes (105 useful ones)
    bool enhanced_rdy;          // Enhanced RDY handling (8502)
    
    // Clock configuration
    uint32_t base_frequency;    // Base clock frequency in Hz
    bool variable_clock;        // Support variable clock (8502)
    
    // Interrupt configuration
    bool has_irq_pin;           // IRQ interrupt pin (disabled on 6507)
    bool has_nmi_pin;           // NMI interrupt pin (disabled on 6507)
    bool has_reset_pin;         // Reset pin (all variants)
    
    // Memory configuration
    uint16_t reset_vector;      // Reset vector address (typically $FFFC)
    uint16_t irq_vector;        // IRQ vector address (typically $FFFE)
    uint16_t nmi_vector;        // NMI vector address (typically $FFFA)
} cpu_config_t;

// Predefined configurations for standard variants
extern const cpu_config_t CPU_CONFIG_6502;
extern const cpu_config_t CPU_CONFIG_6507;
extern const cpu_config_t CPU_CONFIG_6510;
extern const cpu_config_t CPU_CONFIG_8502;

// Configuration validation and management functions
bool cpu_config_validate(const cpu_config_t *config);
bool cpu_config_is_compatible(const cpu_config_t *config, cpu_variant_t variant);
void cpu_config_apply_variant_defaults(cpu_config_t *config, cpu_variant_t variant);

// Configuration utility functions
const char* cpu_config_get_variant_name(cpu_variant_t variant);
uint32_t cpu_config_get_address_mask(const cpu_config_t *config);
bool cpu_config_supports_pin(const cpu_config_t *config, const char* pin_name);

// Runtime configuration switching
typedef struct mos6510_state_s mos6510_state_t; // Forward declaration
bool cpu_config_switch_runtime(mos6510_state_t *cpu, const cpu_config_t *new_config);

#endif // MOS6510_CYCLE_CPU_CONFIG_H