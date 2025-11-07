//#define DEBUG_PLA_BANKING

#include "pla.h"
#include "../../core/chip.h"
#include "../../systems/c64/c64_bus.h"
#ifdef DEBUG_PLA_BANKING
#include <stdio.h>
#endif

// ============================================================================
// PLA CHIP DESCRIPTOR FOR GUI INTEGRATION
// ============================================================================

// PLA create function - returns NULL since PLA is part of the C64 bus system
static void* pla_create(chip_descriptor_t* desc) {
    // PLA is not a standalone chip - it's part of the C64 bus system
    return NULL;
}

// PLA destroy function - no-op since PLA is part of the bus
static void pla_destroy(void* chip) {
    // PLA is part of the C64 system, not destroyed separately
}

#ifdef IMGUI_VERSION
// Forward declaration for GUI function
void pla_render_debug_window(void* chip, bool* show_window);
#endif

// PLA chip descriptor
chip_descriptor_t pla_descriptor = {
    .description = "PLA (Programmable Logic Array)",
    .create = pla_create,
    .destroy = pla_destroy,
    .bus_attach = NULL,
    .bank_change = NULL,
#ifdef IMGUI_VERSION
    .render_debug_window = pla_render_debug_window,
    .render_settings_window = NULL
#endif
};

// ============================================================================
// PLA LOGIC IMPLEMENTATION
// ============================================================================
#include <stdlib.h>
#include <string.h>

// Commodore PLA MOS 906114-01 implementation
// Based on detailed analysis from C64 wiki and dissection documents

pla_906114_01_t* pla_906114_01_create(void) {
    pla_906114_01_t* pla = (pla_906114_01_t*)calloc(1, sizeof(pla_906114_01_t));
    if (!pla) return NULL;
    
    // Set default input states (typical C64 boot state)
    pla->inputs.n_charen = true;    // Character ROM disabled initially
    pla->inputs.n_hiram = true;     // High RAM enabled
    pla->inputs.n_loram = true;     // Low RAM enabled
    pla->inputs.n_cas = true;       // No CAS initially
    pla->inputs.n_va14 = true;      // VA14 high
    pla->inputs.n_aec = false;      // CPU has bus control
    pla->inputs.ba = true;          // Bus available
    pla->inputs.r_w = true;         // Read mode
    pla->inputs.n_exrom = true;     // No external ROM
    pla->inputs.n_game = true;      // No game cartridge
    pla->inputs.va13 = false;       // VA13 low
    pla->inputs.va12 = false;       // VA12 low
    
    // Update outputs based on initial inputs
    pla_906114_01_update_outputs(pla);
    
    return pla;
}

void pla_906114_01_destroy(pla_906114_01_t* pla) {
    free(pla);
}

// Check for Ultimax mode (#GAME = 0, #EXROM = 1)
bool pla_906114_01_is_ultimax_mode(pla_906114_01_t* pla) {
    return pla->inputs.n_exrom && !pla->inputs.n_game;
}

void pla_906114_01_set_cpu_address_bank(pla_906114_01_t* pla, uint8_t high_nybble) {
    if (pla_906114_01_is_ultimax_mode(pla)) {
        // "The address bus lines A15 to A12 (I5 to I8) are connected to the global address bus.
        // They are driven by the CPU when AEC from the VIC-II and #DMA from the Expansion
        // Port are both high. During VIC-II cycles, when AEC is low, they are pulled up by RP4.
        // When they are pulled up by this resistor array only, it is possible to change them from  the Expansion Port.
        // These address lines are used by the PLA to control the memory
        // mapping when AEC is high, i.e. during CPU cycles. However,
        // they are also evaluated in the PLA in Ultimax mode when AEC is low"
        // (which means when the VIC-II has bus control).
        if (pla->inputs.n_aec) {
            // "The address lines A12 to A15 of the C64 address
            // bus are pulled up by RP4 whenever the VIC-II has
            // the bus, so they are %1111 usually"
            uint8_t rp4 = 0x0F; // RP4 pulls A12-A15 high in Ultimax mode TODO : Let cardridge / exrom set this

            high_nybble = rp4;
        }
    }

    pla->inputs.a12 = (high_nybble & 0x01) != 0;
    pla->inputs.a13 = (high_nybble & 0x02) != 0;
    pla->inputs.a14 = (high_nybble & 0x04) != 0;
    pla->inputs.a15 = (high_nybble & 0x08) != 0;
    
    pla_906114_01_update_outputs(pla);
}    

void pla_906114_01_set_vicii_address_bank(pla_906114_01_t* pla, uint8_t high_nybble) {
    pla->inputs.va12 = (high_nybble & 0x01) != 0;
    pla->inputs.va13 = (high_nybble & 0x02) != 0;
    // Handle VA14 line for different configurations
    // For banks 0-3: VA14 = 0, For banks 4-7: VA14 = 0
    // For banks 8-11: VA14 = 1, For banks 12-15: VA14 = 1
    pla->inputs.n_va14 = (high_nybble & 0x04) == 0; // VA14 is inverted in the PLA
    // Note that the VIC-II itself only has 14 address lines,
    // which can only address 16KB of memory. However, VIC-II
    // memory accesses use the upper 2 bits (VA14, VA15) from CIA2,
    // as set via vicii_bank_change().
    // Here, we don't care since we're only initializing the
    // VIC-II bank mapping using the PLA logic.   

    // Also apply the same bank bits to the a12-a15 :
    pla_906114_01_set_cpu_address_bank(pla, high_nybble);
}

void pla_906114_01_set_cardrigde_mode(pla_906114_01_t* pla, uint8_t high_nybble) {
    // "#EXROM, #GAME (I12, I13)
    // The two lines #EXROM and #GAME can be pulled down by cartridges to change the
    // memory map of the C64, e.g., to map external ROM into the address space. When they
    // are not pulled down from the cartridge port, resistors in RP4 pull them up."
}

void pla_906114_01_update_outputs(pla_906114_01_t* pla) {
    #ifdef DEBUG_PLA_BANKING
    static int debug_call_count = 0;
    debug_call_count++;
    if (debug_call_count <= 5) {
        printf("PLA update_outputs called %d times\n", debug_call_count);
    }
    #endif
    
    // Input state (using same variable names as C# code for clarity)
    bool a12 = pla->inputs.a12;
    bool a13 = pla->inputs.a13;
    bool a14 = pla->inputs.a14;
    bool a15 = pla->inputs.a15;
    bool va12 = pla->inputs.va12;
    bool va13 = pla->inputs.va13;
    bool n_va14 = pla->inputs.n_va14;
    bool n_aec = pla->inputs.n_aec;
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
    pla->outputs.n_grw = !p31;
    pla->outputs.n_io = !(p9 || p10 || p11 || p12 || p13 || p14 ||
                  p15 || p16 || p17 || p18);
    pla->outputs.n_roml = !(p19 || p20);
    pla->outputs.n_romh = !(p21 || p22 || p23);
#ifdef DEBUG_PLA_BANKING
    // Debug output for banking issue
    if (a15 == 0 && a14 == 0 && a13 == 0 && a12 == 0) { // Bank 0
        printf("PLA Bank 0: n_casram=%d n_basic=%d n_kernal=%d n_charrom=%d n_io=%d n_roml=%d n_romh=%d\n",
               pla->outputs.n_casram, pla->outputs.n_basic, pla->outputs.n_kernal, 
               pla->outputs.n_charrom, pla->outputs.n_io, pla->outputs.n_roml, pla->outputs.n_romh);
        printf("PLA Inputs: n_loram=%d n_hiram=%d n_charen=%d n_exrom=%d n_game=%d n_aec=%d r_w=%d n_cas=%d\n",
               pla->inputs.n_loram, pla->inputs.n_hiram, pla->inputs.n_charen,
               pla->inputs.n_exrom, pla->inputs.n_game, pla->inputs.n_aec, pla->inputs.r_w, pla->inputs.n_cas);
        
        // Check which product terms are active for n_casram
        bool casram_terms[] = {p0, p1, p2, p3, p4, p5, p6, p7, false, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25, p26, p27, p28, false, false, false};
        printf("Active CASRAM terms: ");
        for (int i = 0; i < 32; i++) {
            if (casram_terms[i]) printf("p%d ", i);
        }
        printf("\n");
    }
#endif
}
