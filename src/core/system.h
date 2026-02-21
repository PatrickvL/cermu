#pragma once

#include "cermu.h"
#include "chip.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// Modern C++ system class for 8-bit systems
class System8Bit {
private:
    static constexpr std::size_t MAX_CHIPS = 16;
    std::array<ChipEntry, MAX_CHIPS> chips_;
    std::uint8_t chip_count_ = 0;

public:
    System8Bit() = default;
    ~System8Bit();
    
    // Disable copy constructor and assignment for now
    System8Bit(const System8Bit&) = delete;
    System8Bit& operator=(const System8Bit&) = delete;
    
    // Enable move semantics
    System8Bit(System8Bit&&) = default;
    System8Bit& operator=(System8Bit&&) = default;
    
    // Chip registration - simplified for compatibility
    std::uint8_t register_chip(void* chip, ChipDescriptor* desc,
                              std::uint16_t base_address, std::size_t size);
    
    // Get chip by ID
    void* get_chip(std::uint8_t chip_id);
    const void* get_chip(std::uint8_t chip_id) const;
    
    // Direct access to chips array for compatibility
    ChipEntry* get_chips() { return chips_.data(); }
    const ChipEntry* get_chips() const { return chips_.data(); }
    
    // Get chip count
    std::uint8_t get_chip_count() const { return chip_count_; }
    
    // Cleanup all chips
    void destroy_all_chips();
};

// Legacy C-style interface for compatibility during transition
extern "C" {
    // Legacy struct for C compatibility
    struct system_8bit_t {
        System8Bit* cpp_system;
        // Compatibility members for legacy code
        ChipEntry* chips;       // Direct access to chips array
        std::uint8_t chip_count; // Current chip count
    };
    
    std::uint8_t system_chip_register(system_8bit_t* system, void* chip,
                                     ChipDescriptor* desc, std::uint16_t base,
                                     std::size_t size);
    void system_chips_destroy(system_8bit_t* system);
    
    // Helper function to initialize system_8bit_t
    void system_8bit_init(system_8bit_t* system);
}
