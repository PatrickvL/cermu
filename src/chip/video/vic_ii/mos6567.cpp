#include "mos6567.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * NTSC VIC-II (MOS6567) lifecycle and bus attach wrappers
 */
vicii_t* mos6567_create() {
    const vicii_chip_config_t* config = vicii_get_default_config(false); // PAL = false (NTSC)
    vicii_t* vicii = vicii_create(config, vicii_memory_bank_change);
    return vicii;
}