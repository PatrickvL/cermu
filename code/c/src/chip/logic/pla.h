#ifndef PLA_H
#define PLA_H

#include "../../core/device.h"
#include <stdint.h>
#include <stdbool.h>

// Commodore PLA MOS 906114-01 REV3 8411
// https://www.c64-wiki.com/wiki/PLA_(C64_chip)
// http://skoe.de/docs/c64-dissected/pla/c64_pla_dissected_a4ss.pdf

// Input pins structure
typedef struct {
    bool a15, a14, a13, a12;  // A15-A12 - Address bus high bits
    bool n_charen;      // #CHAREN - Character ROM enable
    bool n_hiram;       // #HIRAM - High RAM enable  
    bool n_loram;       // #LORAM - Low RAM enable
    bool n_cas;         // #CAS - Column Address Strobe from VIC-II
    bool n_va14;        // #VA14 - VIC-II address bit 14
    bool aec;           // AEC - Address Enable Control
    bool ba;            // BA - Bus Available
    bool r_w;           // R/#W - Read/Write
    bool n_exrom;       // #EXROM - External ROM
    bool n_game;        // #GAME - Game cartridge
    bool va13, va12;    // VA13, VA12 - VIC-II address bits
    bool n_ce;          // #CE - Chip Enable
} pla_inputs_t;

// Output pins structure
typedef struct {
    bool n_romh;        // #ROMH - ROM High select
    bool n_roml;        // #ROML - ROM Low select  
    bool n_io;          // #I/O - I/O select
    bool gr_w;          // GR/#W - Color RAM write enable
    bool n_charrom;     // #CHARROM - Character ROM select
    bool n_kernal;      // #KERNAL - Kernal ROM select
    bool n_basic;       // #BASIC - Basic ROM select
    bool n_casram;      // #CASRAM - RAM CAS select
} pla_outputs_t;

typedef struct pla_906114_01_s {
    device_descriptor_t* desc;
    pla_inputs_t inputs;
    pla_outputs_t outputs;
} pla_906114_01_t;

// Function declarations
void* pla_906114_01_create(device_descriptor_t* desc);
void pla_906114_01_destroy(void* device);

// Main PLA logic function - updates all output lines based on input states
void pla_906114_01_update_outputs(pla_906114_01_t* pla);

// Address bus input (combines A15-A12)
void pla_906114_01_set_address_high(pla_906114_01_t* pla, uint8_t addr_high);

// VIC address bus input (combines VA13-VA12)
void pla_906114_01_set_vic_address(pla_906114_01_t* pla, uint8_t va_high);

extern device_descriptor_t pla_906114_01_descriptor;

#endif // PLA_H
