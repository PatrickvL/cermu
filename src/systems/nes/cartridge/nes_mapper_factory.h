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
            // missing: 16 — Bandai FCG (24C02 EEPROM, complex)
            // missing: 17 — FFE F8xxx (rare, FFE copier mapper)
            // missing: 18 — Jaleco SS 88006 (PRG/CHR/IRQ, moderate)
            // missing: 19 — Namco 163 (expansion audio + complex banking)
            // missing: 20 — FDS (Famicom Disk System, special hardware)
            // missing: 21 — VRC4a/VRC4c (Konami, complex PRG/CHR/IRQ)
            case 22:  return std::make_unique<Mapper022>(prg_banks, chr_banks);
            // missing: 23 — VRC2b/VRC4e (Konami, multiple sub-variants)
            // missing: 24 — VRC6a (Konami, expansion audio)
            // missing: 25 — VRC2c/VRC4b/VRC4d (Konami, multiple sub-variants)
            // missing: 26 — VRC6b (Konami, expansion audio)
            // missing: 27 — VRC4 (pirate variant, rare)
            case 28:  return std::make_unique<Mapper028>(prg_banks, chr_banks);
            // missing: 29 — Sealie Computing (homebrew, rare)
            // missing: 30 — UNROM 512 (homebrew flash cart)
            // missing: 31 — NSF-only (homebrew)
            case 32:  return std::make_unique<Mapper032>(prg_banks, chr_banks);
            case 33:  return std::make_unique<Mapper033>(prg_banks, chr_banks);
            case 34:  return std::make_unique<Mapper034>(prg_banks, chr_banks);
            // missing: 35–40 — rare/pirate/FDS-conversion mappers
            case 41:  return std::make_unique<Mapper041>(prg_banks, chr_banks);
            // missing: 42–63 — misc rare/pirate mappers (42=FDS hack, 46/47=multicarts, 48=Taito TC0690)
            case 64:  return std::make_unique<Mapper064>(prg_banks, chr_banks);
            case 65:  return std::make_unique<Mapper065>(prg_banks, chr_banks);
            case 66:  return std::make_unique<Mapper066>(prg_banks, chr_banks);
            case 67:  return std::make_unique<Mapper067>(prg_banks, chr_banks);
            case 68:  return std::make_unique<Mapper068>(prg_banks, chr_banks); // INCOMPLETE: CHR-ROM nametable replacement not wired
            case 69:  return std::make_unique<Mapper069>(prg_banks, chr_banks); // INCOMPLETE: no Yamaha 5B expansion audio; IRQ approximated via A12
            // missing: 70 — Bandai (simple, 16KB PRG + 8KB CHR)
            case 71:  return std::make_unique<Mapper071>(prg_banks, chr_banks);
            case 72:  return std::make_unique<Mapper072>(prg_banks, chr_banks);
            case 73:  return std::make_unique<Mapper073>(prg_banks, chr_banks);
            // missing: 74 — Waixing (MMC3 variant, CHR-RAM pages, Chinese pirate)
            case 75:  return std::make_unique<Mapper075>(prg_banks, chr_banks);
            // missing: 76 — Namco 3446 (NAMCOT-3446, rare)
            // missing: 77 — Irem (Napoleon Senki only)
            // missing: 78 — Jaleco JF-16 (Cosmo Carrier / Holy Diver)
            case 79:  return std::make_unique<Mapper079>(prg_banks, chr_banks);
            // missing: 80 — Taito X1-005 (PRG-RAM + CHR banking, moderate)
            // missing: 81–85 — misc (82=Taito X1-017, 85=VRC7 Konami w/ expansion audio)
            // missing: 86 — Jaleco JF-13 (simple, PRG+CHR)
            case 87:  return std::make_unique<Mapper087>(prg_banks, chr_banks);
            case 88:  return std::make_unique<Mapper088>(prg_banks, chr_banks);
            // missing: 89 — Sunsoft (simple, 16KB PRG + 8KB CHR + mirror)
            // missing: 90–92 — misc (90=J.Y. Company, 91=pirate, 92=Jaleco JF-19)
            case 93:  return std::make_unique<Mapper093>(prg_banks, chr_banks);
            // missing: 94 — UN1ROM (simple, 16KB PRG only, Senjou no Ookami)
            case 95:  return std::make_unique<Mapper095>(prg_banks, chr_banks);
            // missing: 96 — Oeka Kids (special input, Bandai)
            // missing: 97 — Irem TAM-S1 (simple, Kaiketsu Yanchamaru)
            // missing: 98–112 — misc rare/pirate/complex mappers
            case 113: return std::make_unique<Mapper113>(prg_banks, chr_banks);
            // missing: 114–117 — misc (115=MMC3 pirate, 116=multiboard, 117=rare)
            case 118: return std::make_unique<Mapper118>(prg_banks, chr_banks);
            case 119: return std::make_unique<Mapper119>(prg_banks, chr_banks);
            // missing: 120–132 — misc rare/pirate mappers
            case 133: return std::make_unique<Mapper133>(prg_banks, chr_banks);
            // missing: 134–150 — misc rare/pirate mappers (140=Jaleco JF-11, 148/149=Sachen)
            case 151: return std::make_unique<Mapper151>(prg_banks, chr_banks);
            // missing: 152 — Bandai (simple, like 70 with mirror control)
            // missing: 153–183 — misc (154=Namco 3453, 159=Bandai LZ93D50, 180=UNROM reverse)
            case 184: return std::make_unique<Mapper184>(prg_banks, chr_banks);
            case 185: return std::make_unique<Mapper185>(prg_banks, chr_banks);
            // missing: 186–192 — misc rare/pirate mappers
            case 193: return std::make_unique<Mapper193>(prg_banks, chr_banks);
            // missing: 194–224 — misc (206=DxROM/Namcot-108, 210=Namco 175/340)
            case 225: return std::make_unique<Mapper225>(prg_banks, chr_banks);
            case 226: return std::make_unique<Mapper226>(prg_banks, chr_banks);
            case 227: return std::make_unique<Mapper227>(prg_banks, chr_banks);
            case 228: return std::make_unique<Mapper228>(prg_banks, chr_banks);
            case 229: return std::make_unique<Mapper229>(prg_banks, chr_banks);
            // missing: 230 — multicart (22-in-1, Contra + reset-swap)
            case 231: return std::make_unique<Mapper231>(prg_banks, chr_banks);
            case 232: return std::make_unique<Mapper232>(prg_banks, chr_banks);
            // missing: 233–255 — misc multicarts/rare (234=Maxi 15, 240–243=various simple/pirate)
            default:
                std::cout << "Warning: Unsupported mapper " << (int)mapper_id
                          << ", falling back to NROM" << std::endl;
                return std::make_unique<Mapper000>(prg_banks, chr_banks);
        }
    }
};

} // namespace nes_system
