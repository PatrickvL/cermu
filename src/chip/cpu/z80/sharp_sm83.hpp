#pragma once
/*
 * sharp_sm83.hpp — Sharp SM83 (LR35902) CPU type — Game Boy
 *
 * The SM83 is the CPU in the Nintendo Game Boy (DMG-CPU, CGB-CPU).
 * It is Z80-derived but significantly reduced:
 *
 *   Missing vs Z80:
 *     - No index registers (IX, IY) — no DD/FD prefixes
 *     - No shadow register set (AF', BC', DE', HL', EX AF,AF', EXX)
 *     - No I/O instructions (IN, OUT) — no IORQ signal
 *     - No ED prefix block — no block transfer (LDIR etc.), no I/R regs
 *     - No parity/overflow flag (P/V) — unused bit
 *
 *   Unique to SM83:
 *     - SWAP r/SWAP (HL): swap upper and lower nybbles
 *     - STOP: enter low-power mode (also triggers GBC speed switch)
 *     - Different CB-prefix behavior (bit ops on (HL) are 16 T-states)
 *     - Simplified flag behavior (H flag = half-carry for add/sub only)
 *
 * 80-pin QFP SoC (not available as discrete CPU).
 */

#include "chip/cpu/z80/z80.hpp"

namespace z80 {

inline constexpr Z80Traits SharpSM83Traits = {
    "Sharp",                                              // vendor
    "SM83",                                               // chip_id
    "Sharp SM83 (LR35902)",                               // display_name
    CoreFlags::SM83_BASE,                                 // core_flags
    16,                                                   // address_bits
    42,                                                   // max_clock_mhz_x10 (4.194 MHz)
    {Z80SoundChip::NONE, Z80DMAType::NONE, false, false}  // peripheral
};

using SharpSM83 = z80_t<SharpSM83Traits>;

} // namespace z80

using SharpSM83 = z80::SharpSM83;
