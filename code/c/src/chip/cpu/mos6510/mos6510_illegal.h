#ifndef MOS6510_ILLEGAL_H
#define MOS6510_ILLEGAL_H

#include "mos6510.h"

// ============================================================================
// MOS 6510 ILLEGAL/UNOFFICIAL INSTRUCTION DECLARATIONS
// ============================================================================
// These are MOS6510-specific illegal opcodes that override the family defaults

// Forward declarations for all MOS6510 illegal opcode handlers
void ahx_indirect_y_func(mos6510_t* cpu);
void ahx_absolute_y_func(mos6510_t* cpu);
void alr_immediate_func(mos6510_t* cpu);
void anc_immediate_func(mos6510_t* cpu);
void arr_immediate_func(mos6510_t* cpu);
void axs_immediate_func(mos6510_t* cpu);
void dcp_indirect_x_func(mos6510_t* cpu);
void dcp_zero_page_func(mos6510_t* cpu);
void dcp_zero_page_x_func(mos6510_t* cpu);
void dcp_absolute_func(mos6510_t* cpu);
void dcp_absolute_x_func(mos6510_t* cpu);
void dcp_absolute_y_func(mos6510_t* cpu);
void dcp_indirect_y_func(mos6510_t* cpu);
void isc_indirect_x_func(mos6510_t* cpu);
void isc_zero_page_func(mos6510_t* cpu);
void isc_zero_page_x_func(mos6510_t* cpu);
void isc_absolute_func(mos6510_t* cpu);
void isc_absolute_x_func(mos6510_t* cpu);
void isc_absolute_y_func(mos6510_t* cpu);
void isc_indirect_y_func(mos6510_t* cpu);
void jam_func(mos6510_t* cpu);
void las_absolute_y_func(mos6510_t* cpu);
void lax_immediate_func(mos6510_t* cpu);
void lax_zero_page_func(mos6510_t* cpu);
void lax_zero_page_y_func(mos6510_t* cpu);
void lax_absolute_func(mos6510_t* cpu);
void lax_absolute_y_func(mos6510_t* cpu);
void lax_indirect_x_func(mos6510_t* cpu);
void lax_indirect_y_func(mos6510_t* cpu);
void nop_immediate_func(mos6510_t* cpu);
void nop_zero_page_func(mos6510_t* cpu);
void nop_zero_page_x_func(mos6510_t* cpu);
void nop_absolute_func(mos6510_t* cpu);
void nop_absolute_x_func(mos6510_t* cpu);
void rla_indirect_x_func(mos6510_t* cpu);
void rla_zero_page_func(mos6510_t* cpu);
void rla_zero_page_x_func(mos6510_t* cpu);
void rla_absolute_func(mos6510_t* cpu);
void rla_absolute_x_func(mos6510_t* cpu);
void rla_absolute_y_func(mos6510_t* cpu);
void rla_indirect_y_func(mos6510_t* cpu);
void rra_indirect_x_func(mos6510_t* cpu);
void rra_zero_page_func(mos6510_t* cpu);
void rra_zero_page_x_func(mos6510_t* cpu);
void rra_absolute_func(mos6510_t* cpu);
void rra_absolute_x_func(mos6510_t* cpu);
void rra_absolute_y_func(mos6510_t* cpu);
void rra_indirect_y_func(mos6510_t* cpu);
void sax_zero_page_func(mos6510_t* cpu);
void sax_zero_page_y_func(mos6510_t* cpu);
void sax_absolute_func(mos6510_t* cpu);
void sax_indirect_x_func(mos6510_t* cpu);
void shx_absolute_y_func(mos6510_t* cpu);
void shy_absolute_x_func(mos6510_t* cpu);
void slo_indirect_x_func(mos6510_t* cpu);
void slo_zero_page_func(mos6510_t* cpu);
void slo_zero_page_x_func(mos6510_t* cpu);
void slo_absolute_func(mos6510_t* cpu);
void slo_absolute_x_func(mos6510_t* cpu);
void slo_absolute_y_func(mos6510_t* cpu);
void slo_indirect_y_func(mos6510_t* cpu);
void sre_indirect_x_func(mos6510_t* cpu);
void sre_zero_page_func(mos6510_t* cpu);
void sre_zero_page_x_func(mos6510_t* cpu);
void sre_absolute_func(mos6510_t* cpu);
void sre_absolute_x_func(mos6510_t* cpu);
void sre_absolute_y_func(mos6510_t* cpu);
void sre_indirect_y_func(mos6510_t* cpu);
void tas_absolute_y_func(mos6510_t* cpu);
void xaa_immediate_func(mos6510_t* cpu);

// Wrapper functions for family operations used in illegal opcodes
void sbc_immediate_func(mos6510_t* cpu);

// NOP wrapper functions
void nop_func(mos6510_t* cpu);
void nop_immediate_func(mos6510_t* cpu);
void nop_zero_page_func(mos6510_t* cpu);
void nop_zero_page_x_func(mos6510_t* cpu);
void nop_absolute_func(mos6510_t* cpu);
void nop_absolute_x_func(mos6510_t* cpu);

// Function to apply MOS6510-specific illegal opcode overrides
void mos6510_apply_illegal_opcodes(mos6510_t* cpu);

#endif // MOS6510_ILLEGAL_H
