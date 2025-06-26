#include "mos6502_opcodes.h"
#include "../fam65xx/fam65xx_arithmetic.h"
#include <stdio.h>

// ============================================================================
// OPCODE TABLE INITIALIZATION
// ============================================================================

void mos6502_init_opcode_table(mos6502_t* cpu) {
    if (!cpu) return;
    // Only set up the base table with decimal mode and illegal opcode support.
    // Do NOT override ADC/SBC handlers here; let fam65xx_init_opcode_table handle it based on the feature flag.
    uint32_t features = FAM65XX_FEATURE_DECIMAL_MODE | FAM65XX_FEATURE_ILLEGAL_OPCODES;
    fam65xx_init_opcode_table(&cpu->base, features);
}
