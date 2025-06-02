#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "aiemuc.h"
#include "bus.h"
#include "device.h"
#include "system.h"
// TODO : Move to mos6510.h

#define PFNDUOP void(*)()

typedef struct {
    device_descriptor_t* desc;
    bus_interface_t* bus;
    void* handler_map[256];
} mos6510_t;

// TODO : Move to cia.h

typedef struct {
    device_descriptor_t* desc;
    uint8_t pra, prb, ddra, ddrb;
    uint16_t timer_a, timer_b;
    uint8_t tod_10ths, tod_sec, tod_min, tod_hr;
    uint8_t sdr, icr, cra, crb;
    bool tod_latched;
    uint8_t tod_latch[4];
    bus_interface_t* bus;
} cia_t;

// TODO : Move to vic.h

typedef void (*vic_bank_change_func_t)(void* context, uint8_t bank);

typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[47];
    uint8_t raster_line;
    uint8_t collision_sprite, collision_bg;
    uint8_t bank;
    bus_interface_t* bus;
    vic_bank_change_func_t bank_change;
} vic_ii_t;

// TODO : Move to sid.h

typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[29];
    uint8_t pot_x, pot_y;
    uint8_t osc3, env3;
    bus_interface_t* bus;
} sid_t;

// TODO : Move to custom.h or delete if not needed

typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[256];
    bus_interface_t* bus;
} custom_t;

// TODO : Move to c64.h

#define MAKE_MASK(read_id, write_id) (((read_id) << 4) | (write_id))

// ID macros
#define READ_ID_RAM        0
#define READ_ID_ZERO_PAGE  1
#define READ_ID_CARTRIDGE  2
#define READ_ID_BASIC      3
#define READ_ID_VIC_II     4
#define READ_ID_SID        5
#define READ_ID_CIA1       6
#define READ_ID_CIA2       7
#define READ_ID_CUSTOM     8
#define READ_ID_KERNAL     9
#define READ_ID_MOS6510    10
#define READ_ID_BUS        11

#define WRITE_ID_RAM       0
#define WRITE_ID_ZERO_PAGE 1
#define WRITE_ID_VIC_II    2
#define WRITE_ID_SID       3
#define WRITE_ID_CIA1      4
#define WRITE_ID_CIA2      5
#define WRITE_ID_CUSTOM    6
#define WRITE_ID_MOS6510   7
#define WRITE_ID_BUS       8

// Device structs
typedef struct c64_state c64_state_t;

void c64_dispatch(c64_state_t* c64, uint16_t PC);

typedef struct {
    device_descriptor_t* desc;
    bus_interface_t interface;
    c64_state_t* c64;
} c64_bus_t;

// C64 state
struct c64_state {
    system_8bit_t system;
    uint8_t port[2];
    mos6510_t* mos6510;
    cia_t* cia1;
    cia_t* cia2;
    vic_ii_t* vic_ii;
    sid_t* sid;
    custom_t* custom;
    c64_bus_t* bus;
};

// TODO : Remove single global
c64_state_t* c64 = NULL;

// MOS 6510 functions
void mos6510_system_init(void* context, void* bus) {
    mos6510_t* cpu = (mos6510_t*)context;
    cpu->desc = &mos6510_descriptor;
    cpu->bus = (bus_interface_t*)bus;
    extern uint8_t handler_deltas[256];
    static void* base_address;
    for (int i = 0; i < 256; i++) {
        cpu->handler_map[i] = (void*)(base_address + handler_deltas[i]);
    }
}

uint8_t mos6510_registers_read(void* context, uint16_t address) {
    return 0; // CPU has no readable registers
}

void mos6510_registers_write(void* context, uint16_t address, uint8_t value) {
    // CPU has no writable registers
}

void mos6510_opcode_dispatch(void* context, uint8_t opcode) {
    mos6510_t* cpu = (mos6510_t*)context;
    PFNDUOP handler = cpu->handler_map[opcode];
    handler(cpu_state_ptr);
}

static device_descriptor_t mos6510_descriptor = {
    .init = mos6510_system_init,
    .read = mos6510_registers_read,
    .write = mos6510_registers_write,
    .bank_change = NULL
};

// CIA functions
void cia_system_init(void* context, void* bus) {
    cia_t* cia = (cia_t*)context;
    memset(cia, 0, sizeof(cia_t));
    cia->desc = &cia_descriptor;
    cia->bus = (bus_interface_t*)bus;
}

uint8_t cia_registers_read(void* context, uint16_t address) {
    cia_t* cia = (cia_t*)context;
    uint8_t reg = address & 0xF;
    switch (reg) {
        case 0x0: return cia->pra & cia->ddra;
        case 0x1: return cia->prb & cia->ddrb;
        case 0x2: return cia->ddra;
        case 0x3: return cia->ddrb;
        case 0x4: return cia->timer_a & 0xFF;
        case 0x5: return cia->timer_a >> 8;
        case 0x6: return cia->timer_b & 0xFF;
        case 0x7: return cia->timer_b >> 8;
        case 0x8:
            if (!cia->tod_latched) {
                cia->tod_latch[0] = cia->tod_10ths;
                cia->tod_latch[1] = cia->tod_sec;
                cia->tod_latch[2] = cia->tod_min;
                cia->tod_latch[3] = cia->tod_hr;
                cia->tod_latched = true;
            }
            return cia->tod_latch[0];
        case 0x9: return cia->tod_latched ? cia->tod_latch[1] : cia->tod_sec;
        case 0xA: return cia->tod_latched ? cia->tod_latch[2] : cia->tod_min;
        case 0xB: cia->tod_latched = false; return cia->tod_latched ? cia->tod_latch[3] : cia->tod_hr;
        case 0xC: return 0;
        case 0xD: {
            uint8_t status = cia->icr;
            cia->icr = 0;
            return status;
        }
        case 0xE: return cia->cra;
        case 0xF: return cia->crb;
    }
    return 0;
}

void cia_registers_write(void* context, uint16_t address, uint8_t value) {
    cia_t* cia = (cia_t*)context;
    uint8_t reg = address & 0xF;
    switch (reg) {
        case 0x0:
            cia->pra = value;
            if (address >= 0xDD00 && cia->bus->write) {
                cia->bus->write(cia->bus, address, value);
            }
            break;
        case 0x1: cia->prb = value; break;
        case 0x2: cia->ddra = value; break;
        case 0x3: cia->ddrb = value; break;
        case 0x4: cia->timer_a = (cia->timer_a & 0xFF00) | value; break;
        case 0x5: cia->timer_a = (cia->timer_a & 0xFF) | (value << 8); break;
        case 0x6: cia->timer_b = (cia->timer_b & 0xFF00) | value; break;
        case 0x7: cia->timer_b = (cia->timer_b & 0xFF) | (value << 8); break;
        case 0x8: cia->tod_10ths = value & 0xF; cia->tod_latched = false; break;
        case 0x9: cia->tod_sec = value & 0x7F; break;
        case 0xA: cia->tod_min = value & 0x7F; break;
        case 0xB: cia->tod_hr = value & 0x1F; break;
        case 0xC: cia->sdr = value; break;
        case 0xD: cia->icr = value; break;
        case 0xE: cia->cra = value; break;
        case 0xF: cia->crb = value; break;
    }
}

static device_descriptor_t cia_descriptor = {
    .init = cia_system_init,
    .read = cia_registers_read,
    .write = cia_registers_write,
    .bank_change = NULL
};

// VIC-II functions
void vicii_system_init(void* context, void* bus) {
    vic_ii_t* vic = (vic_ii_t*)context;
    memset(vic, 0, sizeof(vic_ii_t));
    vic->desc = &vic_ii_descriptor;
    vic->bus = (bus_interface_t*)bus;
    vic->bank_change = vicii_bank_change;
}

uint8_t vicii_registers_read(void* context, uint16_t address) {
    vic_ii_t* vic = (vic_ii_t*)context;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) {
        if (reg == 0x12) return vic->raster_line;
        if (reg == 0x1E) {
            uint8_t val = vic->collision_sprite;
            vic->collision_sprite = 0;
            return val;
        }
        if (reg == 0x1F) {
            uint8_t val = vic->collision_bg;
            vic->collision_bg = 0;
            return val;
        }
        return vic->registers[reg];
    }
    return 0;
}

void vicii_registers_write(void* context, uint16_t address, uint8_t value) {
    vic_ii_t* vic = (vic_ii_t*)context;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) vic->registers[reg] = value;
}

void vicii_bank_change(void* context, uint8_t bank) {
    vic_ii_t* vic = (vic_ii_t*)context;
    vic->bank = bank;
}

static device_descriptor_t vic_ii_descriptor = {
    .init = vicii_system_init,
    .read = vicii_registers_read,
    .write = vicii_registers_write,
    .bank_change = vicii_bank_change
};

// SID functions
void sid_system_init(void* context, void* bus) {
    sid_t* sid = (sid_t*)context;
    memset(sid, 0, sizeof(sid_t));
    sid->desc = &sid_descriptor;
    sid->bus = (bus_interface_t*)bus;
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
    .init = sid_system_init,
    .read = sid_registers_read,
    .write = sid_registers_write,
    .bank_change = NULL
};

// Custom functions
void custom_system_init(void* context, void* bus) {
    custom_t* custom = (custom_t*)context;
    memset(custom, 0, sizeof(custom_t));
    custom->desc = &custom_descriptor;
    custom->bus = (bus_interface_t*)bus;
}

uint8_t custom_registers_read(void* context, uint16_t address) {
    custom_t* custom = (custom_t*)context;
    return custom->registers[address & 0xFF];
}

void custom_registers_write(void* context, uint16_t address, uint8_t value) {
    custom_t* custom = (custom_t*)context;
    custom->registers[address & 0xFF] = value;
}

static device_descriptor_t custom_descriptor = {
    .init = custom_system_init,
    .read = custom_registers_read,
    .write = custom_registers_write,
    .bank_change = NULL
};

// Memory callbacks

uint8_t memory_read(void* context, uint16_t address) {
    return ((uint8_t*)context)[address];
}

void memory_write(void* context, uint16_t address, uint8_t value) {
    ((uint8_t*)context)[address] = value;
}

uint8_t c64_zero_page_read(void* context, uint16_t address) {
    c64_state_t* c64 = (c64_state_t*)context;
    if (!(address & 0xFE)) return c64->port[address];
    return c64->system.system_memory[address];
}    

void c64_zero_page_write(void* context, uint16_t address, uint8_t value) {
    c64_state_t* c64 = (c64_state_t*)context;
    if (!(address & 0xFE)) {
        c64->port[address] = value;
        if (address == 0x0001) c64_bankmode_switch(c64, value);
    } else {
        c64->system.system_memory[address] = value;
    }
}

// System functions
bool device_register(system_8bit_t* system, void* device, device_descriptor_t* desc, uint16_t base, uint16_t size, uint8_t device_id) {
    if (system->device_count >= 16) return false;
    device_entry_t* entry = &system->devices[system->device_count++];
    entry->device = device;
    entry->desc = desc;
    entry->base_address = base;
    entry->size = size;
    entry->device_id = device_id;
    return true;
}

void init_memory(system_8bit_t* system) {
    extern uint8_t initial_ram[65536], basic_rom[8192], kernal_rom[8192], cartridge_rom[16384];
    memcpy(system->system_memory, initial_ram, 65536);
    memcpy(system->system_memory + 65536, basic_rom, 8192);
    memcpy(system->system_memory + 73728, kernal_rom, 8192);
    memcpy(system->system_memory + 81920, cartridge_rom, 16384);
}

void c64_callbacks_init(c64_state_t* c64) {
    c64->system.read_callbacks[READ_ID_RAM] = (read_callback_t){ memory_read, c64->system.system_memory };
    c64->system.read_callbacks[READ_ID_ZERO_PAGE] = (read_callback_t){ c64_zero_page_read, c64 };
    c64->system.read_callbacks[READ_ID_CARTRIDGE] = (read_callback_t){ memory_read, c64->system.system_memory + 81920 };
    c64->system.read_callbacks[READ_ID_BASIC] = (read_callback_t){ memory_read, c64->system.system_memory + 65536 };
    c64->system.read_callbacks[READ_ID_VIC_II] = (read_callback_t){ vicii_registers_read, c64->vic_ii };
    c64->system.read_callbacks[READ_ID_SID] = (read_callback_t){ sid_registers_read, c64->sid };
    c64->system.read_callbacks[READ_ID_CIA1] = (read_callback_t){ cia_registers_read, c64->cia1 };
    c64->system.read_callbacks[READ_ID_CIA2] = (read_callback_t){ cia_registers_read, c64->cia2 };
    c64->system.read_callbacks[READ_ID_CUSTOM] = (read_callback_t){ custom_registers_read, c64->custom };
    c64->system.read_callbacks[READ_ID_KERNAL] = (read_callback_t){ memory_read, c64->system.system_memory + 73728 };
    c64->system.read_callbacks[READ_ID_MOS6510] = (read_callback_t){ mos6510_registers_read, c64->mos6510 };
    c64->system.read_callbacks[READ_ID_BUS] = (read_callback_t){ c64_bus_memory_read, c64->bus };
    for (int i = 12; i < 16; i++) c64->system.read_callbacks[i] = (read_callback_t){ memory_read, c64->system.system_memory };

    c64->system.write_callbacks[WRITE_ID_RAM] = (write_callback_t){ memory_write, c64->system.system_memory };
    c64->system.write_callbacks[WRITE_ID_ZERO_PAGE] = (write_callback_t){ c64_zero_page_write, c64 };
    c64->system.write_callbacks[WRITE_ID_VIC_II] = (write_callback_t){ vicii_registers_write, c64->vic_ii };
    c64->system.write_callbacks[WRITE_ID_SID] = (write_callback_t){ sid_registers_write, c64->sid };
    c64->system.write_callbacks[WRITE_ID_CIA1] = (write_callback_t){ cia_registers_write, c64->cia1 };
    c64->system.write_callbacks[WRITE_ID_CIA2] = (write_callback_t){ cia_registers_write, c64->cia2 };
    c64->system.write_callbacks[WRITE_ID_CUSTOM] = (write_callback_t){ custom_registers_write, c64->custom };
    c64->system.write_callbacks[WRITE_ID_MOS6510] = (write_callback_t){ mos6510_registers_write, c64->mos6510 };
    c64->system.write_callbacks[WRITE_ID_BUS] = (write_callback_t){ c64_bus_memory_write, c64->bus };
    for (int i = 9; i < 16; i++) c64->system.write_callbacks[i] = (write_callback_t){ memory_write, c64->system.system_memory };
}

void generate_pla_maps(c64_state_t* c64) {
    system_8bit_t* system = &c64->system;
    for (int mode = 0; mode < 32; mode++) {
        bool loram = mode & 1, hiram = mode & 2, charen = !(mode & 4), game = mode & 8;
        for (int page = 0; page < 256; page++) {
            uint16_t addr = page << 8;
            uint8_t read_id = READ_ID_RAM;
            uint8_t write_id = WRITE_ID_RAM;
            if (page == 0) {
                read_id = READ_ID_ZERO_PAGE;
                write_id = WRITE_ID_ZERO_PAGE;
            } else if (hiram && addr >= 0xE000) {
                read_id = READ_ID_KERNAL;
            } else if (loram && addr >= 0xA000 && addr <= 0xBFFF) {
                read_id = READ_ID_BASIC;
            } else if (game && addr >= 0x8000 && addr <= 0xBFFF) {
                read_id = READ_ID_CARTRIDGE;
            } else if (charen && addr >= 0xD000 && addr <= 0xDFFF) {
                for (int i = 0; i < system->device_count; i++) {
                    device_entry_t* dev = &system->devices[i];
                    if (dev->size && addr >= dev->base_address && addr < dev->base_address + dev->size) {
                        read_id = dev->device_id;
                        write_id = dev->device_id;
                        break;
                    }
                }
            }
            system->chip_select_map[mode][page] = MAKE_MASK(read_id, write_id);
        }
    }
    system->handler_table = system->chip_select_map[0];
}

void c64_dispatch(c64_state_t* c64, uint16_t PC) {
    uint8_t opcode = c64->bus->interface.read(c64->bus, PC);
    mos6510_opcode_dispatch(c64->mos6510, opcode);
}

uint8_t c64_memory_read(c64_state_t* c64, uint16_t address) {
    uint8_t page = address >> 8;
    uint8_t read_id = (c64->system.handler_table[page] >> 4) & 0xF;
    read_callback_t* cb = &c64->system.read_callbacks[read_id];
    return cb->func(cb->context, address);
}

void c64_memory_write(c64_state_t* c64, uint16_t address, uint8_t value) {
    uint8_t page = address >> 8;
    uint8_t write_id = c64->system.handler_table[page] & 0xF;
    write_callback_t* cb = &c64->system.write_callbacks[write_id];
    cb->func(cb->context, address, value);
}

void c64_bankmode_switch(c64_state_t* c64, uint8_t mode) {
    c64->system.handler_table = c64->system.chip_select_map[mode];
}

// Create and initialize C64
c64_state_t* new_c64() {
    c64_state_t* c64 = (c64_state_t*)malloc(sizeof(c64_state_t));
    if (!c64) return NULL;
    c64->system.device_count = 0;
    c64->mos6510 = (mos6510_t*)calloc(1, sizeof(mos6510_t));
    c64->cia1 = (cia_t*)calloc(1, sizeof(cia_t));
    c64->cia2 = (cia_t*)calloc(1, sizeof(cia_t));
    c64->vic_ii = (vic_ii_t*)calloc(1, sizeof(vic_ii_t));
    c64->sid = (sid_t*)calloc(1, sizeof(sid_t));
    c64->custom = (custom_t*)calloc(1, sizeof(custom_t));
    c64->bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64->mos6510 || !c64->cia1 || !c64->cia2 || !c64->vic_ii || !c64->sid || !c64->custom || !c64->bus) {
        free(c64->mos6510);
        free(c64->cia1);
        free(c64->cia2);
        free(c64->vic_ii);
        free(c64->sid);
        free(c64->custom);
        free(c64->bus);
        free(c64);
        return NULL;
    }
    device_register(&c64->system, c64->bus, &c64_bus_descriptor, 0x0000, 0, READ_ID_BUS);
    device_register(&c64->system, c64->mos6510, &mos6510_descriptor, 0x0000, 0, READ_ID_MOS6510);
    device_register(&c64->system, c64->cia1, &cia_descriptor, 0xDC00, 0x100, READ_ID_CIA1);
    device_register(&c64->system, c64->cia2, &cia_descriptor, 0xDD00, 0x100, READ_ID_CIA2);
    device_register(&c64->system, c64->vic_ii, &vic_ii_descriptor, 0xD000, 0x400, READ_ID_VIC_II);
    device_register(&c64->system, c64->sid, &sid_descriptor, 0xD400, 0x400, READ_ID_SID);
    device_register(&c64->system, c64->custom, &custom_descriptor, 0xD800, 0x400, READ_ID_CUSTOM);
    for (int i = 0; i < c64->system.device_count; i++) {
        device_entry_t* dev = &c64->system.devices[i];
        if (dev->desc->init) {
            dev->desc->init(dev->device, i == 0 ? c64 : &c64->bus->interface);
        }
    }
    init_memory(&c64->system);
    c64_callbacks_init(c64);
    generate_pla_maps(c64);
    return c64;
}

// Initialization
void init_c64() {
    c64 = new_c64();
}

// Bus functions
uint8_t c64_bus_memory_read(void* context, uint16_t address) {
    c64_bus_t* bus = (c64_bus_t*)context;
    return c64_memory_read(bus->c64, address);
}

void c64_bus_memory_write(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* bus = (c64_bus_t*)context;
    c64_state_t* c64 = bus->c64;
    if (address == 0xDD00) {
        uint8_t bank = 3 - (value & 0x3);
        if (c64->vic_ii->desc->bank_change) {
            c64->vic_ii->desc->bank_change(c64->vic_ii, bank);
        }
    }
    c64_memory_write(c64, address, value);
}

void c64_bus_system_init(void* context, void* bus) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_bus->desc = &c64_bus_descriptor;
    c64_bus->interface.read = c64_bus_memory_read;
    c64_bus->interface.write = c64_bus_memory_write;
    c64_bus->c64 = (c64_state_t*)bus;
}

static device_descriptor_t c64_bus_descriptor = {
    .init = c64_bus_system_init,
    .read = c64_bus_memory_read,
    .write = c64_bus_memory_write,
    .bank_change = NULL
};
