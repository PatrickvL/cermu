#include "chip/memory/mos2114.hpp"
#include "core/system_lines.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"
#include <cstring>

// ============================================================================
// MOS2114 — constructor
// ============================================================================
MOS2114::MOS2114()
    : MemoryChipBase(ChipInfo{"MOS2114", "MOS Technology", "MOS 2114"})
{
    std::memset(memory, 0, sizeof(memory));
#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif
}

#ifdef CERMU_HAS_GUI
bool MOS2114::has_settings_content() const { return true; }
#endif

#ifdef CERMU_HAS_CHIP_DEBUG
void MOS2114::register_debug_fields() {
    debug_registry_
        .category("Color RAM")
        .memory("RAM Contents",
                +[](const ChipBase* c) -> std::pair<const uint8_t*, size_t> {
                    auto* self = static_cast<const MOS2114*>(c);
                    return {self->memory, sizeof(self->memory)};
                },
                0xD800, 1024);
}
#endif // CERMU_HAS_CHIP_DEBUG

// ============================================================================
// Bus interface — static methods for I/O handler table
// ============================================================================
bus_state_t MOS2114::bus_read(void* context, bus_state_t bus_state) {
    auto* self = static_cast<MOS2114*>(context);
#ifdef CERMU_HAS_GUI
    self->bus_snapshot_ = bus_state;
#endif
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
#ifdef CERMU_HAS_GUI
    self->bus_snapshot_ = bus_state;
#endif

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

REGISTER_CHIP_TYPE("MOS2114", MOS2114)
