// mos8722_gui.cpp — MOS 8722 MMU chip layout (DIP-48)
//
// Pin assignments from the Commodore 128 service manual schematic.
//
// Pin groups:
//   A0-A15 (1-16)    CPU address bus inputs
//   GND (17)         Ground
//   D0-D6 (18-24)    Data bus (bidirectional, bits 0-6)
//   D7 (25)          Data bus bit 7 (right side)
//   /IO (26)         I/O area select output ($D000-$DFFF)
//   FSDIR (27)       Fast serial direction output
//   /Z80EN (28)      Z80 bus enable (active-low)
//   MA0-MA8 (29-37)  RAM address outputs (physical bank address)
//   /RAS1 (38)       Row address strobe, bank 1
//   /RAS0 (39)       Row address strobe, bank 0
//   /CAS (40)        Column address strobe (shared)
//   R/W (41)         Read/write direction
//   /ROML (42)       Low ROM chip select ($8000-$9FFF)
//   /ROMH (43)       High ROM chip select ($A000+/$E000+)
//   /GAME (44)       Game line (expansion port)
//   /EXROM (45)      External ROM (expansion port)
//   CLK (46)         2 MHz system clock
//   /RESET (47)      System reset
//   VCC (48)         +5V supply

#include "chip/mmu/mos8722.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* mos8722_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip48_layout();

        //           Left                         Right
        PIN_LR(layout,  1, A0,      VCC,      48);
        PIN_LR(layout,  2, A1,      _RESET,   47);
        PIN_LR(layout,  3, A2,      CLK,      46);
        PIN_LR(layout,  4, A3,      _EXROM,   45);
        PIN_LR(layout,  5, A4,      _GAME,    44);
        PIN_LR(layout,  6, A5,      _ROMH,    43);
        PIN_LR(layout,  7, A6,      _ROML,    42);
        PIN_LR(layout,  8, A7,      RW,       41);
        PIN_LR(layout,  9, A8,      _CAS,     40);
        PIN_LR(layout, 10, A9,      _RAS0,    39);
        PIN_LR(layout, 11, A10,     _RAS1,    38);
        PIN_LR(layout, 12, A11,     MA8,      37);
        PIN_LR(layout, 13, A12,     MA7,      36);
        PIN_LR(layout, 14, A13,     MA6,      35);
        PIN_LR(layout, 15, A14,     MA5,      34);
        PIN_LR(layout, 16, A15,     MA4,      33);
        PIN_LR(layout, 17, GND,     MA3,      32);
        PIN_LR(layout, 18, D0,      MA2,      31);
        PIN_LR(layout, 19, D1,      MA1,      30);
        PIN_LR(layout, 20, D2,      MA0,      29);
        PIN_LR(layout, 21, D3,      _Z80EN,   28);
        PIN_LR(layout, 22, D4,      FSDIR,    27);
        PIN_LR(layout, 23, D5,      _IO,      26);
        PIN_LR(layout, 24, D6,      D7,       25);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> mos8722_t::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}

#endif // CERMU_HAS_GUI
