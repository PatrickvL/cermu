#ifndef MOS6510_GUI_H
#define MOS6510_GUI_H

#include "mos6510.h"

#ifdef __cplusplus
extern "C" {
#endif

// MOS6510-specific GUI functions
void mos6510_render_debug_window(void* chip, bool* show_window);
void mos6510_render_cpu_specific(void* chip);

#ifdef __cplusplus
}
#endif

#endif // MOS6510_GUI_H
