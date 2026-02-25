#include "mos6561.h"
#include <string.h>

// MOS6561 chip configuration — PAL variant
// The MOS 6561 is the PAL version of the VIC-I chip used in PAL VIC-20s.
// PAL crystal: 4.433619 MHz / 4 = 1,108,405 Hz system clock
// 63 cycles per line, 312 total lines per frame → ~50 Hz refresh
// NOTE: VICE uses 71 cycles/line for PAL; 63 here matches the existing
// working VIC rendering code and should be reviewed separately.
static const vic_chip_config_t vic_config_pal = {
    .cycles_per_line = VIC_PAL_CYCLES_PER_LINE,
    .total_lines = VIC_PAL_TOTAL_LINES,
    .clock_frequency = 1108405,
    .chip_name = "MOS6561 PAL",
    .is_pal = true
};

void mos6561_s::init() {
    is_pal = true;
    info_ = ChipInfo{"MOS6561", "MOS Technology"};
    clock_frequency = vic_config_pal.clock_frequency;
    config = &vic_config_pal;

    // Initialize registers
    memset(registers, 0, sizeof(registers));
    memset(color_ram, 0, sizeof(color_ram));

    // Default timing for PAL
    cycles_per_line = VIC_PAL_CYCLES_PER_LINE;
    total_lines = VIC_PAL_TOTAL_LINES;

    // Reset video generation state
    reset();

    // Initialise audio with PAL clock and default sample rate
    audio_reset(vic_config_pal.clock_frequency, 22050);
}
