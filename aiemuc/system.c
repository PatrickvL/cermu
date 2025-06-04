#include "system.h"
//#include "device.h"

uint8_t system_device_register(system_8bit_t* system, void* device, device_descriptor_t* desc, uint16_t base, unsigned int size) {
    if (system->device_count >= 16) return 0xFF;
    uint8_t id = system->device_count++;
    device_entry_t* entry = &system->devices[id];
    entry->desc = desc;
    entry->device = device;
    entry->rwcb_context = device; // By default, rwcb_context is the device itself
    entry->base_address = base;
    entry->size = size;
    entry->device_id = id;
    return id;
}

void system_devices_destroy(system_8bit_t* system) {
    for (int i = 0; i < system->device_count; i++) {
        device_entry_t* entry = &system->devices[i];
        if (entry->desc && entry->desc->destroy) {
            entry->desc->destroy(entry->device);
        }
    }
}
