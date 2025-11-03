#ifndef FAM65XX_GUI_H
#define FAM65XX_GUI_H

#include <stdint.h>
#include <stdbool.h>

// Function declarations for FAM65XX CPU family debug windows
void fam65xx_render_debug_window(void* chip, bool* show_window);
void fam65xx_render_settings_window(void* chip, bool* show_window);

#endif // FAM65XX_GUI_H