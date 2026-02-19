#include "system.h"
#include <iostream>
#include <memory>

#ifdef _MSC_VER
#pragma message("Compiling with MSVC")
#endif
#ifdef _M_X64
#pragma message("Targeting x64")
#endif
#ifdef _M_IX86
#pragma message("Targeting x86")
#endif
#ifdef __GNUC__
#pragma message("Compiling with GCC")
#endif
#ifdef __clang__
#pragma message("Compiling with Clang")
#endif
#ifdef __x86_64__
#pragma message("Detected x86_64")
#endif
#ifdef __i386__
#pragma message("Detected i386")
#endif

// ============================================================================
// Modern C++ System8Bit Implementation
// ============================================================================

System8Bit::~System8Bit() {
    destroy_all_chips();
}

std::uint8_t System8Bit::register_chip(void* chip, ChipDescriptor* desc,
                                      std::uint16_t base_address,
                                      std::size_t size) {
    if (chip_count_ >= MAX_CHIPS) {
        return 0xFF; // Error: too many chips
    }
    
    std::uint8_t id = chip_count_++;
    ChipEntry& entry = chips_[id];
    
    entry.chip = chip;
    entry.desc = desc;
    entry.chip_id = id;
    entry.base_address = base_address;
    entry.size = size;
    
    return id;
}

void* System8Bit::get_chip(std::uint8_t chip_id) {
    if (chip_id >= chip_count_) {
        return nullptr;
    }
    return chips_[chip_id].chip;
}

const void* System8Bit::get_chip(std::uint8_t chip_id) const {
    if (chip_id >= chip_count_) {
        return nullptr;
    }
    return chips_[chip_id].chip;
}

void System8Bit::destroy_all_chips() {
    // Destroy chips using their descriptors
    for (std::uint8_t i = 0; i < chip_count_; ++i) {
        if (chips_[i].desc && chips_[i].desc->destroy && chips_[i].chip) {
            chips_[i].desc->destroy(chips_[i].chip);
        }
        chips_[i].chip = nullptr;
        chips_[i].desc = nullptr;
    }
    chip_count_ = 0;
}

// ============================================================================
// Legacy C Interface for Compatibility
// ============================================================================

extern "C" {
    std::uint8_t system_chip_register(system_8bit_t* system, void* chip,
                                     ChipDescriptor* desc, std::uint16_t base,
                                     std::size_t size) {
        if (!system || !system->cpp_system) {
            return 0xFF;
        }
        
        std::uint8_t result = system->cpp_system->register_chip(chip, desc, base, size);
        
        // Update compatibility pointers
        system->chips = system->cpp_system->get_chips();
        system->chip_count = system->cpp_system->get_chip_count();
        
        return result;
    }

    void system_8bit_init(system_8bit_t* system) {
        if (system) {
            system->cpp_system = new System8Bit();
            system->chips = nullptr;
            system->chip_count = 0;
        }
    }

    void system_chips_destroy(system_8bit_t* system) {
        if (system && system->cpp_system) {
            system->cpp_system->destroy_all_chips();
        }
    }
}
