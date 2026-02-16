#include "vic20_memory.h"
#include "vic20_chips.h"
#include "../../core/aiemuc.h"
#include "../../chip/io/mos6522.h"
#include "../../chip/video/vic/mos6560.h"
#include "../../chip/video/vic/mos6561.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// VIC-20 Memory Banking System - Optimized Encoded Bank Type Implementation
// ============================================================================
//
// This implementation uses a single encoded byte per 1KB bank (64 bytes total):
// - Bits [3:0] = read_type  (0=UNMAPPED, 1=IO, 2=ROM, 3=RAM)
// - Bits [7:4] = write_type (0=UNMAPPED/ROM, 1=IO, 3=RAM)
//
// Performance advantages:
// - Single array lookup (vs 2 bitmap extractions)
// - Branch-friendly: (type >= VIC20_TYPE_ROM) catches both RAM and ROM
// - Direct buffer access: buffer[addr] for all RAM/ROM
// - I/O dispatches via handler array with minimal overhead

// ============================================================================
// Bank Map Initialization - Optimized Encoded Bank Type
// ============================================================================

void vic20_bank_map_init(vic20_bank_map_t* map, uint8_t expansion_flags, bool cartridge_present) {
    // Clear the map (all banks set to UNMAPPED by default)
    memset(map->bank_type, VIC20_BANK_TYPE_UNMAPPED, 64);
    
    // Now set specific bank types according to VIC-20 memory map
    
    // Bank 0 ($0000-$03FF): Base RAM 0 - always present
    vic20_set_bank_ram(map, 0);
    
    // Banks 1-3 ($0400-$0FFF): Expansion block 0 (3KB)
    if (expansion_flags & VIC20_EXP_BLOCK0) {
        vic20_set_bank_ram(map, 1);
        vic20_set_bank_ram(map, 2);
        vic20_set_bank_ram(map, 3);
    }
    // else: remains UNMAPPED
    
    // Banks 4-7 ($1000-$1FFF): Base RAM 1 - always present
    vic20_set_bank_ram(map, 4);
    vic20_set_bank_ram(map, 5);
    vic20_set_bank_ram(map, 6);
    vic20_set_bank_ram(map, 7);
    
    // Banks 8-15 ($2000-$3FFF): Expansion block 2 (8KB)
    if (expansion_flags & VIC20_EXP_BLOCK2) {
        for (int i = 0; i < 8; i++) {
            vic20_set_bank_ram(map, 8 + i);
        }
    }
    
    // Banks 16-23 ($4000-$5FFF): Expansion block 3 (8KB)
    if (expansion_flags & VIC20_EXP_BLOCK3) {
        for (int i = 0; i < 8; i++) {
            vic20_set_bank_ram(map, 16 + i);
        }
    }
    
    // Banks 24-31 ($6000-$7FFF): Expansion block 5 (8KB)
    if (expansion_flags & VIC20_EXP_BLOCK5) {
        for (int i = 0; i < 8; i++) {
            vic20_set_bank_ram(map, 24 + i);
        }
    }
    
    // Banks 32-35 ($8000-$8FFF): Character ROM (4KB)
    vic20_set_bank_rom(map, 32);
    vic20_set_bank_rom(map, 33);
    vic20_set_bank_rom(map, 34);
    vic20_set_bank_rom(map, 35);
    
    // Banks 36-39 ($9000-$9FFF): I/O region
    // Now encoded as IO type for proper dispatch
    vic20_set_bank_io(map, 36);
    vic20_set_bank_io(map, 37);
    vic20_set_bank_io(map, 38);
    vic20_set_bank_io(map, 39);
    
    // Banks 40-47 ($A000-$BFFF): Cartridge ROM (8KB)
    if (cartridge_present) {
        for (int i = 0; i < 8; i++) {
            vic20_set_bank_rom(map, 40 + i);
        }
    }
    // else: remains UNMAPPED
    
    // Banks 48-55 ($C000-$DFFF): BASIC ROM (8KB)
    for (int i = 0; i < 8; i++) {
        vic20_set_bank_rom(map, 48 + i);
    }
    
    // Banks 56-63 ($E000-$FFFF): KERNAL ROM (8KB)
    for (int i = 0; i < 8; i++) {
        vic20_set_bank_rom(map, 56 + i);
    }
}

void vic20_bank_map_init_vic(vic20_bank_map_t* map, uint8_t expansion_flags) {
    // VIC chip has a 14-bit address space (16KB window = 16 banks).
    // We repeat the 16-bank pattern 4 times across all 64 entries so that
    // vic20_memory_vic_read can use (addr >> 10) directly without masking.
    //
    // VA13 (bit 13) hardware-selects Character ROM:
    //   - VA13=0 ($0000-$1FFF, banks 0-7): Character ROM (4KB mirrored)
    //   - VA13=1 ($2000-$3FFF, banks 8-15): RAM (VIC sees CPU $0000-$1FFF)
    
    // Initialize all 4 copies of the 16-bank pattern
    for (int copy = 0; copy < 4; copy++) {
        const int base = copy * 16;
        
        // Banks 0-7 (VIC $0000-$1FFF): Character ROM (4KB mirrored to 8KB)
        for (int i = 0; i < 8; i++) {
            vic20_set_bank_charrom(map, base + i);
        }
        
        // Bank 8 (VIC $2000-$23FF -> CPU $0000-$03FF): Base RAM 0 - always present
        vic20_set_bank_ram(map, base + 8);
        
        // Banks 9-11 (VIC $2400-$2FFF -> CPU $0400-$0FFF): Expansion block 0 or UNMAPPED
        if (expansion_flags & VIC20_EXP_BLOCK0) {
            vic20_set_bank_ram(map, base + 9);
            vic20_set_bank_ram(map, base + 10);
            vic20_set_bank_ram(map, base + 11);
        } else {
            map->bank_type[base + 9] = VIC20_BANK_TYPE_UNMAPPED;
            map->bank_type[base + 10] = VIC20_BANK_TYPE_UNMAPPED;
            map->bank_type[base + 11] = VIC20_BANK_TYPE_UNMAPPED;
        }
        
        // Banks 12-15 (VIC $3000-$3FFF -> CPU $1000-$1FFF): Base RAM 1 - always present
        for (int i = 0; i < 4; i++) {
            vic20_set_bank_ram(map, base + 12 + i);
        }
    }
}

// ============================================================================
// Memory System Creation and Destruction
// ============================================================================

static void* vic20_memory_chip_create(chip_descriptor_t* desc) {
    vic20_memory_t* mem = (vic20_memory_t*)calloc(1, sizeof(vic20_memory_t));
    if (!mem) return NULL;
    mem->desc = desc;
    return mem;
}

static void vic20_memory_chip_destroy(void* chip) {
    vic20_memory_t* mem = (vic20_memory_t*)chip;
    if (!mem) return;
    
    if (mem->buffer) {
        aiemuc_aligned_free(mem->buffer);
        mem->buffer = NULL;
    }
    
    free(mem);
}

chip_descriptor_t vic20_memory_descriptor = {
    /* description */ "VIC-20 Memory System",
    /* create */ vic20_memory_chip_create,
    /* destroy */ vic20_memory_chip_destroy,
    /* bus_attach */ NULL,
    /* bank_change */ NULL
#ifdef IMGUI_VERSION
    , /* render_debug_window */ NULL,
    /* render_settings_window */ NULL
#endif
};

vic20_memory_t* vic20_memory_create(uint8_t expansion_flags, bool cartridge_present) {
    vic20_memory_t* mem = (vic20_memory_t*)vic20_memory_chip_create(&vic20_memory_descriptor);
    if (!mem) return NULL;
    
    mem->expansion_flags = expansion_flags;
    mem->cartridge_present = cartridge_present;
    
    // Allocate 64KB unified buffer (aligned for cache efficiency)
    mem->buffer_size = 65536;
    mem->buffer = (uint8_t*)aiemuc_aligned_alloc(64, mem->buffer_size);
    if (!mem->buffer) {
        printf("VIC20 Memory: Failed to allocate 64KB unified buffer\n");
        free(mem);
        return NULL;
    }
    
    // Initialize buffer to zero (RAM starts cleared)
    memset(mem->buffer, 0, mem->buffer_size);
    
    // Initialize Color RAM to default color (at $9400 in unified buffer)
    // Color RAM is 4-bit wide, initialize to cyan (3) - standard VIC-20 text color
    // KERNAL will set proper colors during boot, but cyan on blue background is visible
    memset(mem->buffer + VIC20_BASE_COLOR_RAM, VIC_COLOR_CYAN, 1024);
    
    // Initialize bank maps
    vic20_bank_map_init(&mem->cpu_bank_map, expansion_flags, cartridge_present);
    vic20_bank_map_init_vic(&mem->vic_bank_map, expansion_flags);
    
    printf("VIC20 Memory: Created 64KB unified buffer, expansion=$%02X, cart=%s\n",
           expansion_flags, cartridge_present ? "yes" : "no");
    
    return mem;
}

void vic20_memory_destroy(vic20_memory_t* mem) {
    if (mem) {
        vic20_memory_chip_destroy(mem);
    }
}

void vic20_memory_attach_system(vic20_memory_t* mem, void* vic20_system) {
    if (!mem) return;
    mem->vic20 = vic20_system;
}

// ============================================================================
// I/O Handler Functions
// ============================================================================

// Floating bus handler for unmapped I/O regions
static bus_state_t vic20_io_unmapped_read(void* ctx, bus_state_t bus_state) {
    (void)ctx;
    BUS_SET_DATA(bus_state, 0xFF);  // Floating bus
    return bus_state;
}

static bus_state_t vic20_io_unmapped_write(void* ctx, bus_state_t bus_state) {
    (void)ctx;
    return bus_state;  // Write ignored
}

// VIC + VIA I/O handler ($9000-$93FF)
// VIC: 16 registers at $9x00-$9x0F
// VIA1: 16 registers at $9x10-$9x1F
// VIA2: 16 registers at $9x20-$9x2F
// All mirror throughout the 1KB range
static bus_state_t vic20_io_vic_via_read(void* ctx, bus_state_t bus_state) {
    vic20_memory_t* mem = (vic20_memory_t*)ctx;
    uint16_t addr = BUS_GET_ADDR(bus_state);
    uint8_t offset = addr & 0x3F;  // 64-byte pattern repeats
    
    if (offset < 0x10) {
        // VIC registers
        if (mem->vic_chip) {
            uint8_t data = mos6560_read_register((mos6560_t*)mem->vic_chip, offset & 0x0F);
            BUS_SET_DATA(bus_state, data);
        }
    } else if (offset < 0x20) {
        // VIA1 registers
        if (mem->via1_chip) {
            bus_state_t via_state = 0;
            BUS_SET_ADDR(via_state, offset & 0x0F);
            via_state = mos6522_registers_read((mos6522_t*)mem->via1_chip, via_state);
            BUS_SET_DATA(bus_state, BUS_GET_DATA(via_state));
        }
    } else if (offset < 0x30) {
        // VIA2 registers
        if (mem->via2_chip) {
            bus_state_t via_state = 0;
            BUS_SET_ADDR(via_state, offset & 0x0F);
            via_state = mos6522_registers_read((mos6522_t*)mem->via2_chip, via_state);
            BUS_SET_DATA(bus_state, BUS_GET_DATA(via_state));
        }
    } else {
        // $9x30-$9x3F: VIC mirrors
        if (mem->vic_chip) {
            uint8_t data = mos6560_read_register((mos6560_t*)mem->vic_chip, offset & 0x0F);
            BUS_SET_DATA(bus_state, data);
        }
    }
    
    return bus_state;
}

static bus_state_t vic20_io_vic_via_write(void* ctx, bus_state_t bus_state) {
    vic20_memory_t* mem = (vic20_memory_t*)ctx;
    uint16_t addr = BUS_GET_ADDR(bus_state);
    uint8_t offset = addr & 0x3F;
    uint8_t data = BUS_GET_DATA(bus_state);
    
    if (offset < 0x10) {
        // VIC registers
        if (mem->vic_chip) {
            mos6560_write_register((mos6560_t*)mem->vic_chip, offset & 0x0F, data);
        }
    } else if (offset < 0x20) {
        // VIA1 registers
        if (mem->via1_chip) {
            bus_state_t via_state = 0;
            BUS_SET_ADDR(via_state, offset & 0x0F);
            BUS_SET_DATA(via_state, data);
            mos6522_registers_write((mos6522_t*)mem->via1_chip, via_state);
        }
    } else if (offset < 0x30) {
        // VIA2 registers
        if (mem->via2_chip) {
            bus_state_t via_state = 0;
            BUS_SET_ADDR(via_state, offset & 0x0F);
            BUS_SET_DATA(via_state, data);
            mos6522_registers_write((mos6522_t*)mem->via2_chip, via_state);
        }
    } else {
        // $9x30-$9x3F: VIC mirrors
        if (mem->vic_chip) {
            mos6560_write_register((mos6560_t*)mem->vic_chip, offset & 0x0F, data);
        }
    }
    
    return bus_state;
}

// Color RAM I/O handler ($9400-$97FF)
// 4-bit wide RAM, upper 4 bits read as garbage
static bus_state_t vic20_io_colorram_read(void* ctx, bus_state_t bus_state) {
    vic20_memory_t* mem = (vic20_memory_t*)ctx;
    uint16_t addr = BUS_GET_ADDR(bus_state);
    uint16_t offset = addr & 0x03FF;  // 1KB mask
    
    // Read from unified buffer at $9400, upper bits are garbage
    uint8_t data = mem->buffer[VIC20_BASE_COLOR_RAM + offset] | 0xF0;
    BUS_SET_DATA(bus_state, data);
    return bus_state;
}

static bus_state_t vic20_io_colorram_write(void* ctx, bus_state_t bus_state) {
    vic20_memory_t* mem = (vic20_memory_t*)ctx;
    uint16_t addr = BUS_GET_ADDR(bus_state);
    uint16_t offset = addr & 0x03FF;
    uint8_t data = BUS_GET_DATA(bus_state);
    
    // Only store lower 4 bits
    mem->buffer[VIC20_BASE_COLOR_RAM + offset] = data & 0x0F;
    return bus_state;
}

void vic20_memory_init_io_handlers(vic20_memory_t* mem) {
    if (!mem) return;
    
    // Handler 0: $9000-$93FF - VIC + VIA registers with mirrors
    mem->io_handlers[0].read_handler = vic20_io_vic_via_read;
    mem->io_handlers[0].read_context = mem;
    mem->io_handlers[0].write_handler = vic20_io_vic_via_write;
    mem->io_handlers[0].write_context = mem;
    
    // Handler 1: $9400-$97FF - Color RAM
    mem->io_handlers[1].read_handler = vic20_io_colorram_read;
    mem->io_handlers[1].read_context = mem;
    mem->io_handlers[1].write_handler = vic20_io_colorram_write;
    mem->io_handlers[1].write_context = mem;
    
    // Handler 2: $9800-$9BFF - I/O expansion 2 (unmapped by default)
    mem->io_handlers[2].read_handler = vic20_io_unmapped_read;
    mem->io_handlers[2].read_context = NULL;
    mem->io_handlers[2].write_handler = vic20_io_unmapped_write;
    mem->io_handlers[2].write_context = NULL;
    
    // Handler 3: $9C00-$9FFF - I/O expansion 3 (unmapped by default)
    mem->io_handlers[3].read_handler = vic20_io_unmapped_read;
    mem->io_handlers[3].read_context = NULL;
    mem->io_handlers[3].write_handler = vic20_io_unmapped_write;
    mem->io_handlers[3].write_context = NULL;
    
    printf("VIC20 Memory: I/O handlers initialized\n");
}

// ============================================================================
// Memory Access Functions - Ultra-Optimized Implementation
// ============================================================================

/**
 * Ultra-optimized CPU memory tick function.
 * Uses encoded bank types for minimal branch overhead.
 * 
 * Fast path: RAM and ROM access via direct buffer[addr] lookup
 * I/O path: Handler dispatch via pre-initialized handler array
 * 
 * @param mem Pointer to memory system
 * @param bus_state Current bus state with address, data, and control lines
 * @return Updated bus state after memory access
 */
bus_state_t REGISTER_CALL vic20_memory_cpu_tick(vic20_memory_t* mem, bus_state_t bus_state) {
    if (unlikely(!mem)) return bus_state;
    
    const uint16_t addr = BUS_GET_ADDR(bus_state);
    const uint8_t bank = addr >> 10;  // 1KB bank (0-63)
    const uint8_t encoded = mem->cpu_bank_map.bank_type[bank];
    const bool is_read = (BUS_GET_LINES(bus_state) & BUS_MASK_RW) != 0;
    
    if (is_read) {
        // === READ OPERATION ===
        const uint8_t read_type = vic20_decode_read_type(encoded);
        
        if (likely(read_type >= VIC20_TYPE_ROM)) {
            // RAM or ROM - most common path: direct buffer access
            BUS_SET_DATA(bus_state, mem->buffer[addr]);
        } else if (read_type == VIC20_TYPE_IO) {
            // I/O region - dispatch to handler
            // Handler index: (addr >> 10) & 3 gives 0-3 for $9000-$9FFF
            const uint8_t io_page = (addr >> 10) & 3;
            bus_state = mem->io_handlers[io_page].read_handler(
                mem->io_handlers[io_page].read_context, bus_state);
        }
        // UNMAPPED (read_type == 0): leave data unchanged (floating bus)
    } else {
        // === WRITE OPERATION ===
        const uint8_t write_type = vic20_decode_write_type(encoded);
        
        if (likely(write_type == VIC20_TYPE_RAM)) {
            // RAM - direct buffer write
            mem->buffer[addr] = BUS_GET_DATA(bus_state);
        } else if (write_type == VIC20_TYPE_IO) {
            // I/O region - dispatch to handler
            const uint8_t io_page = (addr >> 10) & 3;
            bus_state = mem->io_handlers[io_page].write_handler(
                mem->io_handlers[io_page].write_context, bus_state);
        }
        // ROM (write_type == 0) or UNMAPPED: write ignored
    }
    
    return bus_state;
}

/**
 * VIC memory read function - handles memory reads from VIC chip.
 * 
 * The VIC has a 14-bit address bus (VA0-VA13) giving a 16KB window.
 * The 16-bank pattern is repeated 4x in the 64-entry bank map, so we can
 * use (addr >> 10) directly without masking - any 6-bit result is valid.
 *   - Banks 0-7 ($0000-$1FFF): CHARROM → buffer[$8000 + (addr & 0x0FFF)]
 *   - Banks 8-15 ($2000-$3FFF): RAM → buffer[addr & 0x1FFF]
 * 
 * @param mem Pointer to memory system
 * @param addr 14-bit address from VIC
 * @return Data byte at the specified address
 */
uint8_t vic20_memory_vic_read(vic20_memory_t* mem, uint16_t addr) {
    if (unlikely(!mem)) return 0xFF;

    // Direct bank lookup - bank map repeated 4x so no masking needed
    const uint8_t bank = addr >> 10;
    const uint8_t read_type = vic20_decode_read_type(mem->vic_bank_map.bank_type[bank]);

    // Single branch for all valid memory (CHARROM, ROM, RAM)
    // CHARROM=4, ROM=2, RAM=3, so all are >= VIC20_TYPE_ROM
    if (likely(read_type >= VIC20_TYPE_ROM)) {
        // CHARROM: base=$8000, mask=0x0FFF (4KB mirrored to 8KB)
        // RAM:     base=0,     mask=0x1FFF (maps to CPU $0000-$1FFF)
        const bool is_charrom = (read_type == VIC20_TYPE_CHARROM);
        const uint16_t base = is_charrom ? VIC20_BASE_CHARROM : 0;
        const uint16_t mask = is_charrom ? 0x0FFF : 0x1FFF;
        return mem->buffer[base + (addr & mask)];
    }

    // Unmapped (expansion block 0 at $0400-$0FFF when not expanded)
    return 0xFF;
}

uint8_t vic20_memory_color_read(vic20_memory_t* mem, uint16_t addr) {
    if (!mem) return 0x0F;
    
    // Color RAM is at $9400 in unified buffer, 1KB size
    const uint16_t offset = addr & 0x03FF;

    return mem->buffer[VIC20_BASE_COLOR_RAM + offset] & 0x0F;
}

uint8_t vic20_memory_read_byte(vic20_memory_t* mem, uint16_t addr) {
    if (!mem) return 0xFF;
    
    bus_state_t bus_state = 0;
    BUS_SET_ADDR(bus_state, addr);
    BUS_SET_LINES(bus_state, BUS_MASK_RW);  // Read mode
    
    bus_state = vic20_memory_cpu_tick(mem, bus_state);
    return BUS_GET_DATA(bus_state);
}

void vic20_memory_write_byte(vic20_memory_t* mem, uint16_t addr, uint8_t value) {
    if (!mem) return;
    
    bus_state_t bus_state = 0;
    BUS_SET_ADDR(bus_state, addr);
    BUS_SET_DATA(bus_state, value);
    // BUS_MASK_RW clear = write mode
    
    vic20_memory_cpu_tick(mem, bus_state);
}

// ============================================================================
// ROM Loading Functions
// ============================================================================

bool vic20_memory_load_rom(vic20_memory_t* mem, uint16_t addr, const uint8_t* data, size_t size) {
    if (!mem || !data) return false;
    
    // Validate address range
    if (addr + size > 65536) {
        printf("VIC20 Memory: ROM load address $%04X + size %zu exceeds 64KB\n", addr, size);
        return false;
    }
    
    // Direct copy to unified buffer (bypass write protection)
    memcpy(mem->buffer + addr, data, size);
    
    // Note: We do NOT mirror Character ROM to $9000 because that would
    // overwrite Color RAM at $9400-$97FF and I/O registers at $9000-$93FF.
    
    printf("VIC20 Memory: Loaded %zuKB ROM at $%04X\n", size / 1024, addr);
    return true;
}

uint8_t* vic20_memory_get_ptr(vic20_memory_t* mem, uint16_t addr) {
    if (!mem) return NULL;
    return mem->buffer + addr;
}

uint8_t* vic20_memory_get_colorram_ptr(vic20_memory_t* mem) {
    if (!mem) return NULL;
    return mem->buffer + VIC20_BASE_COLOR_RAM;
}

void vic20_memory_set_expansion(vic20_memory_t* mem, uint8_t expansion_flags) {
    if (!mem) return;
    
    mem->expansion_flags = expansion_flags;
    
    // Reinitialize bank maps with new expansion configuration
    vic20_bank_map_init(&mem->cpu_bank_map, expansion_flags, mem->cartridge_present);
    vic20_bank_map_init_vic(&mem->vic_bank_map, expansion_flags);
    
    printf("VIC20 Memory: Expansion updated to $%02X\n", expansion_flags);
}
