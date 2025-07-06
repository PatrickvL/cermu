#ifndef PLA_H
#define PLA_H

#include <stdint.h>
#include <stdbool.h>

#include "../../core/chip.h"

// PLA chip descriptor for GUI integration
extern chip_descriptor_t pla_descriptor;

// Commodore PLA MOS 906114-01 REV3 8411
// https://www.c64-wiki.com/wiki/PLA_(C64_chip)
// http://skoe.de/docs/c64-dissected/pla/c64_pla_dissected_a4ss.pdf

// PLA MOS 906114-01 DIP has 28 pins; Pinout :

// Input pins structure
typedef struct {
    // Left hand side, top>down pins:
    // Not needed:      pin  1/FE : N.C.(FE) - Used for programming field-programmable parts and not connected internally for mask-programmable parts
    bool a13;        // pin  2/I7 : A13 - Address bus high bits
    bool a14;        // pin  3/I6 : A14 - Address bus high bits
    bool a15;        // pin  4/I5 : A15 - Address bus high bits
    bool n_va14;     // pin  5/I4 : #VA14 - VIC-II address bit 14 (inverted)
    bool n_charen;   // pin  6/I3 : #CHAREN - Character ROM enable
    bool n_hiram;    // pin  7/I2 : #HIRAM - High RAM enable  
    bool n_loram;    // pin  8/I1 : #LORAM - Low RAM enable
    bool n_cas;      // pin  9/I0 : #CAS - Column Address Strobe from VIC-II
    // Note : pin 10 to 14 are outputs (see below)

    // Right hand side, bottom>up pins:
    // Note : pin 15 to 18 are outputs (see below)
    // Not needed:      pin 19/CE : CE - Chip Enable
    bool va12;       // pin 20/I15: VA12 - VIC-II address bits
    bool va13;       // pin 21/I14: VA13 - VIC-II address bits
    bool n_game;     // pin 22/I13: #GAME - Game cartridge
    bool n_exrom;    // pin 23/I12: #EXROM - External ROM
    bool r_w;        // pin 24/I11: R/#W - Read/Write
    bool n_aec;      // pin 25/I10: #AEC - Address Enable Control
    bool ba;         // pin 26/I9 : BA - Bus Available
    bool a12;        // pin 27/I8 : A12 - Address bus high bits
    // Not needed:      pin 28/VCC : Plus5Volt - +5V
} pla_inputs_t;

// Output pins structure
typedef struct {
    // Left hand side, top>down pins:
    bool n_romh;     // pin 10/F7 : #ROMH - ROM High select
    bool n_roml;     // pin 11/F6 : #ROML - ROM Low select  
    bool n_io;       // pin 12/F5 : #I/O - I/O select
    bool gr_w;       // pin 13/F4 : GR/#W - Color RAM write enable (Connected to #WE on the color RAM)
    // Not needed:      pin 14/VSS : GND - Ground

    // Right hand side, bottom>up pins:
    bool n_charrom;  // pin 15/F3 : #CHARROM - Character ROM select
    bool n_kernal;   // pin 16/F2 : #KERNAL - Kernal ROM select
    bool n_basic;    // pin 17/F1 : #BASIC - Basic ROM select
    bool n_casram;   // pin 18/F0 : #CASRAM - RAM CAS select
} pla_outputs_t;

typedef struct pla_906114_01_s {
    pla_inputs_t inputs;
    pla_outputs_t outputs;
} pla_906114_01_t;

// Function declarations
pla_906114_01_t* pla_906114_01_create(void);
void pla_906114_01_destroy(pla_906114_01_t* pla);

// Main PLA logic function - updates all output lines based on input states
void pla_906114_01_update_outputs(pla_906114_01_t* pla);

// Address bus input (combines A15-A12)
void pla_906114_01_set_cpu_address_bank(pla_906114_01_t* pla, uint8_t addr_high);

// VIC address bus input (combines VA13-VA12)
void pla_906114_01_set_vicii_address_bank(pla_906114_01_t* pla, uint8_t va_high);

// Convert PLA output signals to ACID (Accessor ID) values
uint8_t pla_906114_01_outputs_to_acid(pla_906114_01_t* pla);

// GUI debug window
void pla_render_debug_window(void* chip, bool* show_window);

#endif // PLA_H
