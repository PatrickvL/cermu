#pragma once
#include "core/config/rom_config.h"

enum vic20_video_standard_t {
    VIC20_PAL,
    VIC20_NTSC
};

struct vic20_config_t {
    vic20_video_standard_t video_standard;
    const rom_config_t* rom_config;
};

const vic20_config_t* vic20_config_get_default(void);