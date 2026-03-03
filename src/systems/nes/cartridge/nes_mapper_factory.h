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
#include "mappers/mapper_013_cprom.h"
#include "mappers/mapper_015_100in1.h"
#include "mappers/mapper_022_vrc2a.h"
#include "mappers/mapper_028_action53.h"
#include "mappers/mapper_032_irem_g101.h"
#include "mappers/mapper_033_taito_tc0190.h"
#include "mappers/mapper_034_bnrom.h"
#include "mappers/mapper_041_caltron.h"
#include "mappers/mapper_064_rambo1.h"
#include "mappers/mapper_065_irem_h3001.h"
#include "mappers/mapper_066_gxrom.h"
#include "mappers/mapper_067_sunsoft3.h"
#include "mappers/mapper_068_sunsoft4.h"
#include "mappers/mapper_069_sunsoft_fme7.h"
#include "mappers/mapper_071_camerica.h"
#include "mappers/mapper_072_jaleco_jf17.h"
#include "mappers/mapper_073_vrc3.h"
#include "mappers/mapper_075_vrc1.h"
#include "mappers/mapper_079_nina.h"
#include "mappers/mapper_087_jaleco_jf05.h"
#include "mappers/mapper_088_namco_3433.h"
#include "mappers/mapper_093_sunsoft2.h"
#include "mappers/mapper_095_namco_3425.h"
#include "mappers/mapper_113_nina06.h"
#include "mappers/mapper_118_txsrom.h"
#include "mappers/mapper_119_tqrom.h"
#include "mappers/mapper_133_sachen.h"
#include "mappers/mapper_151_vrc1_vs.h"
#include "mappers/mapper_184_sunsoft1.h"
#include "mappers/mapper_185_cnrom_protect.h"
#include "mappers/mapper_193_ntdec.h"
#include "mappers/mapper_225_multicart52.h"
#include "mappers/mapper_226_multicart76.h"
#include "mappers/mapper_227_multicart1200.h"
#include "mappers/mapper_228_action52.h"
#include "mappers/mapper_229_multicart31.h"
#include "mappers/mapper_231_multicart20.h"
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
            case 0:   return std::make_unique<Mapper000>(prg_banks, chr_banks);
            case 1:   return std::make_unique<Mapper001>(prg_banks, chr_banks);
            case 2:   return std::make_unique<Mapper002>(prg_banks, chr_banks);
            case 3:   return std::make_unique<Mapper003>(prg_banks, chr_banks);
            case 4:   return std::make_unique<Mapper004>(prg_banks, chr_banks);
            case 5:   return std::make_unique<Mapper005>(prg_banks, chr_banks);
            case 7:   return std::make_unique<Mapper007>(prg_banks, chr_banks);
            case 9:   return std::make_unique<Mapper009>(prg_banks, chr_banks);
            case 10:  return std::make_unique<Mapper010>(prg_banks, chr_banks);
            case 11:  return std::make_unique<Mapper011>(prg_banks, chr_banks);
            case 13:  return std::make_unique<Mapper013>(prg_banks, chr_banks);
            case 15:  return std::make_unique<Mapper015>(prg_banks, chr_banks);
            case 22:  return std::make_unique<Mapper022>(prg_banks, chr_banks);
            case 28:  return std::make_unique<Mapper028>(prg_banks, chr_banks);
            case 32:  return std::make_unique<Mapper032>(prg_banks, chr_banks);
            case 33:  return std::make_unique<Mapper033>(prg_banks, chr_banks);
            case 34:  return std::make_unique<Mapper034>(prg_banks, chr_banks);
            case 41:  return std::make_unique<Mapper041>(prg_banks, chr_banks);
            case 64:  return std::make_unique<Mapper064>(prg_banks, chr_banks);
            case 65:  return std::make_unique<Mapper065>(prg_banks, chr_banks);
            case 66:  return std::make_unique<Mapper066>(prg_banks, chr_banks);
            case 67:  return std::make_unique<Mapper067>(prg_banks, chr_banks);
            case 68:  return std::make_unique<Mapper068>(prg_banks, chr_banks);
            case 69:  return std::make_unique<Mapper069>(prg_banks, chr_banks);
            case 71:  return std::make_unique<Mapper071>(prg_banks, chr_banks);
            case 72:  return std::make_unique<Mapper072>(prg_banks, chr_banks);
            case 73:  return std::make_unique<Mapper073>(prg_banks, chr_banks);
            case 75:  return std::make_unique<Mapper075>(prg_banks, chr_banks);
            case 79:  return std::make_unique<Mapper079>(prg_banks, chr_banks);
            case 87:  return std::make_unique<Mapper087>(prg_banks, chr_banks);
            case 88:  return std::make_unique<Mapper088>(prg_banks, chr_banks);
            case 93:  return std::make_unique<Mapper093>(prg_banks, chr_banks);
            case 95:  return std::make_unique<Mapper095>(prg_banks, chr_banks);
            case 113: return std::make_unique<Mapper113>(prg_banks, chr_banks);
            case 118: return std::make_unique<Mapper118>(prg_banks, chr_banks);
            case 119: return std::make_unique<Mapper119>(prg_banks, chr_banks);
            case 133: return std::make_unique<Mapper133>(prg_banks, chr_banks);
            case 151: return std::make_unique<Mapper151>(prg_banks, chr_banks);
            case 184: return std::make_unique<Mapper184>(prg_banks, chr_banks);
            case 185: return std::make_unique<Mapper185>(prg_banks, chr_banks);
            case 193: return std::make_unique<Mapper193>(prg_banks, chr_banks);
            case 225: return std::make_unique<Mapper225>(prg_banks, chr_banks);
            case 226: return std::make_unique<Mapper226>(prg_banks, chr_banks);
            case 227: return std::make_unique<Mapper227>(prg_banks, chr_banks);
            case 228: return std::make_unique<Mapper228>(prg_banks, chr_banks);
            case 229: return std::make_unique<Mapper229>(prg_banks, chr_banks);
            case 231: return std::make_unique<Mapper231>(prg_banks, chr_banks);
            case 232: return std::make_unique<Mapper232>(prg_banks, chr_banks);
            default:
                std::cout << "Warning: Unsupported mapper " << (int)mapper_id
                          << ", falling back to NROM" << std::endl;
                return std::make_unique<Mapper000>(prg_banks, chr_banks);
        }
    }
};

} // namespace nes_system
