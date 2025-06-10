#ifndef MOS6581_H
#define MOS6581_H

#include "../../core/chip.h"

// MOS6581, also known as SID (Sound Interface Device),
// is a sound chip used in the Commodore 64.
// It provides three oscillators, an envelope generator, and various filters.

typedef struct mos6581_s {
    chip_descriptor_t* desc;
    uint8_t registers[29];
    uint16_t envelope_counter[3];
    uint8_t envelope_state[3];
    uint8_t pot_x, pot_y;
    uint8_t osc3, env3;
} mos6581_t;

// Function declarations
void mos6581_cycle(mos6581_t* sid);

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI function declarations
void mos6581_render_debug_window(void* chip, bool* show_window);
void mos6581_render_settings_window(void* chip, bool* show_window);
#endif

extern chip_descriptor_t mos6581_descriptor;

#endif // MOS6581_H