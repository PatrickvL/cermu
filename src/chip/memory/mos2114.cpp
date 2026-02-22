#include "mos2114.h"
#include "../../core/system_lines.h"
#include <cstring>

// ============================================================================
// MOS2114 — constructor
// ============================================================================
MOS2114::MOS2114() {
    std::memset(memory, 0, sizeof(memory));
}

// ============================================================================
// ChipBase identity
// ============================================================================
ChipIdentity MOS2114::chip_identity() const {
    return {"MOS2114", "MOS Technology"};
}

bool MOS2114::has_debug_content()    const { return true; }
bool MOS2114::has_settings_content() const { return true; }
bool MOS2114::has_layout_content()   const { return true; }

// ============================================================================
// Bus interface — static methods usable as C function pointers
// ============================================================================
bus_state_t MOS2114::bus_read(void* context, bus_state_t bus_state) {
    auto* self = static_cast<MOS2114*>(context);
    // Color RAM is mapped at $D800–$DBFF (1024 bytes)
    // Mask to 10 bits for 1K addressing
    uint16_t offset = BUS_GET_ADDR(bus_state) & 0x3FF;

    // MOS2114 is 4-bit wide — lower 4 bits valid, upper 4 float from bus
    uint8_t color_nibble = self->memory[offset];
    uint8_t floating_upper_bits = BUS_GET_DATA(bus_state) & 0xF0;
    BUS_SET_DATA(bus_state, floating_upper_bits | color_nibble);
    return bus_state;
}

bus_state_t MOS2114::bus_write(void* context, bus_state_t bus_state) {
    auto* self = static_cast<MOS2114*>(context);

    // HARDWARE REFERENCE: PLA _GRW Signal for Color RAM Write Control
    // ================================================================
    // In real C64 hardware, Color RAM writes are gated by the PLA's _GRW
    // signal.  The PLA MOS 906114-01 generates _GRW specifically for Color RAM.
    //
    // _GRW active-low conditions:
    //   - I/O region enabled (!n_io = low)
    //   - Address in Color RAM range ($D800–$DBFF)
    //   - CPU writing (!r_w = low)
    //   - Memory configuration allows I/O access (CHAREN bit)

    // MOS2114 is 4-bit wide — store only lower nibble
    uint16_t offset = BUS_GET_ADDR(bus_state) & 0x3FF;
    self->memory[offset] = BUS_GET_DATA(bus_state) & 0x0F;
    return bus_state;
}

// ============================================================================
// Legacy free-function bus wrappers (c64_bus.cpp I/O handler table)
// ============================================================================
bus_state_t mos2114_read(void* context, bus_state_t bus_state) {
    return MOS2114::bus_read(context, bus_state);
}

bus_state_t mos2114_write(void* context, bus_state_t bus_state) {
    return MOS2114::bus_write(context, bus_state);
}

// ============================================================================
// Legacy lifecycle helpers
// ============================================================================
MOS2114* mos2114_create() {
    return new MOS2114();
}

void mos2114_destroy(MOS2114* chip) {
    delete chip;
}