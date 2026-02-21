#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Legacy C-compatible wrappers for FAM65XX CPU family GUI rendering.
// These delegate to the ChipBase virtual methods on the CPU instance.
void fam65xx_render_debug_content(void *chip);
void fam65xx_render_settings_content(void *chip);
void fam65xx_render_layout_content(void *chip);

// Bus state update function (retained for backward compatibility — currently a no-op;
// set gui_bus_state on the CPU object directly instead)
void fam65xx_update_bus_state(void *chip, uint64_t bus_state);

#ifdef __cplusplus
}
#endif
