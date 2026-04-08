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

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_000_nrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_001_mmc1.hpp"
#include "systems/nes/cartridge/mappers/mapper_002_uxrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_003_cnrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_004_mmc3.hpp"
#include "systems/nes/cartridge/mappers/mapper_005_mmc5.hpp"
#include "systems/nes/cartridge/mappers/mapper_007_axrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_mmc24.hpp"
#include "systems/nes/cartridge/mappers/mapper_011_color_dreams.hpp"
#include "systems/nes/cartridge/mappers/mapper_013_cprom.hpp"
#include "systems/nes/cartridge/mappers/mapper_015_100in1.hpp"
#include "systems/nes/cartridge/mappers/mapper_016_bandai_fcg.hpp"
#include "systems/nes/cartridge/mappers/mapper_018_jaleco_ss88006.hpp"
#include "systems/nes/cartridge/mappers/mapper_019_namco163.hpp"
#include "systems/nes/cartridge/mappers/mapper_vrc24.hpp"
#include "systems/nes/cartridge/mappers/mapper_vrc6.hpp"
#include "systems/nes/cartridge/mappers/mapper_028_action53.hpp"
#include "systems/nes/cartridge/mappers/mapper_030_unrom512.hpp"
#include "systems/nes/cartridge/mappers/mapper_032_irem_g101.hpp"
#include "systems/nes/cartridge/mappers/mapper_033_taito_tc0190.hpp"
#include "systems/nes/cartridge/mappers/mapper_034_bnrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_038_crime_busters.hpp"
#include "systems/nes/cartridge/mappers/mapper_040_fds_hack.hpp"
#include "systems/nes/cartridge/mappers/mapper_041_caltron.hpp"
#include "systems/nes/cartridge/mappers/mapper_047_nesqj.hpp"
#include "systems/nes/cartridge/mappers/mapper_048_taito_tc0690.hpp"
#include "systems/nes/cartridge/mappers/mapper_064_rambo1.hpp"
#include "systems/nes/cartridge/mappers/mapper_065_irem_h3001.hpp"
#include "systems/nes/cartridge/mappers/mapper_066_gxrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_067_sunsoft3.hpp"
#include "systems/nes/cartridge/mappers/mapper_068_sunsoft4.hpp"
#include "systems/nes/cartridge/mappers/mapper_069_sunsoft_fme7.hpp"
#include "systems/nes/cartridge/mappers/mapper_070_bandai.hpp"
#include "systems/nes/cartridge/mappers/mapper_071_camerica.hpp"
#include "systems/nes/cartridge/mappers/mapper_072_jaleco_jf17.hpp"
#include "systems/nes/cartridge/mappers/mapper_073_vrc3.hpp"
#include "systems/nes/cartridge/mappers/mapper_075_vrc1.hpp"
#include "systems/nes/cartridge/mappers/mapper_076_namcot3446.hpp"
#include "systems/nes/cartridge/mappers/mapper_077_irem_early.hpp"
#include "systems/nes/cartridge/mappers/mapper_078_jaleco_jf16.hpp"
#include "systems/nes/cartridge/mappers/mapper_079_nina.hpp"
#include "systems/nes/cartridge/mappers/mapper_080_taito_x1005.hpp"
#include "systems/nes/cartridge/mappers/mapper_082_taito_x1017.hpp"
#include "systems/nes/cartridge/mappers/mapper_085_vrc7.hpp"
#include "systems/nes/cartridge/mappers/mapper_086_jaleco_jf13.hpp"
#include "systems/nes/cartridge/mappers/mapper_087_jaleco_jf05.hpp"
#include "systems/nes/cartridge/mappers/mapper_namcot108.hpp"
#include "systems/nes/cartridge/mappers/mapper_089_sunsoft_early.hpp"
#include "systems/nes/cartridge/mappers/mapper_096_oeka_kids.hpp"
#include "systems/nes/cartridge/mappers/mapper_093_sunsoft2.hpp"
#include "systems/nes/cartridge/mappers/mapper_094_un1rom.hpp"
#include "systems/nes/cartridge/mappers/mapper_097_irem_tam_s1.hpp"
#include "systems/nes/cartridge/mappers/mapper_113_nina06.hpp"
#include "systems/nes/cartridge/mappers/mapper_118_txsrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_119_tqrom.hpp"
#include "systems/nes/cartridge/mappers/mapper_120_fds_hack.hpp"
#include "systems/nes/cartridge/mappers/mapper_133_sachen.hpp"
#include "systems/nes/cartridge/mappers/mapper_140_jaleco_jf11.hpp"
#include "systems/nes/cartridge/mappers/mapper_151_vrc1_vs.hpp"
#include "systems/nes/cartridge/mappers/mapper_152_bandai.hpp"
#include "systems/nes/cartridge/mappers/mapper_154_namcot3453.hpp"
#include "systems/nes/cartridge/mappers/mapper_156_dis_ic32.hpp"
#include "systems/nes/cartridge/mappers/mapper_180_unrom_reverse.hpp"
#include "systems/nes/cartridge/mappers/mapper_184_sunsoft1.hpp"
#include "systems/nes/cartridge/mappers/mapper_185_cnrom_protect.hpp"
#include "systems/nes/cartridge/mappers/mapper_189_mmc3_tfc.hpp"
#include "systems/nes/cartridge/mappers/mapper_193_ntdec.hpp"
#include "systems/nes/cartridge/mappers/mapper_210_namco175340.hpp"
#include "systems/nes/cartridge/mappers/mapper_225_multicart52.hpp"
#include "systems/nes/cartridge/mappers/mapper_226_multicart76.hpp"
#include "systems/nes/cartridge/mappers/mapper_227_multicart1200.hpp"
#include "systems/nes/cartridge/mappers/mapper_228_action52.hpp"
#include "systems/nes/cartridge/mappers/mapper_229_multicart31.hpp"
#include "systems/nes/cartridge/mappers/mapper_230_multicart22.hpp"
#include "systems/nes/cartridge/mappers/mapper_231_multicart20.hpp"
#include "systems/nes/cartridge/mappers/mapper_232_camerica_bf9096.hpp"
#include "systems/nes/cartridge/mappers/mapper_240_multicart.hpp"
#include "systems/nes/cartridge/mappers/mapper_245_waixing.hpp"

#include <memory>
#include <iostream>

namespace nes_system {

/// Factory for creating mapper instances by iNES mapper ID.
struct MapperFactory {
    /// Create a mapper instance for the given iNES mapper ID.
    /// Returns nullptr only in theory — unsupported IDs fall back to NROM.
    static inline std::unique_ptr<Mapper> create(uint16_t mapper_id,
                                                  uint16_t prg_banks,
                                                  uint16_t chr_banks) {
        switch (mapper_id) {
            // --- 000–004: Core mappers (NROM, SxROM/MMC1, UxROM, CNROM, TxROM/MMC3) ---
            case 0:   return std::make_unique<Mapper000>(prg_banks, chr_banks);
            case 1:   return std::make_unique<Mapper001>(prg_banks, chr_banks);
            case 2:   return std::make_unique<Mapper002>(prg_banks, chr_banks);
            case 3:   return std::make_unique<Mapper003>(prg_banks, chr_banks);
            case 4:   return std::make_unique<Mapper004>(prg_banks, chr_banks);
            case 5:   return std::make_unique<Mapper005>(prg_banks, chr_banks); // INCOMPLETE: no expansion audio ($5000-$5015), no vertical split mode
            // missing:  6 — FFE F4xxx (rare, FFE copier mapper)
            case 7:   return std::make_unique<Mapper007>(prg_banks, chr_banks);
            // missing:  8 — FFE F3xxx (rare, FFE copier mapper)
            case 9:   return std::make_unique<Mapper009>(prg_banks, chr_banks);
            case 10:  return std::make_unique<Mapper010>(prg_banks, chr_banks);
            case 11:  return std::make_unique<Mapper011>(prg_banks, chr_banks);
            // missing: 12 — DBZ5 (MMC3 variant, rare pirate)
            case 13:  return std::make_unique<Mapper013>(prg_banks, chr_banks);
            // missing: 14 — SL-1632 (rare pirate multicart)
            case 15:  return std::make_unique<Mapper015>(prg_banks, chr_banks);
            case 16:  return std::make_unique<Mapper016>(prg_banks, chr_banks);
            // missing: 17 — FFE F8xxx (rare, FFE copier mapper)
            case 18:  return std::make_unique<Mapper018>(prg_banks, chr_banks);
            case 19:  return std::make_unique<Mapper019>(prg_banks, chr_banks);
            // missing: 20 — FDS (Famicom Disk System, special hardware)
            case 21:  return std::make_unique<MapperVRC24<VRC4aTraits>>(prg_banks, chr_banks);
            case 22:  return std::make_unique<MapperVRC24<VRC2aTraits>>(prg_banks, chr_banks);
            case 23:  return std::make_unique<MapperVRC24<VRC24_023Traits>>(prg_banks, chr_banks);
            case 24:  return std::make_unique<Mapper024>(prg_banks, chr_banks);
            case 25:  return std::make_unique<MapperVRC24<VRC24_025Traits>>(prg_banks, chr_banks);
            case 26:  return std::make_unique<Mapper026>(prg_banks, chr_banks);
            // missing: 27 — VRC4 (pirate variant, rare)
            case 28:  return std::make_unique<Mapper028>(prg_banks, chr_banks);
            // missing: 29 — Sealie Computing (homebrew, rare)
            case 30:  return std::make_unique<Mapper030>(prg_banks, chr_banks);
            // missing: 31 — NSF-only (homebrew)
            case 32:  return std::make_unique<Mapper032>(prg_banks, chr_banks);
            case 33:  return std::make_unique<Mapper033>(prg_banks, chr_banks);
            case 34:  return std::make_unique<Mapper034>(prg_banks, chr_banks);
            // missing: 35–37 — rare/pirate/FDS-conversion mappers
            case 38:  return std::make_unique<Mapper038>(prg_banks, chr_banks);
            // missing: 39 — rare/pirate
            case 40:  return std::make_unique<Mapper040>(prg_banks, chr_banks);
            case 41:  return std::make_unique<Mapper041>(prg_banks, chr_banks);
            // missing: 42–46 — misc rare/pirate mappers
            case 47:  return std::make_unique<Mapper047>(prg_banks, chr_banks);
            case 48:  return std::make_unique<Mapper048>(prg_banks, chr_banks);
            // missing: 49–63 — misc rare/pirate mappers
            case 64:  return std::make_unique<Mapper064>(prg_banks, chr_banks);
            case 65:  return std::make_unique<Mapper065>(prg_banks, chr_banks);
            case 66:  return std::make_unique<Mapper066>(prg_banks, chr_banks);
            case 67:  return std::make_unique<Mapper067>(prg_banks, chr_banks);
            case 68:  return std::make_unique<Mapper068>(prg_banks, chr_banks);
            case 69:  return std::make_unique<Mapper069>(prg_banks, chr_banks); // no Yamaha 5B expansion audio
            case 70:  return std::make_unique<Mapper070>(prg_banks, chr_banks);
            case 71:  return std::make_unique<Mapper071>(prg_banks, chr_banks);
            case 72:  return std::make_unique<Mapper072>(prg_banks, chr_banks);
            case 73:  return std::make_unique<Mapper073>(prg_banks, chr_banks);
            // missing: 74 — Waixing (MMC3 variant, CHR-RAM pages, Chinese pirate)
            case 75:  return std::make_unique<Mapper075>(prg_banks, chr_banks);
            case 76:  return std::make_unique<Mapper076>(prg_banks, chr_banks);
            case 77:  return std::make_unique<Mapper077>(prg_banks, chr_banks);
            case 78:  return std::make_unique<Mapper078>(prg_banks, chr_banks);
            case 79:  return std::make_unique<Mapper079>(prg_banks, chr_banks);
            case 80:  return std::make_unique<Mapper080>(prg_banks, chr_banks);
            // missing: 81 — rare
            case 82:  return std::make_unique<Mapper082>(prg_banks, chr_banks);
            // missing: 83–84 — misc rare
            case 85:  return std::make_unique<Mapper085>(prg_banks, chr_banks);
            case 86:  return std::make_unique<Mapper086>(prg_banks, chr_banks);
            case 87:  return std::make_unique<Mapper087>(prg_banks, chr_banks);
            case 88:  return std::make_unique<MapperNamcot108<Namcot108Variant::ChrSplit>>(prg_banks, chr_banks);
            case 89:  return std::make_unique<Mapper089>(prg_banks, chr_banks);
            // missing: 90–92 — misc (90=J.Y. Company, 91=pirate, 92=Jaleco JF-19)
            case 93:  return std::make_unique<Mapper093>(prg_banks, chr_banks);
            case 94:  return std::make_unique<Mapper094>(prg_banks, chr_banks);
            case 95:  return std::make_unique<MapperNamcot108<Namcot108Variant::NtFromD5>>(prg_banks, chr_banks);
            case 96:  return std::make_unique<Mapper096>(prg_banks, chr_banks);
            case 97:  return std::make_unique<Mapper097>(prg_banks, chr_banks);
            // missing: 98–112 — misc rare/pirate/complex mappers
            case 113: return std::make_unique<Mapper113>(prg_banks, chr_banks);
            // missing: 114–117 — misc (115=MMC3 pirate, 116=multiboard, 117=rare)
            case 118: return std::make_unique<Mapper118>(prg_banks, chr_banks);
            case 119: return std::make_unique<Mapper119>(prg_banks, chr_banks);
            case 120: return std::make_unique<Mapper120>(prg_banks, chr_banks);
            // missing: 121–132 — misc rare/pirate mappers
            case 133: return std::make_unique<Mapper133>(prg_banks, chr_banks);
            // missing: 134–139 — misc rare/pirate mappers
            case 140: return std::make_unique<Mapper140>(prg_banks, chr_banks);
            // missing: 141–150 — misc rare/pirate mappers (148/149=Sachen)
            case 151: return std::make_unique<Mapper151>(prg_banks, chr_banks);
            case 152: return std::make_unique<Mapper152>(prg_banks, chr_banks);
            // missing: 153 — Bandai (rare, SRAM variant)
            case 154: return std::make_unique<Mapper154>(prg_banks, chr_banks);
            // missing: 155 — MMC1A (rare)
            case 156: return std::make_unique<Mapper156>(prg_banks, chr_banks);
            case 157: return std::make_unique<Mapper157>(prg_banks, chr_banks);
            // missing: 158 — rare
            case 159: return std::make_unique<Mapper159>(prg_banks, chr_banks);
            // missing: 160–179 — misc rare
            case 180: return std::make_unique<Mapper180>(prg_banks, chr_banks);
            // missing: 181–183 — misc rare
            case 184: return std::make_unique<Mapper184>(prg_banks, chr_banks);
            case 185: return std::make_unique<Mapper185>(prg_banks, chr_banks);
            // missing: 186–188 — misc rare/pirate mappers
            case 189: return std::make_unique<Mapper189>(prg_banks, chr_banks);
            // missing: 190–192 — misc rare/pirate mappers
            case 193: return std::make_unique<Mapper193>(prg_banks, chr_banks);
            // missing: 194–205 — misc rare/pirate mappers
            case 206: return std::make_unique<MapperNamcot108<Namcot108Variant::DxROM>>(prg_banks, chr_banks);
            case 207: return std::make_unique<Mapper207>(prg_banks, chr_banks);
            // missing: 208–209 — misc rare
            case 210: return std::make_unique<Mapper210>(prg_banks, chr_banks);
            // missing: 211–224 — misc rare
            case 225: return std::make_unique<Mapper225>(prg_banks, chr_banks);
            case 226: return std::make_unique<Mapper226>(prg_banks, chr_banks);
            case 227: return std::make_unique<Mapper227>(prg_banks, chr_banks);
            case 228: return std::make_unique<Mapper228>(prg_banks, chr_banks);
            case 229: return std::make_unique<Mapper229>(prg_banks, chr_banks);
            case 230: return std::make_unique<Mapper230>(prg_banks, chr_banks);
            case 231: return std::make_unique<Mapper231>(prg_banks, chr_banks);
            case 232: return std::make_unique<Mapper232>(prg_banks, chr_banks);
            // missing: 233–239 — misc multicarts/rare (234=Maxi 15)
            case 240: return std::make_unique<Mapper240>(prg_banks, chr_banks);
            // missing: 241–244 — misc multicarts/pirate
            case 245: return std::make_unique<Mapper245>(prg_banks, chr_banks);
            // missing: 246–255 — misc rare
            default:
                std::cout << "Warning: Unsupported mapper " << mapper_id
                          << ", falling back to NROM" << std::endl;
                return std::make_unique<Mapper000>(prg_banks, chr_banks);
        }
    }
};

} // namespace nes_system
