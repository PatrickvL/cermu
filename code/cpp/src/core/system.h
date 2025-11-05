#pragma once

#include "aiemuc.h"
#include "chip.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// Unified chip access callback - modernized with C++ features
struct AccessCallback {
    chip_callback_t read_func;
    chip_callback_t write_func;
    void* context;  // Shared context for both read and write operations
    
    AccessCallback(chip_callback_t read, chip_callback_t write, void* ctx)
        : read_func(read), write_func(write), context(ctx) {}
};

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
    
    // Modernized chip registration
    std::uint8_t register_chip(std::unique_ptr<ChipBase> chip,
                              std::unique_ptr<ChipDescriptor> desc,
                              std::uint16_t base_address,
                              std::size_t size);
    
    // Get chip by ID
    ChipBase* get_chip(std::uint8_t chip_id);
    const ChipBase* get_chip(std::uint8_t chip_id) const;
    
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
    };
    
    std::uint8_t system_chip_register(system_8bit_t* system, void* chip,
                                     ChipDescriptor* desc, std::uint16_t base,
                                     std::size_t size);
    void system_chips_destroy(system_8bit_t* system);
}
