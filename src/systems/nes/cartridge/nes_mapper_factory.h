#pragma once
/*
 * nes_mapper_factory.h — Mapper creation by iNES mapper ID
 *
 * Header-only factory: given a mapper number and bank counts, returns
 * a std::unique_ptr<Mapper>.  Falls back to NROM (000) for unsupported IDs.
 *
 * Adding a new mapper:
 *   1. Create cartridge/mappers/mapper_NNN_name.h
 *   2. #include it here
 *   3. Add a case to create_mapper()
 */

#include "nes_mapper.h"
#include "mappers/mapper_000_nrom.h"
#include "mappers/mapper_001_mmc1.h"
#include "mappers/mapper_002_uxrom.h"
#include "mappers/mapper_003_cnrom.h"
#include "mappers/mapper_004_mmc3.h"

#include <memory>
#include <iostream>

namespace nes_system {

// Create a mapper instance for the given iNES mapper ID.
// Returns nullptr only in theory — unsupported IDs fall back to NROM.
inline std::unique_ptr<Mapper> create_mapper(uint8_t mapper_id,
                                             uint8_t prg_banks,
                                             uint8_t chr_banks) {
    switch (mapper_id) {
        case 0:  return std::make_unique<Mapper000>(prg_banks, chr_banks);
        case 1:  return std::make_unique<Mapper001>(prg_banks, chr_banks);
        case 2:  return std::make_unique<Mapper002>(prg_banks, chr_banks);
        case 3:  return std::make_unique<Mapper003>(prg_banks, chr_banks);
        case 4:  return std::make_unique<Mapper004>(prg_banks, chr_banks);
        default:
            std::cout << "Warning: Unsupported mapper " << (int)mapper_id
                      << ", falling back to NROM" << std::endl;
            return std::make_unique<Mapper000>(prg_banks, chr_banks);
    }
}

} // namespace nes_system
