#include "mos6569.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/**
 * PAL VIC-II (MOS6569) lifecycle and bus attach wrappers
 */
vicii_t* mos6569_create() {
    const vicii_chip_config_t* config = vicii_get_default_config(true); // PAL = true
    vicii_t* vicii = vicii_create(config, vicii_memory_bank_change);
    return vicii;
}
