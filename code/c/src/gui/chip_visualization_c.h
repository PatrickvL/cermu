/*
 * chip_visualization_c.h - C wrapper for chip visualization system
 * 
 * Provides C-compatible interface to the C++ ChipVisualization system
 */

#ifndef CHIP_VISUALIZATION_C_H
#define CHIP_VISUALIZATION_C_H

#include "../core/chip_layout.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// C-compatible function to render chip visualization with collapsing header
void render_chip_visualization_c(const struct ChipLayout* layout, 
                                const struct PinSignalState* pin_states, 
                                const char* header_title);

// Helper function to get total pins from ChipLayout
int get_chip_layout_total_pins(const struct ChipLayout* layout);

#ifdef __cplusplus
}
#endif

#endif // CHIP_VISUALIZATION_C_H