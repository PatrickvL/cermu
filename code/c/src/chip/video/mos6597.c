#include "mos6597.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * NTSC VIC-II (MOS6597) lifecycle and bus attach wrappers
 */
void* mos6597_system_create(chip_descriptor_t* desc) {
    return vicii_common_system_create(desc, mos6597_bank_change);
}

void mos6597_system_destroy(void* chip) {
    vicii_common_system_destroy(chip);
}

void mos6597_bus_attach(void* chip, void* bus) {
    vicii_common_bus_attach(chip, bus);
}

/*
 * Register I/O wrappers
 */
uint8_t mos6597_registers_read(void* chip, uint16_t address) {
    return vicii_common_registers_read(chip, address);
}

void mos6597_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_registers_write(chip, address, value);
}

/*
 * Bank change callback wrapper
 */
void mos6597_bank_change(void* chip, uint8_t bank) {
    vicii_common_bank_change(chip, bank);
}

/*
 * Descriptor for NTSC VIC-II
 */
chip_descriptor_t mos6597_descriptor = {
    .create     = mos6597_system_create,
    .destroy    = mos6597_system_destroy,
    .bus_attach = mos6597_bus_attach,
    .read       = mos6597_registers_read,
    .write      = mos6597_registers_write,
    .bank_change= mos6597_bank_change
};

/*
 * NTSC cycle logic: delegate to common with NTSC timing
 */
void mos6597_cycle(mos6597_t* vicii) {
    vicii_common_cycle((vicii_common_t*)vicii, MOS6597_CYCLES_PER_LINE, MOS6597_TOTAL_LINES);
}