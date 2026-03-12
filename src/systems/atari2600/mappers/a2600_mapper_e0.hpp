#pragma once
/*
 * a2600_mapper_e0.h — Atari 2600 E0 (Parker Brothers) bank switching
 *
 * 8KB ROM organized as 8 × 1KB slices. The 4KB cartridge window is
 * divided into 4 × 1KB segments, three switchable and one fixed:
 *
 *   $1000–$13FF  Segment 0 — switchable via $1FE0–$1FE7 (select slice 0–7)
 *   $1400–$17FF  Segment 1 — switchable via $1FE8–$1FEF (select slice 0–7)
 *   $1800–$1BFF  Segment 2 — switchable via $1FF0–$1FF7 (select slice 0–7)
 *   $1C00–$1FFF  Segment 3 — fixed to slice 7 (contains vectors + hotspots)
 *
 * Games: Montezuma's Revenge, Star Wars: The Empire Strikes Back,
 *        Popeye, Q*bert's Qubes, Super Cobra, Tooth Protectors.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600MapperE0 : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);
        uint8_t segment    = (offset >> 10) & 0x03;   // 0–3
        uint16_t seg_off   = offset & 0x03FF;          // offset within 1KB
        return rom_[static_cast<uint32_t>(slice_[segment]) * 0x0400 + seg_off];
    }

    void write(uint16_t offset, uint8_t data) override {
        (void)data;
        check_hotspot(offset);
    }

    const char* name() const override { return "E0"; }

    void reset() override {
        // Default: map slices 4–7 into segments 0–3
        slice_[0] = 4;
        slice_[1] = 5;
        slice_[2] = 6;
        slice_[3] = 7;  // Fixed — stays at slice 7
    }

    uint8_t current_bank() const override { return slice_[0]; }
    uint8_t bank_count() const override { return 8; }

private:
    uint8_t slice_[4] = { 4, 5, 6, 7 };

    inline void check_hotspot(uint16_t offset) {
        // Hotspots: $FE0–$FF7 in the 12-bit offset space
        // $FE0–$FE7 → segment 0, slice = low 3 bits
        // $FE8–$FEF → segment 1, slice = low 3 bits
        // $FF0–$FF7 → segment 2, slice = low 3 bits
        // Segment 3 is always fixed to slice 7
        if (offset >= 0x0FE0 && offset <= 0x0FF7) {
            uint8_t hotspot = static_cast<uint8_t>(offset - 0x0FE0);
            uint8_t segment = hotspot >> 3;   // 0–2
            uint8_t slice   = hotspot & 0x07; // 0–7
            slice_[segment] = slice;
        }
    }
};
