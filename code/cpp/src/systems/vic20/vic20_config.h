#pragma once

#include <stdbool.h>
#include "../../core/config/rom_config.h"

typedef enum {
    VIC20_PAL,
    VIC20_NTSC
} vic20_video_standard_t;

typedef struct {
    vic20_video_standard_t video_standard;
    const rom_config_t* rom_config;
} vic20_config_t;

const vic20_config_t* vic20_config_get_default(void);