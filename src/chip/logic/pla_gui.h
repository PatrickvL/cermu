#pragma once

#include <stdint.h>
#include <stdbool.h>

// Function declarations for PLA debug window (now integrated with chip system)
void pla_render_debug_window(void* chip, bool* show_window);
void pla_render_settings_window(void* chip, bool* show_window);


