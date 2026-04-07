//#define DEBUG_PLA_BANKING

#include "core/cermu.hpp"
#include "chip/logic/pla.hpp"
#ifdef DEBUG_PLA_BANKING
#include <cstdio>
#endif

// ============================================================================
// PLA906114 METHOD IMPLEMENTATIONS
// ============================================================================
// Commodore PLA MOS 906114-01 implementation
// Based on detailed analysis from C64 wiki and dissection documents

void PLA906114::set_cpu_address_bank(uint8_t high_nybble) {
    if (is_ultimax_mode()) {
        // "The address bus lines A15 to A12 (I5 to I8) are connected to the global address bus.
        // They are driven by the CPU when AEC from the VIC-II and #DMA from the Expansion
        // Port are both high. During VIC-II cycles, when AEC is low, they are pulled up by RP4.
        // When they are pulled up by this resistor array only, it is possible to change them from  the Expansion Port.
        // These address lines are used by the PLA to control the memory
        // mapping when AEC is high, i.e. during CPU cycles. However,
        // they are also evaluated in the PLA in Ultimax mode when AEC is low"
        // (which means when the VIC-II has bus control).
        if (inputs_.n_aec) {
            // "The address lines A12 to A15 of the C64 address
            // bus are pulled up by RP4 whenever the VIC-II has
            // the bus, so they are %1111 usually"
            // RP4 pulls A12-A15 high when VIC-II has the bus.
            // A cartridge could override individual lines via expansion port
            // bus contention, but no known cartridge does this in practice.
            uint8_t rp4 = 0x0F;

            high_nybble = rp4;
        }
    }

    inputs_.a12 = (high_nybble & 0x01) != 0;
    inputs_.a13 = (high_nybble & 0x02) != 0;
    inputs_.a14 = (high_nybble & 0x04) != 0;
    inputs_.a15 = (high_nybble & 0x08) != 0;

    update_outputs();
}

void PLA906114::set_vicii_address_bank(uint8_t high_nybble) {
    inputs_.va12 = (high_nybble & 0x01) != 0;
    inputs_.va13 = (high_nybble & 0x02) != 0;
    // #VA14 reflects the corresponding address bit (bit 2 of high_nybble = bit 14 of address)
    // Note: The signal is active-low (#VA14), so it's inverted from the address bit
    inputs_.n_va14 = (high_nybble & 0x04) == 0;

    // Also apply the same bank bits to the a12-a15 for address decoding:
    set_cpu_address_bank(high_nybble);
}

void PLA906114::set_banking_mode(uint8_t mode) {
    // Positive logic: bit set = feature enabled = PLA variable true.
    // The n_ prefix in the PLA struct refers to the signal name, not the
    // variable's polarity — see c64_bus.cpp generate_all_pla_modes() for
    // the detailed explanation.
    inputs_.n_loram  = (mode & 0x01) != 0;
    inputs_.n_hiram  = (mode & 0x02) != 0;
    inputs_.n_charen = (mode & 0x04) != 0;
    inputs_.n_exrom  = (mode & 0x08) != 0;
    inputs_.n_game   = (mode & 0x10) != 0;
}

void PLA906114::tick(bus_state_t bus_state) {
    // Extract address bus bits A12-A15 from bus state
    uint16_t addr = BUS_GET_ADDR(bus_state);
    inputs_.a12 = (addr & 0x1000) != 0;
    inputs_.a13 = (addr & 0x2000) != 0;
    inputs_.a14 = (addr & 0x4000) != 0;
    inputs_.a15 = (addr & 0x8000) != 0;

    // Control signals from bus state
    inputs_.r_w   = BUS_GET_BIT(bus_state, BUS_RW_BIT);
    inputs_.n_aec = !BUS_GET_BIT(bus_state, BUS_AEC_BIT); // AEC active-high in bus_state, n_aec in PLA
    inputs_.ba    = BUS_GET_BIT(bus_state, BUS_BA_BIT);

    // In Ultimax mode, RP4 pulls A12-A15 high when VIC-II has the bus
    // (n_aec = true means AEC is low, i.e. VIC-II is driving).
    if (is_ultimax_mode() && inputs_.n_aec) {
        inputs_.a12 = true;
        inputs_.a13 = true;
        inputs_.a14 = true;
        inputs_.a15 = true;
    }

    update_outputs();
}

void PLA906114::update_outputs() {
    #ifdef DEBUG_PLA_BANKING
    static int debug_call_count = 0;
    debug_call_count++;
    if (debug_call_count <= 5) {
        log_info("PLA update_outputs called %d times\n", debug_call_count);
    }
    #endif

    // Input state (using same variable names as C# code for clarity)
    bool a12 = inputs_.a12;
    bool a13 = inputs_.a13;
    bool a14 = inputs_.a14;
    bool a15 = inputs_.a15;
    bool va12 = inputs_.va12;
    bool va13 = inputs_.va13;
    bool n_va14 = inputs_.n_va14;
    bool n_aec = inputs_.n_aec;
    bool n_cas = inputs_.n_cas;
    bool n_charen = inputs_.n_charen;
    bool n_exrom = inputs_.n_exrom;
    bool n_game = inputs_.n_game;
    bool n_hiram = inputs_.n_hiram;
    bool n_loram = inputs_.n_loram;
    bool rd = inputs_.r_w;  // read mode when high
    bool ba = inputs_.ba;

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
    outputs_.n_casram = (p0 || p1 || p2 ||
                     p3 || p4 || p5 || p6 || p7 ||
                     p9 || p10 || p11 || p12 || p13 ||
                     p14 || p15 || p16 || p17 || p18 ||
                     p19 || p20 || p21 || p22 || p23 ||
                     p24 || p25 || p26 || p27 || p28 || p30);

    outputs_.n_basic = !p0;
    outputs_.n_kernal = !(p1 || p2);
    outputs_.n_charrom = !(p3 || p4 || p5 || p6 || p7);
    outputs_.n_grw = !p31;
    outputs_.n_io = !(p9 || p10 || p11 || p12 || p13 || p14 ||
                  p15 || p16 || p17 || p18);
    outputs_.n_roml = !(p19 || p20);
    outputs_.n_romh = !(p21 || p22 || p23);

    sync_regs();

#ifdef DEBUG_PLA_BANKING
    // Debug output for banking issue
    if (a15 == 0 && a14 == 0 && a13 == 0 && a12 == 0) { // Bank 0
        log_info("PLA Bank 0: n_casram=%d n_basic=%d n_kernal=%d n_charrom=%d n_io=%d n_roml=%d n_romh=%d\n",
               outputs_.n_casram, outputs_.n_basic, outputs_.n_kernal,
               outputs_.n_charrom, outputs_.n_io, outputs_.n_roml, outputs_.n_romh);
        log_info("PLA Inputs: n_loram=%d n_hiram=%d n_charen=%d n_exrom=%d n_game=%d n_aec=%d r_w=%d n_cas=%d\n",
               inputs_.n_loram, inputs_.n_hiram, inputs_.n_charen,
               inputs_.n_exrom, inputs_.n_game, inputs_.n_aec, inputs_.r_w, inputs_.n_cas);

        // Check which product terms are active for n_casram
        bool casram_terms[] = {p0, p1, p2, p3, p4, p5, p6, p7, false, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25, p26, p27, p28, false, false, false};
        log_info("Active CASRAM terms: ");
        for (int i = 0; i < 32; i++) {
            if (casram_terms[i]) log_info("p%d ", i);
        }
        log_info("\n");
    }
#endif
}
