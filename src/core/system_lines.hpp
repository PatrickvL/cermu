#pragma once
#include <cstdint>
/*
Unified 64-bit bus state layout
Layout:
- [DATA:7-0]    (lowest 8 bits)
- [ADDR:23-8]   (next 16 bits)
- [BANK:31-24]  (next 8 bits)
- [INPUT PINS:47-32]  (active-low for IRQ/NMI/RES, others active-high)
- [OUTPUT PINS:63-48] (active-high)
All pin access uses direct BUS_GET_BIT/BUS_SET_BIT/BUS_CLR_BIT operations.
*/

typedef uint64_t bus_state_t;

/* Field shifts and masks */
#define BUS_DATA_SHIFT      0
#define BUS_ADDR_SHIFT      8
#define BUS_BANK_SHIFT      24

#define BUS_DATA_MASK       0x00000000000000FFULL
#define BUS_ADDR_MASK       0x0000000000FFFF00ULL
#define BUS_BANK_MASK       0x00000000FF000000ULL
/* Combined 24-bit address: ADDR (bits 15-0) + BANK (bits 23-16).
   Used by CPUs with >16-bit address buses (M68000: 24-bit).
   8-bit systems continue using BUS_GET_ADDR / BUS_SET_ADDR unchanged. */
#define BUS_ADDR24_MASK     (BUS_ADDR_MASK | BUS_BANK_MASK)       /* 0x00000000FFFFFF00ULL */

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
#define BUS_GET_ADDR24(state)   ((uint32_t)(((state) & BUS_ADDR24_MASK) >> BUS_ADDR_SHIFT))

/* XOR-AND-XOR field write — 3 ops, no NOT, matches bitmix() convention.
   The field mask isolates the target bits so excess bits in the value
   argument are automatically discarded. */
#define BUS_SET_DATA(state, data)   ((state) = (state) ^ (((state) ^ ((bus_state_t)(data) << BUS_DATA_SHIFT)) & BUS_DATA_MASK))
#define BUS_SET_ADDR(state, addr)   ((state) = (state) ^ (((state) ^ ((bus_state_t)(addr) << BUS_ADDR_SHIFT)) & BUS_ADDR_MASK))
#define BUS_SET_BANK(state, bank)   ((state) = (state) ^ (((state) ^ ((bus_state_t)(bank) << BUS_BANK_SHIFT)) & BUS_BANK_MASK))
#define BUS_SET_ADDR24(state, addr) ((state) = (state) ^ (((state) ^ ((bus_state_t)(addr) << BUS_ADDR_SHIFT)) & BUS_ADDR24_MASK))

/* Bitwise multiplexer within a bus_state_t field.
   Merges NEW_VAL into STATE's field (at FIELD_MASK / SHIFT) using DATA_MASK:
   where DATA_MASK bit = 1 → take from NEW_VAL, where 0 → keep from STATE.
   Compiles to the same XOR-AND-XOR as BUS_SET_*, but with a narrower mask
   so only the data-mask–selected bits change.  The compound mask is a
   compile-time constant when DATA_MASK is, so cost is identical. */
#define BUS_BITMIX(state, new_val, data_mask, field_mask, shift) \
    ((state) = (state) ^ (((state) ^ ((bus_state_t)(new_val) << (shift))) \
                          & ((bus_state_t)(data_mask) << (shift)) & (field_mask)))

/* Convenience: bitmix within the DATA field (bits 7-0). */
#define BUS_BITMIX_DATA(state, new_val, data_mask) \
    BUS_BITMIX(state, new_val, data_mask, BUS_DATA_MASK, BUS_DATA_SHIFT)

/* ── Data bus floating ────────────────────────────────────────────────────
   For systems with TTL/NMOS buses (NES, etc.) where undriven data lines
   float toward a deterministic idle level.  Call once per bus cycle before
   the address-decode / memory-tick to model impedance pull-up or pull-down.

   BUS_FLOAT_DATA_HIGH — all 8 data bits → 1  (NMOS pull-up default)
   BUS_FLOAT_DATA_LOW  — all 8 data bits → 0  (CMOS pull-down default)

   For gradual decay, apply an LFSR mask to float a subset of bits per tick:
     BUS_FLOAT_DATA_DECAY_HIGH(state, mask) — set only bits where mask = 1
     BUS_FLOAT_DATA_DECAY_LOW (state, mask) — clear only bits where mask = 1
   Feed the mask from a 16-bit Galois LFSR (see lfsr16_step in cermu.h) to
   randomly select ~50% of bits each tick; repeated application floats all
   bits to the idle level after a few cycles. */
#define BUS_FLOAT_DATA_HIGH(state)              ((state) |=  BUS_DATA_MASK)
#define BUS_FLOAT_DATA_LOW(state)               ((state) &= ~BUS_DATA_MASK)
#define BUS_FLOAT_DATA_DECAY_HIGH(state, mask)  BUS_BITMIX_DATA(state, 0xFF, mask)
#define BUS_FLOAT_DATA_DECAY_LOW(state, mask)   BUS_BITMIX_DATA(state, 0x00, mask)
