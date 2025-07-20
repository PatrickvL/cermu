#include "system.h"

#ifdef _MSC_VER
#pragma message("Compiling with MSVC")
#endif
#ifdef _M_X64
#pragma message("Targeting x64")
#endif
#ifdef _M_IX86
#pragma message("Targeting x86")
#endif
#ifdef __GNUC__
#pragma message("Compiling with GCC")
#endif
#ifdef __clang__
#pragma message("Compiling with Clang")
#endif
#ifdef __x86_64__
#pragma message("Detected x86_64")
#endif
#ifdef __i386__
#pragma message("Detected i386")
#endif

uint8_t system_chip_register(system_8bit_t* system, void* chip, chip_descriptor_t* desc, uint16_t base, unsigned int size) {
    if (system->chip_count >= 16) return 0xFF;
    uint8_t id = system->chip_count++;
    chip_entry_t* entry = &system->chips[id];
    entry->desc = desc;
    entry->chip = chip;
    entry->chip_id = id;
    entry->base_address = base;
    entry->size = size;
    entry->rwcb_context = chip; // Default to chip itself. Can get replaced by a callback.  
    // Use chip-specific callback to set rwcb_context if available
    if (desc->get_rwcb_context) {
        void* memory = desc->get_rwcb_context(chip);
        if (memory) {
            // Store callback context (adjusted for base address)
            entry->rwcb_context = (uint8_t*)memory - base;
        }
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
