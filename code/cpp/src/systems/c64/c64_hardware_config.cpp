#include "c64_hardware_config.h"
#include "c64_config.h"
#include "c64_test_framework.h"
#include <stdio.h>
#include <string.h>

void c64_hardware_config_init_defaults(c64_hardware_config_t* hw_config) {
    if (!hw_config) return;
    
    // C64C defaults (most common modern C64)
    hw_config->vicii_model = VICII_MODEL_6569_R3;  // PAL new
    hw_config->is_pal = true;
    hw_config->is_ntsc = false;
    hw_config->cia1_model = CIA_MODEL_6526A;       // New CIA
    hw_config->cia2_model = CIA_MODEL_6526A;
    hw_config->sid_model = SID_REV_8580_R5;        // 8580 SID
    hw_config->sid_filters_enabled = true;
    hw_config->pal_timing = true;
}

void c64_hardware_config_init_original(c64_hardware_config_t* hw_config, bool pal) {
    if (!hw_config) return;
    
    // Original C64 "breadbin"
    if (pal) {
        hw_config->vicii_model = VICII_MODEL_6569_R1;  // PAL old
        hw_config->is_pal = true;
        hw_config->is_ntsc = false;
        hw_config->pal_timing = true;
    } else {
        hw_config->vicii_model = VICII_MODEL_6567_R56A; // NTSC old
        hw_config->is_pal = false;
        hw_config->is_ntsc = true;
        hw_config->pal_timing = false;
    }
    
    hw_config->cia1_model = CIA_MODEL_6526;        // Old CIA
    hw_config->cia2_model = CIA_MODEL_6526;
    hw_config->sid_model = SID_REV_6581_R3;        // 6581 SID
    hw_config->sid_filters_enabled = true;
}

c64_hardware_config_t c64_hardware_config_from_test_flags(uint32_t test_hw_flags) {
    using namespace c64_test;
    
    c64_hardware_config_t hw_config;
    c64_hardware_config_init_defaults(&hw_config);  // Start with defaults
    
    // VIC-II configuration
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::VICII_PAL)) {
        hw_config.vicii_model = VICII_MODEL_6569_R3;
        hw_config.is_pal = true;
        hw_config.is_ntsc = false;
        hw_config.pal_timing = true;
    }
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::VICII_NTSC)) {
        hw_config.vicii_model = VICII_MODEL_6567_R8;
        hw_config.is_pal = false;
        hw_config.is_ntsc = true;
        hw_config.pal_timing = false;
    }
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::VICII_NTSCOLD)) {
        hw_config.vicii_model = VICII_MODEL_6567_R56A;
        hw_config.is_pal = false;
        hw_config.is_ntsc = true;
        hw_config.pal_timing = false;
    }
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::VICII_OLD)) {
        if (hw_config.is_pal) {
            hw_config.vicii_model = VICII_MODEL_6569_R1;
        } else {
            hw_config.vicii_model = VICII_MODEL_6567_R56A;
        }
    }
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::VICII_NEW)) {
        if (hw_config.is_pal) {
            hw_config.vicii_model = VICII_MODEL_6569_R3;
        } else {
            hw_config.vicii_model = VICII_MODEL_6567_R8;
        }
    }
    
    // CIA configuration
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::CIA_OLD)) {
        hw_config.cia1_model = CIA_MODEL_6526;
        hw_config.cia2_model = CIA_MODEL_6526;
    }
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::CIA_NEW)) {
        hw_config.cia1_model = CIA_MODEL_6526A;
        hw_config.cia2_model = CIA_MODEL_6526A;
    }
    
    // SID configuration
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::SID_6581)) {
        hw_config.sid_model = SID_REV_6581_R3;
    }
    if (test_hw_flags & static_cast<uint32_t>(HardwareConfig::SID_8580)) {
        hw_config.sid_model = SID_REV_8580_R5;
    }
    
    return hw_config;
}

bool c64_hardware_config_matches(const c64_hardware_config_t* current,
                                  const c64_hardware_config_t* required) {
    if (!current || !required) return false;
    
    // Check VIC-II match (PAL/NTSC compatibility)
    if (required->is_pal && !current->is_pal) return false;
    if (required->is_ntsc && !current->is_ntsc) return false;
    
    // Check CIA match (old vs new)
    if (required->cia1_model != current->cia1_model) return false;
    if (required->cia2_model != current->cia2_model) return false;
    
    // Check SID match (6581 vs 8580)
    bool required_is_6581 = (required->sid_model >= SID_REV_6581_R1 && 
                             required->sid_model <= SID_REV_CSG_6581);
    bool current_is_6581 = (current->sid_model >= SID_REV_6581_R1 && 
                            current->sid_model <= SID_REV_CSG_6581);
    
    if (required_is_6581 != current_is_6581) return false;
    
    return true;
}

const char* c64_hardware_config_description(const c64_hardware_config_t* hw_config) {
    static char desc[256];
    
    if (!hw_config) return "NULL";
    
    const char* vic_name = "Unknown";
    switch (hw_config->vicii_model) {
        case VICII_MODEL_6569_R1: vic_name = "6569 R1 (PAL old)"; break;
        case VICII_MODEL_6569_R3: vic_name = "6569 R3 (PAL new)"; break;
        case VICII_MODEL_6567_R56A: vic_name = "6567 R56A (NTSC old)"; break;
        case VICII_MODEL_6567_R8: vic_name = "6567 R8 (NTSC new)"; break;
        case VICII_MODEL_6567_R9: vic_name = "6567 R9 (NTSC old)"; break;
        case VICII_MODEL_6572: vic_name = "6572 (PAL-N)"; break;
    }
    
    const char* cia_name = "Unknown";
    switch (hw_config->cia1_model) {
        case CIA_MODEL_6526: cia_name = "6526 (old)"; break;
        case CIA_MODEL_6526A: cia_name = "6526A (new)"; break;
        case CIA_MODEL_8521: cia_name = "8521"; break;
    }
    
    const char* sid_name = "Unknown";
    switch (hw_config->sid_model) {
        case SID_REV_6581_R1: sid_name = "6581 R1"; break;
        case SID_REV_6581_R2: sid_name = "6581 R2"; break;
        case SID_REV_6581_R3: sid_name = "6581 R3"; break;
        case SID_REV_6581_R4: sid_name = "6581 R4"; break;
        case SID_REV_6581_R4AR: sid_name = "6581 R4AR"; break;
        case SID_REV_8580_R5: sid_name = "8580 R5"; break;
        case SID_REV_CSG_6581: sid_name = "CSG 6581"; break;
        case SID_REV_CSG_8580: sid_name = "CSG 8580"; break;
    }
    
    snprintf(desc, sizeof(desc), "VIC-II: %s, CIA: %s, SID: %s",
             vic_name, cia_name, sid_name);
    
    return desc;
}

void c64_hardware_config_apply(const c64_hardware_config_t* hw_config, 
                                c64_config_t* system_config) {
    if (!hw_config || !system_config) return;
    
    // Apply VIC-II configuration
    system_config->vicii_standard = hw_config->is_pal ? VIC_PAL : VIC_NTSC;
    
    // Note: CIA and SID models need to be applied during chip creation
    // or via additional configuration in c64_system_create
    // For now, this just sets the basic PAL/NTSC mode
}