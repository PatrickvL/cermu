#pragma once
#include "fam65xx_core.h"

// AHX
void fam65xx_ahx_indirect_y(fam65xx_t* cpu);
void fam65xx_ahx_absolute_y(fam65xx_t* cpu);

// ALR/ANC/ARR/AXS (immediate)
void fam65xx_alr_immediate(fam65xx_t* cpu);
void fam65xx_anc_immediate(fam65xx_t* cpu);
void fam65xx_arr_immediate(fam65xx_t* cpu);
void fam65xx_axs_immediate(fam65xx_t* cpu);

// DCP
void fam65xx_dcp_zero_page(fam65xx_t* cpu);
void fam65xx_dcp_zero_page_x(fam65xx_t* cpu);
void fam65xx_dcp_absolute(fam65xx_t* cpu);
void fam65xx_dcp_absolute_x(fam65xx_t* cpu);
void fam65xx_dcp_absolute_y(fam65xx_t* cpu);
void fam65xx_dcp_indirect_x(fam65xx_t* cpu);
void fam65xx_dcp_indirect_y(fam65xx_t* cpu);

// ISC
void fam65xx_isc_zero_page(fam65xx_t* cpu);
void fam65xx_isc_zero_page_x(fam65xx_t* cpu);
void fam65xx_isc_absolute(fam65xx_t* cpu);
void fam65xx_isc_absolute_x(fam65xx_t* cpu);
void fam65xx_isc_absolute_y(fam65xx_t* cpu);
void fam65xx_isc_indirect_x(fam65xx_t* cpu);
void fam65xx_isc_indirect_y(fam65xx_t* cpu);

// JAM
void fam65xx_jam(fam65xx_t* cpu);

// LAS
void fam65xx_las_absolute_y(fam65xx_t* cpu);

// LAX
void fam65xx_lax_immediate(fam65xx_t* cpu);
void fam65xx_lax_zero_page(fam65xx_t* cpu);
void fam65xx_lax_zero_page_y(fam65xx_t* cpu);
void fam65xx_lax_absolute(fam65xx_t* cpu);
void fam65xx_lax_absolute_y(fam65xx_t* cpu);
void fam65xx_lax_indirect_x(fam65xx_t* cpu);
void fam65xx_lax_indirect_y(fam65xx_t* cpu);

// ILLEGAL NOPs
void fam65xx_nop(fam65xx_t* cpu);
void fam65xx_nop_imm(fam65xx_t* cpu);
void fam65xx_nop_zp(fam65xx_t* cpu);
void fam65xx_nop_zpx(fam65xx_t* cpu);
void fam65xx_nop_zpy(fam65xx_t* cpu);
void fam65xx_nop_abs(fam65xx_t* cpu);
void fam65xx_nop_absx(fam65xx_t* cpu);
void fam65xx_nop_absy(fam65xx_t* cpu);
void fam65xx_nop_imm_special(fam65xx_t* cpu);