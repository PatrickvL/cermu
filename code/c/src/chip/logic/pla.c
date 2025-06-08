#include "pla.h"
#include <stdlib.h>
#include <string.h>

// Commodore PLA MOS 906114-01 implementation
// Based on detailed analysis from C64 wiki and dissection documents

void* pla_906114_01_create(device_descriptor_t* desc) {
    pla_906114_01_t* pla = (pla_906114_01_t*)calloc(1, sizeof(pla_906114_01_t));
    if (!pla) return NULL;
    
    pla->desc = desc;
    
    // Set default input states (typical C64 boot state)
    pla->inputs.n_charen = true;    // Character ROM disabled initially
    pla->inputs.n_hiram = true;     // High RAM enabled
    pla->inputs.n_loram = true;     // Low RAM enabled
    pla->inputs.n_cas = true;       // No CAS initially
    pla->inputs.n_va14 = true;      // VA14 high
    pla->inputs.aec = true;         // CPU has bus control
    pla->inputs.ba = true;          // Bus available
    pla->inputs.r_w = true;         // Read mode
    pla->inputs.n_exrom = true;     // No external ROM
    pla->inputs.n_game = true;      // No game cartridge
    pla->inputs.va13 = false;       // VA13 low
    pla->inputs.va12 = false;       // VA12 low
    pla->inputs.n_ce = false;       // Chip enabled
    
    // Update outputs based on initial inputs
    pla_906114_01_update_outputs(pla);
    
    return pla;
}

void pla_906114_01_destroy(void* device) {
    free(device);
}

void pla_906114_01_set_address_high(pla_906114_01_t* pla, uint8_t addr_high) {
    pla->inputs.a12 = (addr_high & 0x01) != 0;
    pla->inputs.a13 = (addr_high & 0x02) != 0;
    pla->inputs.a14 = (addr_high & 0x04) != 0;
    pla->inputs.a15 = (addr_high & 0x08) != 0;
    
    pla_906114_01_update_outputs(pla);
}

void pla_906114_01_set_vic_address(pla_906114_01_t* pla, uint8_t va_high) {
    pla->inputs.va12 = (va_high & 0x01) != 0;
    pla->inputs.va13 = (va_high & 0x02) != 0;
    
    pla_906114_01_update_outputs(pla);
}

void pla_906114_01_update_outputs(pla_906114_01_t* pla) {
    // Input state (using same variable names as C# code for clarity)
    bool a12 = pla->inputs.a12;
    bool a13 = pla->inputs.a13;
    bool a14 = pla->inputs.a14;
    bool a15 = pla->inputs.a15;
    bool va12 = pla->inputs.va12;
    bool va13 = pla->inputs.va13;
    bool n_va14 = !pla->inputs.n_va14;  // Note: inverted in hardware
    bool aec = pla->inputs.aec;
    bool n_aec = !aec;
    bool n_cas = pla->inputs.n_cas;
    bool n_charen = pla->inputs.n_charen;
    bool n_exrom = pla->inputs.n_exrom;
    bool n_game = pla->inputs.n_game;
    bool n_hiram = pla->inputs.n_hiram;
    bool n_loram = pla->inputs.n_loram;
    bool rd = pla->inputs.r_w;  // read mode when high
    bool ba = pla->inputs.ba;

    // Product Term for #BASIC
    bool p0 = n_loram & n_hiram &
              a15 & !a14 & a13 &
              !n_aec & rd & n_game;

    // Product Terms for #KERNAL
    bool p1 = n_hiram &
              a15 & a14 & a13 &
              !n_aec & rd & n_game;

    bool p2 = n_hiram &
              a15 & a14 & a13 &
              !n_aec & rd & !n_exrom & !n_game;

    // Product Terms for #CHARROM
    bool p3 = n_hiram & !n_charen &
              a15 & a14 & !a13 & a12 &
              !n_aec & rd & n_game;

    bool p4 = n_loram & !n_charen &
              a15 & a14 & !a13 & a12 &
              !n_aec & rd & n_game;

    bool p5 = n_hiram & !n_charen &
              a15 & a14 & !a13 & a12 &
              !n_aec & rd & !n_exrom & !n_game;

    bool p6 = n_va14 & !va13 & va12 &
              n_aec & n_game;

    bool p7 = n_va14 & !va13 & va12 &
              n_aec & !n_exrom & !n_game;

    // Product Terms for #IO
    bool p9 = n_hiram & n_charen &
              a15 & a14 & !a13 & a12 &
              !n_aec & ba & rd & n_game;

    bool p10 = n_hiram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & !rd & n_game;

    bool p11 = n_loram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & ba & rd & n_game;

    bool p12 = n_loram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & !rd & n_game;

    bool p13 = n_hiram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & ba & rd &
               !n_exrom & !n_game;

    bool p14 = n_hiram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & !rd &
               !n_exrom & !n_game;

    bool p15 = n_loram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & ba & rd &
               !n_exrom & !n_game;

    bool p16 = n_loram & n_charen &
               a15 & a14 & !a13 & a12 &
               !n_aec & !rd &
               !n_exrom & !n_game;

    bool p17 = a15 & a14 & !a13 & a12 &
               !n_aec & ba & rd &
               n_exrom & !n_game;

    bool p18 = a15 & a14 & !a13 & a12 &
               !n_aec & !rd & n_exrom & !n_game;

    // Product Terms for #ROML
    bool p19 = n_loram & n_hiram &
               a15 & !a14 & !a13 &
               !n_aec & rd & !n_exrom;

    bool p20 = a15 & !a14 & !a13 &
               !n_aec & n_exrom & !n_game;

    // Product Terms for #ROMH
    bool p21 = n_hiram &
               a15 & !a14 & a13 &
               !n_aec & rd & !n_exrom & !n_game;

    bool p22 = a15 & a14 & a13 &
               !n_aec & n_exrom & !n_game;

    bool p23 = va13 & va12 &
               n_aec & n_exrom & !n_game;

    // Additional Product Terms for #CASRAM (Ultimax mode RAM hiding)
    bool p24 = !a15 & !a14 & a12 &
               n_exrom & !n_game;

    bool p25 = !a15 & !a14 & a13 &
               n_exrom & !n_game;

    bool p26 = !a15 & a14 &
               n_exrom & !n_game;

    bool p27 = a15 & !a14 & a13 &
               n_exrom & !n_game;

    bool p28 = a15 & a14 & !a13 & !a12 &
               n_exrom & !n_game;

    // Product Term to Forward #CAS to #CASRAM
    bool p30 = n_cas;

    // Product Term for #GRW (Color RAM write)
    bool p31 = !n_cas &
               a15 & a14 & !a13 & a12 &
               !n_aec & !rd;

    // Sum Terms - Calculate outputs
    pla->outputs.n_casram = (p0 || p1 || p2 ||
                     p3 || p4 || p5 || p6 || p7 ||
                     p9 || p10 || p11 || p12 || p13 ||
                     p14 || p15 || p16 || p17 || p18 ||
                     p19 || p20 || p21 || p22 || p23 ||
                     p24 || p25 || p26 || p27 || p28 || p30);

    pla->outputs.n_basic = !p0;
    pla->outputs.n_kernal = !(p1 || p2);
    pla->outputs.n_charrom = !(p3 || p4 || p5 || p6 || p7);
    pla->outputs.gr_w = p31;
    pla->outputs.n_io = !(p9 || p10 || p11 || p12 || p13 || p14 ||
                  p15 || p16 || p17 || p18);
    pla->outputs.n_roml = !(p19 || p20);
    pla->outputs.n_romh = !(p21 || p22 || p23);
}

device_descriptor_t pla_906114_01_descriptor = {
    .create = pla_906114_01_create,
    .destroy = pla_906114_01_destroy,
    .bus_attach = NULL,
    .read = NULL,
    .write = NULL,
    .bank_change = NULL
};
