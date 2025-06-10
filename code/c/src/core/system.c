#include "system.h"

uint8_t system_chip_register(system_8bit_t* system, void* chip, chip_descriptor_t* desc, uint16_t base, unsigned int size) {
    if (system->chip_count >= 16) return 0xFF;
    uint8_t id = system->chip_count++;
    chip_entry_t* entry = &system->chips[id];
    entry->desc = desc;
    entry->chip = chip;
    entry->base_address = base;
    entry->size = size;
    entry->chip_id = id;
    
    // Use chip-specific callback to set rwcb_context if available
     entry->rwcb_context = desc->get_rwcb_context(chip);
    if (desc->get_rwcb_context) {
        // If the callback returns a buffer, adjust it's base address
        (char*)entry->rwcb_context -= base;
    } else {
        // If the callback returns NULL, use the chip itself as context
        entry->rwcb_context = chip;
    }
    
    return id;
}

void system_chips_destroy(system_8bit_t* system) {
    for (int i = 0; i < system->chip_count; i++) {
        chip_entry_t* entry = &system->chips[i];
        if (entry->desc && entry->desc->destroy) {
            entry->desc->destroy(entry->chip);
        }
    }
}
