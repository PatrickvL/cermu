#include "system.h"
//#include "device.h"

uint8_t register_device(system_8bit_t* system, void* device, device_descriptor_t* desc, uint16_t base, uint16_t size) {
    if (system->device_count >= 16) return 0xFF;
    uint8_t id = system->device_count;
    device_entry_t* entry = &system->devices[system->device_count++];
    entry->device = device;
    entry->desc = desc;
    entry->base_address = base;
    entry->size = size;
    return id;
}

