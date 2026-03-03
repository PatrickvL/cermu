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
#include "mappers/mapper_005_mmc5.h"
#include "mappers/mapper_007_axrom.h"
#include "mappers/mapper_009_mmc2.h"
#include "mappers/mapper_010_mmc4.h"
#include "mappers/mapper_011_color_dreams.h"
#include "mappers/mapper_022_vrc2a.h"
#include "mappers/mapper_028_action53.h"
#include "mappers/mapper_034_bnrom.h"
#include "mappers/mapper_064_rambo1.h"
#include "mappers/mapper_065_irem_h3001.h"
#include "mappers/mapper_066_gxrom.h"
#include "mappers/mapper_067_sunsoft3.h"
#include "mappers/mapper_068_sunsoft4.h"
#include "mappers/mapper_069_sunsoft_fme7.h"
#include "mappers/mapper_071_camerica.h"
#include "mappers/mapper_079_nina.h"
#include "mappers/mapper_113_nina06.h"
#include "mappers/mapper_232_camerica_bf9096.h"

#include <memory>
#include <iostream>

namespace nes_system {

/// Factory for creating mapper instances by iNES mapper ID.
struct MapperFactory {
    /// Create a mapper instance for the given iNES mapper ID.
    /// Returns nullptr only in theory — unsupported IDs fall back to NROM.
    static inline std::unique_ptr<Mapper> create(uint8_t mapper_id,
                                                  uint8_t prg_banks,
                                                  uint8_t chr_banks) {
        switch (mapper_id) {
            case 0:  return std::make_unique<Mapper000>(prg_banks, chr_banks);
            case 1:  return std::make_unique<Mapper001>(prg_banks, chr_banks);
            case 2:  return std::make_unique<Mapper002>(prg_banks, chr_banks);
            case 3:  return std::make_unique<Mapper003>(prg_banks, chr_banks);
            case 4:  return std::make_unique<Mapper004>(prg_banks, chr_banks);
            case 5:  return std::make_unique<Mapper005>(prg_banks, chr_banks);
            case 7:  return std::make_unique<Mapper007>(prg_banks, chr_banks);
            case 9:  return std::make_unique<Mapper009>(prg_banks, chr_banks);
            case 10: return std::make_unique<Mapper010>(prg_banks, chr_banks);
            case 11: return std::make_unique<Mapper011>(prg_banks, chr_banks);
            case 22: return std::make_unique<Mapper022>(prg_banks, chr_banks);
            case 28: return std::make_unique<Mapper028>(prg_banks, chr_banks);
            case 34: return std::make_unique<Mapper034>(prg_banks, chr_banks);
            case 64: return std::make_unique<Mapper064>(prg_banks, chr_banks);
            case 65: return std::make_unique<Mapper065>(prg_banks, chr_banks);
            case 66: return std::make_unique<Mapper066>(prg_banks, chr_banks);
            case 67: return std::make_unique<Mapper067>(prg_banks, chr_banks);
            case 68: return std::make_unique<Mapper068>(prg_banks, chr_banks);
            case 69: return std::make_unique<Mapper069>(prg_banks, chr_banks);
            case 71: return std::make_unique<Mapper071>(prg_banks, chr_banks);
            case 79: return std::make_unique<Mapper079>(prg_banks, chr_banks);
            case 113: return std::make_unique<Mapper113>(prg_banks, chr_banks);
            case 232: return std::make_unique<Mapper232>(prg_banks, chr_banks);
            default:
                std::cout << "Warning: Unsupported mapper " << (int)mapper_id
                          << ", falling back to NROM" << std::endl;
                return std::make_unique<Mapper000>(prg_banks, chr_banks);
        }
    }
};

} // namespace nes_system
