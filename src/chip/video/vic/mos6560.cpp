#include "mos6560.h"
#include <string.h>

// VIC-6560 chip configuration — NTSC variant
// The MOS 6560 is the NTSC version of the VIC-I chip used in NTSC VIC-20s.
// NTSC crystal: 14.31818 MHz / 14 = 1,022,727 Hz system clock
// 65 cycles per line, 262 total lines per frame → ~60 Hz refresh
static const vic_chip_config_t vic_config_ntsc = {
    .cycles_per_line = VIC_NTSC_CYCLES_PER_LINE,
    .total_lines = VIC_NTSC_TOTAL_LINES,
    .clock_frequency = 1022727,
    .chip_name = "MOS6560 NTSC",
    .is_pal = false
};

void mos6560_s::init() {
    is_pal = false;
    clock_frequency = vic_config_ntsc.clock_frequency;
    config = &vic_config_ntsc;

    // Initialize registers
    memset(registers, 0, sizeof(registers));
    memset(color_ram, 0, sizeof(color_ram));

    // Default timing for NTSC
    cycles_per_line = VIC_NTSC_CYCLES_PER_LINE;
    total_lines = VIC_NTSC_TOTAL_LINES;

    // Reset video generation state
    reset();

    // Initialise audio with NTSC clock and default sample rate
    audio_reset(vic_config_ntsc.clock_frequency, 22050);
}