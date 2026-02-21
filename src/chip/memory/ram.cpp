#include "ram.h"
#include <string.h>
#include <stdlib.h>

void* ram_system_create(chip_descriptor_t* desc) {
    ram_t* ram = (ram_t*)calloc(1, sizeof(ram_t));
    if (!ram) return NULL;
    ram->desc = desc;
    // Note: memory pointer will be set later to point into unified buffer
    ram->memory = NULL;
    ram->owns_memory = false;  // Memory will be owned by unified buffer
    return ram;
}

void ram_system_destroy(void* context) {
    ram_t* ram = (ram_t*)context;
    if (!ram) return;
    // Only free memory if we own it (not pointing into unified buffer)
    if (ram->owns_memory && ram->memory) {
        free(ram->memory);
    }
    free(ram);
}

chip_descriptor_t ram_descriptor = {
    .description = "System RAM",
    .create = ram_system_create,
    .destroy = ram_system_destroy,
    .bus_attach = NULL,
    .bank_change = NULL
};

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "ram_gui.h"
#endif
