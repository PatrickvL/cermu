#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
Unified 64-bit bus state layout (compatible with new C++ core pins)
Layout:
- [DATA:7-0]    (lowest 8 bits)
- [ADDR:23-8]   (next 16 bits)
- [BANK:31-24]  (next 8 bits)
- [INPUT PINS:47-32]  (active-low for IRQ/NMI/RES, others active-high)
- [OUTPUT PINS:63-48] (active-high)
All pin access uses direct BUS_GET_BIT/BUS_SET_BIT/BUS_CLR_BIT operations.
Legacy BUS_MASK, bus_lines_apply, BUS_STATE retained for NES and test helpers.
*/

typedef uint64_t bus_state_t;

/* Field shifts and masks */
#define BUS_DATA_SHIFT      0
#define BUS_ADDR_SHIFT      8
#define BUS_BANK_SHIFT      24

#define BUS_DATA_MASK       0x00000000000000FFULL
#define BUS_ADDR_MASK       0x0000000000FFFF00ULL
#define BUS_BANK_MASK       0x00000000FF000000ULL

/* Core pin bit indices */
#define BUS_RES_BIT     32  // Reset (active low)
#define BUS_IRQ_BIT     33  // IRQ (active low)
#define BUS_NMI_BIT     34  // NMI (active low)
#define BUS_RDY_BIT     35  // Ready
#define BUS_SO_BIT      36  // Set Overflow (active low)
#define BUS_AEC_BIT     37  // Address Enable Control
#define BUS_BE_BIT      38  // Bus Enable
#define BUS_ABORT_BIT   39  // Abort (active low)
#define BUS_VPB_BIT     40  // Vector Pull Bar (65C816)
#define BUS_E_BIT       41  // Enable (65C816)
#define BUS_MX_BIT      42  // Memory/Index size (65C816)
#define BUS_VDA_BIT     43  // Valid Data Address (65C816)
#define BUS_PHI0_BIT    44  // φ0 clock input
#define BUS_PHI1_BIT    45  // φ1 clock output
#define BUS_CNT_BIT     46  // CIA CNT pin (Counter input)
#define BUS_FLAG_BIT    47  // CIA FLAG pin (Interrupt input)

/* Output pins */
#define BUS_RW_BIT      48  // 1 = Read, 0 = Write
#define BUS_SYNC_BIT    49  // Synchronize
#define BUS_PHI2_BIT    50  // φ2 clock output
#define BUS_SP_BIT      51  // CIA SP pin (Serial Port data)
#define BUS_BA_BIT      52  // Bus available
#define BUS_VP_BIT      53  // Vector Pull
#define BUS_ML_BIT      54  // Memory Lock
/* 55-63 reserved */

/* Helpers */
#define BUS_BIT(bit)            (1ULL << (bit))
#define BUS_GET_BIT(state, bit) (((state) & BUS_BIT(bit)) != 0)

/* Direct single-pin manipulation — avoids the full extract→modify→apply
   roundtrip through bus_lines_extract/bus_lines_apply.  Use these when
   setting or clearing individual active-high pins (BA, AEC, RW, RDY). */
#define BUS_SET_BIT(state, bit)    ((state) |=  BUS_BIT(bit))
#define BUS_CLR_BIT(state, bit)    ((state) &= ~BUS_BIT(bit))

/* High-performance field access macros */
#define BUS_GET_DATA(state)     ((uint8_t) (((state) & BUS_DATA_MASK) >> BUS_DATA_SHIFT))
#define BUS_GET_ADDR(state)     ((uint16_t)(((state) & BUS_ADDR_MASK) >> BUS_ADDR_SHIFT))
#define BUS_GET_BANK(state)     ((uint8_t) (((state) & BUS_BANK_MASK) >> BUS_BANK_SHIFT))

#define BUS_SET_DATA(state, data)   ((state) = ((state) & ~BUS_DATA_MASK) | (((uint64_t)(data) & 0xFFULL) << BUS_DATA_SHIFT))
#define BUS_SET_ADDR(state, addr)   ((state) = ((state) & ~BUS_ADDR_MASK) | (((uint64_t)(addr) & 0xFFFFULL) << BUS_ADDR_SHIFT))
#define BUS_SET_BANK(state, bank)   ((state) = ((state) & ~BUS_BANK_MASK) | (((uint64_t)(bank) & 0xFFULL) << BUS_BANK_SHIFT))

/* Legacy bus control line definitions (kept stable for callers) */
#define BUS_LINE_IRQ    0 // Interrupt request line (legacy: 1 = asserted)
#define BUS_LINE_NMI    1 // Non-maskable interrupt line (legacy: 1 = asserted)
#define BUS_LINE_RW     2 // Read/Write line (1 = read, 0 = write)
#define BUS_LINE_BA     3 // Bus available line (1 = available)
#define BUS_LINE_AEC    4 // Address enable control line (1 = CPU drives address bus)
#define BUS_LINE_RDY    5 // Ready line (1 = ready)

/* Legacy bit masks (on the synthetic 8-bit lines value) */
#define BUS_MASK_IRQ        (1 << BUS_LINE_IRQ)
#define BUS_MASK_NMI        (1 << BUS_LINE_NMI)
#define BUS_MASK_RW         (1 << BUS_LINE_RW)
#define BUS_MASK_BA         (1 << BUS_LINE_BA)
#define BUS_MASK_AEC        (1 << BUS_LINE_AEC)
#define BUS_MASK_RDY        (1 << BUS_LINE_RDY)

/* Apply legacy 8-bit LINES to 64-bit pin layout (retained for BUS_STATE helper) */
static inline bus_state_t bus_lines_apply(bus_state_t s, uint8_t lines) {
    /* IRQ/NMI are active-low inputs on the core */
    if (lines & BUS_MASK_IRQ)  BUS_CLR_BIT(s, BUS_IRQ_BIT); else BUS_SET_BIT(s, BUS_IRQ_BIT);
    if (lines & BUS_MASK_NMI)  BUS_CLR_BIT(s, BUS_NMI_BIT); else BUS_SET_BIT(s, BUS_NMI_BIT);

    /* Active-high pins */
    if (lines & BUS_MASK_RW)   BUS_SET_BIT(s, BUS_RW_BIT);  else BUS_CLR_BIT(s, BUS_RW_BIT);
    if (lines & BUS_MASK_BA)   BUS_SET_BIT(s, BUS_BA_BIT);  else BUS_CLR_BIT(s, BUS_BA_BIT);
    if (lines & BUS_MASK_AEC)  BUS_SET_BIT(s, BUS_AEC_BIT); else BUS_CLR_BIT(s, BUS_AEC_BIT);
    if (lines & BUS_MASK_RDY)  BUS_SET_BIT(s, BUS_RDY_BIT); else BUS_CLR_BIT(s, BUS_RDY_BIT);

    return s;
}

/* Constructor helper — uses bus_lines_apply for legacy mask→pin-bit translation */
/* Prefer direct BUS_SET_BIT/BUS_CLR_BIT for new code. */
static inline bus_state_t bus_state_make(uint16_t addr, uint8_t data, uint8_t lines) {
    bus_state_t s = 0;
    s |= (((bus_state_t)data & 0xFFULL) << BUS_DATA_SHIFT);
    s |= (((bus_state_t)addr & 0xFFFFULL) << BUS_ADDR_SHIFT);
    /* BANK defaults to 0 for 6502/6510 family */
    s = bus_lines_apply(s, lines);
    return s;
}
#define BUS_STATE(addr, data, lines) (bus_state_make((uint16_t)(addr), (uint8_t)(data), (uint8_t)(lines)))
