#pragma once

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct c64_config_s;
typedef struct c64_config_s c64_config_t;

// SID revision enum (matches mos6581.h)
typedef enum {
    SID_REV_6581_R1,
    SID_REV_6581_R2,
    SID_REV_6581_R3,
    SID_REV_6581_R4,
    SID_REV_6581_R4AR,
    SID_REV_8580_R5,
    SID_REV_CSG_6581,
    SID_REV_CSG_8580
} sid_revision_t;

/**
 * Hardware variant selection for test compatibility
 * Extends c64_config_t to support VICE test requirements
 */

// CIA timing models
typedef enum {
    CIA_MODEL_6526,      // Original 6526 (most common)
    CIA_MODEL_6526A,     // 6526A "new" CIA with different timer behavior
    CIA_MODEL_8521       // 8521 (C128/C64C)
} cia_model_t;

// VIC-II chip variants 
typedef enum {
    VICII_MODEL_6569_R1,  // PAL old (R1)
    VICII_MODEL_6569_R3,  // PAL new (R3)
    VICII_MODEL_6567_R56A, // NTSC old (R56A)
    VICII_MODEL_6567_R8,  // NTSC new (R8)
    VICII_MODEL_6567_R9,  // NTSC old (R9)
    VICII_MODEL_6572      // PAL-N (Drean)
} vicii_model_t;

/**
 * Complete hardware configuration for test execution
 * Maps to VICE test requirements
 */
typedef struct {
    // VIC-II configuration
    vicii_model_t vicii_model;
    bool is_pal;          // Derived from vicii_model
    bool is_ntsc;         // Derived from vicii_model
    
    // CIA configuration
    cia_model_t cia1_model;
    cia_model_t cia2_model;
    
    // SID configuration
    sid_revision_t sid_model;
    bool sid_filters_enabled;
    
    // Timing configuration
    bool pal_timing;      // Master clock: PAL (985248 Hz) vs NTSC (1022727 Hz)
    
} c64_hardware_config_t;

/**
 * Initialize hardware config with C64C defaults (most common)
 * - PAL 6569 (new)
 * - 6526A CIA (new)
 * - 8580 SID
 */
void c64_hardware_config_init_defaults(c64_hardware_config_t* hw_config);

/**
 * Initialize hardware config for original C64 (breadbin)
 * - PAL 6569 R1 (old) or NTSC 6567 R56A (old)
 * - 6526 CIA (old)
 * - 6581 SID
 */
void c64_hardware_config_init_original(c64_hardware_config_t* hw_config, bool pal);

/**
 * Convert test framework HardwareConfig flags to system hardware config
 */
c64_hardware_config_t c64_hardware_config_from_test_flags(uint32_t test_hw_flags);

/**
 * Check if current hardware config matches test requirements
 * Returns true if all required hardware is present
 */
bool c64_hardware_config_matches(const c64_hardware_config_t* current,
                                  const c64_hardware_config_t* required);

/**
 * Get human-readable description of hardware configuration
 */
const char* c64_hardware_config_description(const c64_hardware_config_t* hw_config);

/**
 * Apply hardware configuration to c64_config_t for system creation
 */
void c64_hardware_config_apply(const c64_hardware_config_t* hw_config, 
                                c64_config_t* system_config);