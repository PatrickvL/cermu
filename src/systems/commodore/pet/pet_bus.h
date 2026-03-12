#pragma once
/*
 * pet_bus.h — Commodore PET Bus State
 *
 * Simple bus structure for the PET.  The PET has a straightforward
 * fixed memory map (no PLA, no banking), making the bus logic simpler
 * than the C64 or even the VIC-20.
 */

#include <cstdint>

#include "core/bus_cycle_interface.h"
#include "core/system_lines.h"
#include "chip/cpu/fam65xx/mos6502.h"

// PET default bus state — MOS6502 defaults + data lines all high (pull-up)
#define PET_BUS_DEFAULT_STATE \
    (MOS6502::default_bus_state() | BUS_DATA_MASK)

// PET bus structure
struct pet_bus_t {
    bus_state_t state;
    bus_state_t default_state;
};
