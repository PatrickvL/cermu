#include "sid.h"
#include <string.h>

void* sid_system_create(void* bus) {
    sid_t* sid = (sid_t*)calloc(1, sizeof(sid_t));
    if (!sid) return NULL;
    sid->desc = &sid_descriptor;
    sid->bus = (bus_interface_t*)bus;
    return sid;
}

void sid_system_destroy(void* context) {
    free(context);
}

uint8_t sid_registers_read(void* context, uint16_t address) {
    sid_t* sid = (sid_t*)context;
    uint8_t reg = address & 0x1F;
    if (reg >= 0x19 && reg <= 0x1C) {
        switch (reg) {
            case 0x19: return sid->pot_x;
            case 0x1A: return sid->pot_y;
            case 0x1B: return sid->osc3;
            case 0x1C: return sid->env3;
        }
    }
    return 0;
}

void sid_registers_write(void* context, uint16_t address, uint8_t value) {
    sid_t* sid = (sid_t*)context;
    uint8_t reg = address & 0x1F;
    if (reg <= 0x18) sid->registers[reg] = value;
}

static device_descriptor_t sid_descriptor = {
    .create = sid_system_create,
    .destroy = sid_system_destroy,
    .read = sid_registers_read,
    .write = sid_registers_write,
    .bank_change = NULL
};

// old

// Forward declaration of device descriptor
static const device_t sid_device_descriptor;

// Direct implementation functions for device lifecycle
static void sid_init(struct device_s* dev) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    // Initialize SID state
    memset(sid_dev, 0, sizeof(*sid_dev));
    // Set up device callbacks from descriptor pointer
    sid_dev->device = &sid_device_descriptor;
}

static void sid_cycle(struct device_s* dev) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    // Envelope generators always run (hardware accurate)
    for (int voice = 0; voice < 3; voice++) {
        sid_dev->envelope_counter[voice]++;
        if (sid_dev->envelope_counter[voice] >= 0x8000) {
            sid_dev->envelope_counter[voice] = 0;
            // Simplified envelope state machine
            sid_dev->envelope_state[voice] = (sid_dev->envelope_state[voice] + 1) & 0xFF;
        }
    }
}

// Optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t sid_r8(struct device_s* dev, uint16_t address) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    uint8_t reg = address & 0x1F;
    return sid_dev->registers[reg];
}

void sid_w8(struct device_s* dev, uint16_t address, uint8_t data) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    uint8_t reg = address & 0x1F;
    sid_dev->registers[reg] = data;
}

// Static device descriptor for SID
static const device_t sid_device_descriptor = {
    .r8 = sid_r8,
    .w8 = sid_w8,
    .init = sid_init,
    .cycle = sid_cycle,
    .cleanup = NULL
};