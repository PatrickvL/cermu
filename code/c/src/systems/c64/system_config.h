#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

typedef enum {
    VIC_PAL,   // PAL timing standard
    VIC_NTSC   // NTSC timing standard
} vic_standard_t;

/**
 * System configuration options.
 * Pass this to c64_system_create() to select VIC-II standard.
 */
typedef struct {
    vic_standard_t vic_standard;
} system_config_t;

#endif // SYSTEM_CONFIG_H