#include "c64_bus.h"
#include "c64.h"
#include <stdlib.h>
#include <string.h>

// Inline function to calculate condensed bank index from address.
// Bank number is derived from the upper 4 bits of the address, whereby the lower 4 
// IO range (bank 13), returns 16 + the page number (from the 2nd address nybble).
static inline int c64_bus_address_to_bankidx(uint16_t address) {
    int page = (address >> 8) & 0x0F;
    int bank = address >> 12; // bank 0 to 15 (13 is unused)
    int bank13_delta = 3 + page; // 3 to 18, when added to 13 gives 16 to 31
    int is_bank13_mask = - (int)(bank == 13); // 0x00000000 or 0xFFFFFFFF
    return bank + (is_bank13_mask & bank13_delta); // 0 to 31: 0 to 15 bank numbers (13 unused), 16 and up for bank 13 pages
}

uint8_t c64_bus_memory_read(c64_bus_t* c64_bus, uint16_t address) {
    int bankidx = c64_bus_address_to_bankidx(address);
    uint8_t devid = DEVID_READ_DECODE(c64_bus->devid_per_bankidx[bankidx]);
    access_callback_t* cb = &c64_bus->access_callback_per_devid[devid];
    return cb->read_func(cb->context, address);
}

void c64_bus_memory_write(c64_bus_t* c64_bus, uint16_t address, uint8_t value) {
    int bankidx = c64_bus_address_to_bankidx(address);
    uint8_t devid = DEVID_WRITE_DECODE(c64_bus->devid_per_bankidx[bankidx]);
    access_callback_t* cb = &c64_bus->access_callback_per_devid[devid];
    cb->write_func(cb->context, address, value);
    // TODO : Move below signalling of VIC-II bank change to somewhere else with less impact on performance
    if (address == 0xDD00) {
        c64_t* c64 = c64_bus->c64;
        uint8_t bank = 3 - (value & 0x3);
        if (c64->sid->desc->bank_change) {
            c64->sid->desc->bank_change(c64->sid, bank);
        }
    }
}

void c64_bus_system_destroy(void* device) {
    free(device);
}

void* c64_bus_system_create(device_descriptor_t* desc) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = desc;
    // Initialize bus state
    c64_bus->address = 0;
    c64_bus->data = 0;
    c64_bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
    return c64_bus;
}

void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64) {
    c64_bus->c64 = c64;  // Store as opaque pointer
}

device_descriptor_t c64_bus_descriptor = {
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .bus_attach = NULL,
    .read = NULL,
    .write = NULL,
    .bank_change = NULL
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    // Update the device ID mapping for the current mode
    memcpy(c64_bus->devid_per_bankidx, c64_bus->devid_per_bankidx_per_mode[mode], sizeof(c64_bus->devid_per_bankidx));
}

uint8_t c64_bus_read_cycle(c64_bus_t *c64_bus, uint16_t addr) {
    c64_bus->address = addr; // Perhaps this is no longer needed
    uint8_t data = c64_bus_memory_read(c64_bus, addr);
    c64_bus->data = data; // Perhaps this is no longer needed
    c64_non_cpu_cycle(c64_bus->c64);
    return data;
}    

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    c64_bus->address = addr; // Perhaps this is no longer needed
    c64_bus->data = value; // Perhaps this is no longer needed
    c64_bus_memory_write(c64_bus, addr, value);
    c64_non_cpu_cycle(c64_bus->c64);
}

// PLA integration functions
#include "../../chip/logic/pla.h"

void c64_bus_populate_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla,
                                 uint8_t ram_id, uint8_t basic_id, uint8_t kernal_id, 
                                 uint8_t charrom_id, uint8_t io_id, uint8_t cartridge_roml_id, 
                                 uint8_t cartridge_romh_id, uint8_t colorram_id) {
    // Clear current mapping
    for (int i = 0; i < 32; i++) {
        bus->devid_per_bankidx[i] = DEVID_UNMAPPED;
    }      // Map memory regions based on PLA outputs
    for (uint32_t addr = 0; addr < 0x10000; addr += 0x100) {
        int bank_idx = c64_bus_address_to_bankidx(addr);
        uint8_t device_id = DEVID_UNMAPPED;
        
        // Set address in PLA
        pla_906114_01_set_address_high((pla_906114_01_t*)pla, (addr >> 8) & 0x0F);
        
        // Determine device based on PLA outputs
        if (!pla->outputs.n_casram) {
            // RAM is selected
            if (addr < 0x0002) {
                device_id = DEVID_ZEROPAGE;  // Special handling for CPU I/O ports
            } else {
                device_id = ram_id;
            }
        } else if (!pla->outputs.n_basic) {
            device_id = basic_id;
        } else if (!pla->outputs.n_kernal) {
            device_id = kernal_id;
        } else if (!pla->outputs.n_charrom) {
            device_id = charrom_id;
        } else if (!pla->outputs.n_io) {
            // I/O region - determine specific device
            if (addr >= 0xD000 && addr < 0xD400) {
                device_id = DEVID_VIC;
            } else if (addr >= 0xD400 && addr < 0xD800) {
                device_id = DEVID_SID;
            } else if (addr >= 0xD800 && addr < 0xDC00) {
                device_id = colorram_id;
            } else if (addr >= 0xDC00 && addr < 0xE000) {
                device_id = DEVID_CIA;
            } else {
                device_id = io_id;
            }
        } else if (!pla->outputs.n_roml) {
            device_id = cartridge_roml_id;
        } else if (!pla->outputs.n_romh) {
            device_id = cartridge_romh_id;
        }
        
        bus->devid_per_bankidx[bank_idx] = device_id;
    }
}

void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla,
                                   uint8_t ram_id, uint8_t basic_id, uint8_t kernal_id,
                                   uint8_t charrom_id, uint8_t io_id, uint8_t cartridge_roml_id,
                                   uint8_t cartridge_romh_id, uint8_t colorram_id) {
    pla_906114_01_t* pla_impl = (pla_906114_01_t*)pla;
    
    // Generate all 32 memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        // Set PLA inputs based on mode
        pla_impl->inputs.n_loram = (mode & 0x01) == 0;    // LORAM (inverted)
        pla_impl->inputs.n_hiram = (mode & 0x02) == 0;    // HIRAM (inverted)
        pla_impl->inputs.n_charen = (mode & 0x04) == 0;   // CHAREN (inverted)
        pla_impl->inputs.n_exrom = (mode & 0x08) == 0;    // EXROM (inverted)
        pla_impl->inputs.n_game = (mode & 0x10) == 0;     // GAME (inverted)
        
        // Set other inputs for normal CPU operation
        pla_impl->inputs.aec = true;     // CPU has bus control
        pla_impl->inputs.ba = true;      // Bus available
        pla_impl->inputs.r_w = true;     // Read mode
        pla_impl->inputs.n_cas = true;   // No CAS
        pla_impl->inputs.n_va14 = true;  // VA14 high
        pla_impl->inputs.va13 = false;   // VA13 low
        pla_impl->inputs.va12 = false;   // VA12 low
        pla_impl->inputs.n_ce = false;   // Chip enabled
          // Populate mapping for this mode
        c64_bus_populate_pla_mapping(bus, pla, ram_id, basic_id, kernal_id, 
                                   charrom_id, io_id, cartridge_roml_id, 
                                   cartridge_romh_id, colorram_id);
        
        // Copy the mapping to the mode-specific array
        memcpy(bus->devid_per_bankidx_per_mode[mode], bus->devid_per_bankidx, sizeof(bus->devid_per_bankidx));
    }
}
