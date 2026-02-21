#include "ram.h"
#include <string.h>
#include <stdlib.h>

ram_t* ram_create() {
    ram_t* ram = (ram_t*)calloc(1, sizeof(ram_t));
    if (!ram) return NULL;
    ram->desc = &ram_descriptor;
    // Note: memory pointer will be set later to point into unified buffer
    ram->memory = NULL;
    ram->owns_memory = false;  // Memory will be owned by unified buffer
    return ram;
}

void ram_destroy(ram_t* ram) {
    if (!ram) return;
    // Only free memory if we own it (not pointing into unified buffer)
    if (ram->owns_memory && ram->memory) {
        free(ram->memory);
    }
    free(ram);
}

chip_descriptor_t ram_descriptor = {
    .description = "System RAM",
    .create = [](chip_descriptor_t*) -> void* { return ram_create(); },
    .destroy = [](void* chip) { ram_destroy(static_cast<ram_t*>(chip)); },
    .bus_attach = NULL,
    .bank_change = NULL
};

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "ram_gui.h"
#endif
