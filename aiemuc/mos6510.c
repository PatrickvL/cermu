#include "mos6510.h"

typedef void(*)() PFNDUOP; // TODO : replace

void* mos6510_system_create(void* bus) {
    mos6510_t* cpu = (mos6510_t*)calloc(1, sizeof(mos6510_t));
    if (!cpu) return NULL;
    cpu->desc = &mos6510_descriptor;
    cpu->bus = (bus_interface_t*)bus;
    return cpu;
}

void mos6510_system_destroy(void* context) {
    free(context);
}

uint8_t mos6510_registers_read(void* context, uint16_t address) {
    return 0; // CPU has no readable registers
}

void mos6510_registers_write(void* context, uint16_t address, uint8_t value) {
    // CPU has no writable registers
}

void mos6510_opcode_dispatch(void* context, uint8_t opcode) {
    mos6510_t* cpu = (mos6510_t*)context;
    PFNDUOP handler = mos6510_opcode_map[opcode];
    handler(cpu);
}

static device_descriptor_t mos6510_descriptor = {
    .create = mos6510_system_create,
    .destroy = mos6510_system_destroy,
    .read = mos6510_registers_read,
    .write = mos6510_registers_write,
    .bank_change = NULL
};
