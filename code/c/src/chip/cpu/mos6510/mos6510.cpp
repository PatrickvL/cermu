#include "mos6510.h"
#include "../fam65xx_cpp/fam65xx.hpp"
#include "../fam65xx_cpp/cpu_config.hpp"
#include "../../../core/chip.h"

// C wrapper around the template-based CPU - zero overhead
struct mos6510_chip {
    fam65xx_cpp::fam65xx<config_6510> cpu;
};

mos6510_chip_t* mos6510_create(void) {
    return new mos6510_chip_t;
}

void mos6510_destroy(mos6510_chip_t* cpu) {
    delete cpu;
}

void mos6510_init(mos6510_chip_t* cpu,
                  void (*io_callback)(uint16_t addr, uint8_t data, bool write)) {
    (void)io_callback; // TODO: Implement IO callback integration
    cpu->cpu.init();
}

bus_state_t mos6510_tick(mos6510_chip_t* cpu, bus_state_t bus_state) {
    return cpu->cpu.cycle_tick(bus_state);
}

uint16_t mos6510_get_pc(mos6510_chip_t* cpu) {
    return cpu->cpu.get_pc();
}

uint8_t mos6510_get_a(mos6510_chip_t* cpu) {
    return cpu->cpu.get_a();
}

uint8_t mos6510_get_x(mos6510_chip_t* cpu) {
    return cpu->cpu.get_x();
}

uint8_t mos6510_get_y(mos6510_chip_t* cpu) {
    return cpu->cpu.get_y();
}

uint8_t mos6510_get_s(mos6510_chip_t* cpu) {
    return cpu->cpu.get_s();
}

uint8_t mos6510_get_p(mos6510_chip_t* cpu) {
    return cpu->cpu.get_p();
}

// Chip interface functions for system integration
static void* mos6510_chip_create(chip_descriptor_t* desc) {
    (void)desc; // Unused parameter
    return mos6510_create();
}

static void mos6510_chip_destroy(void* chip) {
    mos6510_destroy((mos6510_chip_t*)chip);
}

bus_state_t mos6510_tick_chip(void* chip, bus_state_t bus_state) {
    return mos6510_tick((mos6510_chip_t*)chip, bus_state);
}

// Chip descriptor for system registration
chip_descriptor_t mos6510_descriptor = {
    .description = "MOS6510 CPU",
    .create = mos6510_chip_create,
    .destroy = mos6510_chip_destroy,
    .bus_attach = NULL, // No special bus attachment needed
    .bank_change = NULL // No banking change needed
};