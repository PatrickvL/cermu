#!/usr/bin/env python3
#-------------------------------------------------------------------------------
#   fam65xx_gen.py
#   Generate instruction decoder for FAM65xx family CPU emulator.
#   Optimized for world's fastest 100% hardware accurate implementation.
#   
#   ARCHITECTURAL REDESIGN:
#   - Unified cycle sequences (no more first cycle + continuation split)
#   - Auto-generated labels based on opcode name + memory access mode
#   - Data-driven branch instructions (no hardcoded logic)
#   - Clean separation of concerns
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# Addressing Mode Definitions
# Each definition contains: (acronym, long_name, const_name, cycles_list)
# Each cycle is a string containing the cycle code
#-------------------------------------------------------------------------------

# Special addressing mode objects for non-standard cases
AM_NON = ("---", "no addressing mode", "ADDR_NON", ())   # No addressing mode
AM_JMP = ("---", "special JMP", "ADDR_NON", ())          # Special JMP cases
AM_JSR = ("---", "special JSR", "ADDR_NON", ())          # Special JSR case
AM_INV = ("---", "invalid instruction", "ADDR_NON", ())  # Invalid instruction

# Addressing mode objects for standard cases
AM_IMM = ("IMM", "immediate", "ADDR_IMM", (
    "c->AD = c->PC++;\nNEXT_OPCODE;",
))

AM_ZER = ("ZP", "zero page", "ADDR_ZER", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = c->DL;\nNEXT_OPCODE;"
))

AM_ZPX = ("ZPX", "zero page,X", "ADDR_ZPX", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = (c->DL + c->X) & 0xFF;\nNEXT_OPCODE;"
))

AM_ZPY = ("ZPY", "zero page,Y", "ADDR_ZPY", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = (c->DL + c->Y) & 0xFF;\nNEXT_OPCODE;"
))

AM_ABS = ("ABS", "absolute", "ADDR_ABS", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "c->ADL = c->TMP;\nc->ADH = c->DL;\nNEXT_OPCODE;"
))

AM_ABX = ("ABX", "absolute,X", "ADDR_ABX", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "{\n\tuint16_t sum = c->TMP + c->X;\n\tif (sum > 0xFF) {\n\t\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\t\tNEXT_OPCODE;\n\t} else {\n\t\tc->AD = sum | (c->DL << 8); NEXT_CYCLE;\n\t}\n}",
    "NEXT_OPCODE;"
))

AM_ABX_W = ("ABX", "absolute,X (write - always takes extra cycle)", "ADDR_ABX_W", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "{\n\tuint16_t sum = c->TMP + c->X;\n\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\tNEXT_OPCODE;\n}"
))

AM_ABY = ("ABY", "absolute,Y", "ADDR_ABY", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "{\n\tuint16_t sum = c->TMP + c->Y;\n\tif (sum > 0xFF) {\n\t\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\t\tNEXT_OPCODE;\n\t} else {\n\t\tc->AD = sum | (c->DL << 8);\n\t\tNEXT_CYCLE;\n\t}\n}",
    "NEXT_OPCODE;"
))

AM_ABY_W = ("ABY", "absolute,Y (write - always takes extra cycle)", "ADDR_ABY_W", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "{\n\tuint16_t sum = c->TMP + c->Y;\n\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\tNEXT_OPCODE;\n}"
))

AM_IDX = ("IDX", "indexed indirect (zp,X)", "ADDR_IDX", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = (c->DL + c->X) & 0xFF;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = (c->AD + 1) & 0xFF;\nNEXT_CYCLE;",
    "c->AD = (c->DL << 8) | c->TMP;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
))

AM_IDY = ("IDY", "indirect indexed (zp),Y", "ADDR_IDY", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = (c->DL + 1) & 0xFF;\nNEXT_CYCLE;",
    "c->AD = c->TMP | (c->DL << 8);\nif ((c->AD >> 8) != ((c->AD + c->Y) >> 8)) {\n\tNEXT_CYCLE;\n} else {\n\tc->AD += c->Y; NEXT_CYCLE;\n}",
    "c->AD += c->Y;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
))

AM_IDY_W = ("IDY", "indirect indexed (zp),Y (write - always takes extra cycle)", "ADDR_IDY_W", (
    "c->AD = c->PC++;\nNEXT_CYCLE;",
    "c->TMP = c->DL;\nc->AD = (c->DL + 1) & 0xFF;\nNEXT_CYCLE;",
    "c->AD = c->TMP | (c->DL << 8);\nNEXT_CYCLE;",
    "c->AD += c->Y;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
))

# Addressing mode list
ADDRESSING_MODES = [
    AM_IMM,
    AM_ZER,
    AM_ZPX,
    AM_ZPY,
    AM_ABS,
    AM_ABX,
    AM_ABX_W,
    AM_ABY,
    AM_ABY_W,
    AM_IDX,
    AM_IDY,
    AM_IDY_W,
]

# Memory access modes
M___ = 0        # no memory access
M_R_ = 1        # read access
M__W = 2        # write access
M_RW = 3        # read-modify-write

#-------------------------------------------------------------------------------
# Unified Operation Definitions
# Each definition contains: (mnemonic, mem_access, cycles_list)
# cycles_list is a complete list of all cycles needed for the operation
#-------------------------------------------------------------------------------

# Simple implied mode operations using NEXT_CYCLE macros
OP_BRK = ("BRK", M___, [
    "if (0 == (c->brk_flags & (FAM65XX_BRK_IRQ | FAM65XX_BRK_NMI))) {\n\tc->PC++;\n}\nc->write_src = R_PCH;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_CYCLE;",
    "c->write_src = R_PCL;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_CYCLE;",
    "c->TMP = c->P | FAM65XX_BF;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->P |= FAM65XX_IF;\nc->P &= ~FAM65XX_BF;\nc->brk_flags = 0;\nNEXT_CYCLE;",
    "c->AD = _fam65xx_get_vector_addr(c);\nc->PCL = c->DL;\nNEXT_CYCLE;",
    "c->AD++;\nc->PCH = c->DL;\nNEXT_CYCLE;",
    "c->AD = c->PC;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_PHP = ("PHP", M___, [
    "c->AD = c->PC;\nNEXT_CYCLE;",
    "c->TMP = c->P | FAM65XX_BF;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_OPCODE;"
])

OP_PLP = ("PLP", M___, [
    "c->AD = 0x0100 | c->S++;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S;\nc->P = (c->DL | FAM65XX_BF) & ~FAM65XX_XF;\nNEXT_OPCODE;"
])

OP_PHA = ("PHA", M___, [
    "c->AD = c->PC;\nNEXT_CYCLE;",
    "c->write_src = R_A;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_OPCODE;"
])

OP_PLA = ("PLA", M___, [
    "c->AD = 0x0100 | c->S++;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S;\nc->A = c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"
])

OP_RTI = ("RTI", M_R_, [
    "c->AD = 0x0100 | c->S++;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S++;\nc->P = (c->DL | FAM65XX_BF) & ~FAM65XX_XF;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S++;\nc->PCL = c->DL;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S;\nc->PCH = c->DL;\nNEXT_OPCODE;"
])

OP_RTS = ("RTS", M_R_, [
    "c->AD = 0x0100 | c->S++;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S++;\nc->PCL = c->DL;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S;\nc->PCH = c->DL;\nNEXT_OPCODE;"
])

OP_JSR = ("JSR", M_R_, [
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S;\nNEXT_CYCLE;",
    "c->write_src = R_PCH;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_CYCLE;",
    "c->write_src = R_PCL;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_CYCLE;",
    "c->AD = c->PC;\nc->PCH = c->DL;\nc->PC = (c->DL << 8) | c->TMP;\nNEXT_OPCODE;"
])

OP_JMP = ("JMP", M_R_, [
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = c->PC++;\nc->PCH = c->DL;\nc->PC = (c->DL << 8) | c->TMP;\nNEXT_OPCODE;"
])

OP_JMP_I = ("JMP", M_R_, [
    "c->TMP = c->DL;\nc->AD = c->PC++;\nNEXT_CYCLE;",
    "c->AD = c->PC++;\nc->TMP = c->DL;\nc->AD = (c->DL << 8) | c->TMP;\nNEXT_CYCLE;",
    "c->AD = c->AD;\nc->PCL = c->DL;\nNEXT_CYCLE;",
    "c->AD = (c->AD & 0xFF00) | ((c->AD + 1) & 0xFF);\nc->PCH = c->DL;\nNEXT_OPCODE;"
])

# Register transfer operations using NEXT_OPCODE macro
OP_TAX = ("TAX", M___, ["c->X = c->A;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_TXA = ("TXA", M___, ["c->A = c->X;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_TAY = ("TAY", M___, ["c->Y = c->A;\n_NZ(c->Y);\nNEXT_OPCODE;"])
OP_TYA = ("TYA", M___, ["c->A = c->Y;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_TSX = ("TSX", M___, ["c->X = c->S;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_TXS = ("TXS", M___, ["c->S = c->X;\nNEXT_OPCODE;"])

# Increment/Decrement operations using NEXT_OPCODE macro
OP_DEX = ("DEX", M___, ["c->X--;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_INX = ("INX", M___, ["c->X++;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_DEY = ("DEY", M___, ["c->Y--;\n_NZ(c->Y);\nNEXT_OPCODE;"])
OP_INY = ("INY", M___, ["c->Y++;\n_NZ(c->Y);\nNEXT_OPCODE;"])

# Flag operations using NEXT_OPCODE macro
OP_CLC = ("CLC", M___, ["c->P &= ~FAM65XX_CF;\nNEXT_OPCODE;"])
OP_SEC = ("SEC", M___, ["c->P |= FAM65XX_CF;\nNEXT_OPCODE;"])
OP_CLI = ("CLI", M___, ["c->P &= ~FAM65XX_IF;\nNEXT_OPCODE;"])
OP_SEI = ("SEI", M___, ["c->P |= FAM65XX_IF;\nNEXT_OPCODE;"])
OP_CLV = ("CLV", M___, ["c->P &= ~FAM65XX_VF;\nNEXT_OPCODE;"])
OP_CLD = ("CLD", M___, ["c->P &= ~FAM65XX_DF;\nNEXT_OPCODE;"])
OP_SED = ("SED", M___, ["c->P |= FAM65XX_DF;\nNEXT_OPCODE;"])

# Data-driven branch operations using NEXT_CYCLE and NEXT_OPCODE macros
OP_BPL = ("BPL", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_NF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BMI = ("BMI", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_NF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BVC = ("BVC", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_VF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BVS = ("BVS", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_VF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BCC = ("BCC", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_CF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BCS = ("BCS", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_CF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BNE = ("BNE", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_ZF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

OP_BEQ = ("BEQ", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_ZF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tNEXT_CYCLE;\n} else {\n\tNEXT_OPCODE;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tNEXT_OPCODE;\n} else {\n\tNEXT_CYCLE;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\nNEXT_OPCODE;"
])

# ALU operations - Memory variants using NEXT_OPCODE macro
OP_ORA = ("ORA", M_R_, ["c->A |= c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_AND = ("AND", M_R_, ["c->A &= c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_EOR = ("EOR", M_R_, ["c->A ^= c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_ADC = ("ADC", M_R_, ["_fam65xx_adc(c, c->DL);\nNEXT_OPCODE;"])
OP_SBC = ("SBC", M_R_, ["_fam65xx_sbc(c, c->DL);\nNEXT_OPCODE;"])
OP_CMP = ("CMP", M_R_, ["_fam65xx_cmp(c, c->A, c->DL);\nNEXT_OPCODE;"])
OP_BIT = ("BIT", M_R_, ["_fam65xx_bit(c, c->DL);\nNEXT_OPCODE;"])

# Immediate mode variants using NEXT_CYCLE and NEXT_OPCODE macros
OP_ORA_IMM = ("ORA", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A |= c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_AND_IMM = ("AND", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A &= c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_EOR_IMM = ("EOR", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A ^= c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_ADC_IMM = ("ADC", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\n_fam65xx_adc(c, c->DL);\nNEXT_OPCODE;"])
OP_SBC_IMM = ("SBC", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\n_fam65xx_sbc(c, c->DL);\nNEXT_OPCODE;"])
OP_CMP_IMM = ("CMP", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\n_fam65xx_cmp(c, c->A, c->DL);\nNEXT_OPCODE;"])

# Load operations using NEXT_OPCODE macro
OP_LDA = ("LDA", M_R_, ["c->A = c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_LDX = ("LDX", M_R_, ["c->X = c->DL;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_LDY = ("LDY", M_R_, ["c->Y = c->DL;\n_NZ(c->Y);\nNEXT_OPCODE;"])

# Immediate load operations using NEXT_CYCLE and NEXT_OPCODE macros
OP_LDA_IMM = ("LDA", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A = c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_LDX_IMM = ("LDX", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->X = c->DL;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_LDY_IMM = ("LDY", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->Y = c->DL;\n_NZ(c->Y);\nNEXT_OPCODE;"])

# Store operations using NEXT_OPCODE macro
OP_STA = ("STA", M__W, ["c->write_src = R_A;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])
OP_STX = ("STX", M__W, ["c->write_src = R_X;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])
OP_STY = ("STY", M__W, ["c->write_src = R_Y;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])

# Compare operations using NEXT_OPCODE macro
OP_CPX = ("CPX", M_R_, ["_fam65xx_cmp(c, c->X, c->DL);\nNEXT_OPCODE;"])
OP_CPY = ("CPY", M_R_, ["_fam65xx_cmp(c, c->Y, c->DL);\nNEXT_OPCODE;"])

# Immediate compare operations using NEXT_CYCLE and NEXT_OPCODE macros
OP_CPX_IMM = ("CPX", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\n_fam65xx_cmp(c, c->X, c->DL);\nNEXT_OPCODE;"])
OP_CPY_IMM = ("CPY", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\n_fam65xx_cmp(c, c->Y, c->DL);\nNEXT_OPCODE;"])

# RMW operations - have two variants (accumulator vs memory)
OP_ASL_A = ("ASL", M___, [
    "c->A = _fam65xx_asl(c, c->A);\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ASL_M = ("ASL", M_RW, [
    "c->TMP = _fam65xx_asl(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_LSR_A = ("LSR", M___, [
    "c->A = _fam65xx_lsr(c, c->A);\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_LSR_M = ("LSR", M_RW, [
    "c->TMP = _fam65xx_lsr(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ROL_A = ("ROL", M___, [
    "c->A = _fam65xx_rol(c, c->A);\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ROL_M = ("ROL", M_RW, [
    "c->TMP = _fam65xx_rol(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ROR_A = ("ROR", M___, [
    "c->A = _fam65xx_ror(c, c->A);\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ROR_M = ("ROR", M_RW, [
    "c->TMP = _fam65xx_ror(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_INC = ("INC", M_RW, [
    "c->TMP = c->DL + 1;\n_NZ(c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_DEC = ("DEC", M_RW, [
    "c->TMP = c->DL - 1;\n_NZ(c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

# NOP variants
OP_NOP_I = ("NOP", M___, [
    "c->AD = c->PC;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_NOP_R = ("NOP", M_R_, ["NEXT_OPCODE;"])

# Illegal/undocumented instructions using NEXT_OPCODE macro
OP_LAX = ("LAX", M_R_, ["c->A = c->X = c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_SAX = ("SAX", M__W, ["c->TMP = c->A & c->X;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])

OP_SLO = ("SLO", M_RW, [
    "c->TMP = _fam65xx_asl(c, c->DL);\nc->A |= c->TMP;\n_NZ(c->A);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_RLA = ("RLA", M_RW, [
    "c->TMP = _fam65xx_rol(c, c->DL);\nc->A &= c->TMP;\n_NZ(c->A);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_SRE = ("SRE", M_RW, [
    "c->TMP = _fam65xx_lsr(c, c->DL);\nc->A ^= c->TMP;\n_NZ(c->A);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_RRA = ("RRA", M_RW, [
    "c->TMP = _fam65xx_ror(c, c->DL);\n_fam65xx_adc(c, c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_DCP = ("DCP", M_RW, [
    "c->TMP = c->DL - 1;\n_fam65xx_cmp(c, c->A, c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ISC = ("ISC", M_RW, [
    "c->TMP = c->DL + 1;\n_fam65xx_sbc(c, c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_ANC = ("ANC", M_R_, ["c->A &= c->DL;\n_NZ(c->A);\nc->P = (c->P & ~FAM65XX_CF) | ((c->A & 0x80) ? FAM65XX_CF : 0);\nNEXT_OPCODE;"])
OP_ASR = ("ASR", M_R_, ["c->A &= c->DL;\nc->P = (c->P & ~FAM65XX_CF) | (c->A & 1);\nc->A>>=1;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_ARR = ("ARR", M_R_, ["c->A = (c->A & c->DL) >> 1 | (c->P & FAM65XX_CF ? 0x80 : 0);\n_NZ(c->A);\nc->P = (c->P & ~(FAM65XX_CF | FAM65XX_VF)) | ((c->A & 0x40) ? FAM65XX_CF : 0) | ((c->A & 0x20) ^ (c->A & 0x40) ? FAM65XX_VF : 0);\nNEXT_OPCODE;"])
OP_XAA = ("XAA", M_R_, ["c->A = (c->A | 0xEE) & c->X & c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_SBX = ("SBX", M_R_, ["c->TMP = (c->A & c->X) - c->DL;\nc->X = c->TMP;\n_NZ(c->X);\nc->P = (c->P & ~FAM65XX_CF) | ((c->TMP & 0x100) ? 0 : FAM65XX_CF);\nNEXT_OPCODE;"])

# Immediate mode illegal operations using NEXT_CYCLE and NEXT_OPCODE macros
OP_ANC_IMM = ("ANC", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A &= c->DL;\n_NZ(c->A);\nc->P = (c->P & ~FAM65XX_CF) | ((c->A & 0x80) ? FAM65XX_CF : 0);\nNEXT_OPCODE;"])
OP_ASR_IMM = ("ASR", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A &= c->DL;\nc->P = (c->P & ~FAM65XX_CF) | (c->A & 1);\nc->A>>=1;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_ARR_IMM = ("ARR", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A = (c->A & c->DL) >> 1 | (c->P & FAM65XX_CF ? 0x80 : 0);\n_NZ(c->A);\nc->P = (c->P & ~(FAM65XX_CF | FAM65XX_VF)) | ((c->A & 0x40) ? FAM65XX_CF : 0) | ((c->A & 0x20) ^ (c->A & 0x40) ? FAM65XX_VF : 0);\nNEXT_OPCODE;"])
OP_XAA_IMM = ("XAA", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A = (c->A | 0xEE) & c->X & c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_SBX_IMM = ("SBX", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->TMP = (c->A & c->X) - c->DL;\nc->X = c->TMP;\n_NZ(c->X);\nc->P = (c->P & ~FAM65XX_CF) | ((c->TMP & 0x100) ? 0 : FAM65XX_CF);\nNEXT_OPCODE;"])
OP_LAX_IMM = ("LAX", M_R_, ["c->AD = c->PC;\nNEXT_CYCLE;", "c->PC++;\nc->A = c->X = c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_SHY = ("SHY", M__W, ["c->TMP = c->Y & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])
OP_SHX = ("SHX", M__W, ["c->TMP = c->X & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])
OP_SHA = ("SHA", M_RW, ["c->TMP = c->A & c->X & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])
OP_SHS = ("SHS", M__W, ["c->S = c->A & c->X;\nc->TMP = c->S & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nNEXT_OPCODE;"])
OP_LAS = ("LAS", M_R_, ["c->A = c->X = c->S = c->S & c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])

OP_JAM = ("JAM", M_R_, [
    "c->PC--;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

# Example of new macro-based definitions for improved readability
OP_PHA_MACRO = ("PHA", M___, [
    "c->AD = c->PC;\nNEXT_CYCLE;",
    "c->write_src = R_A;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

OP_PLA_MACRO = ("PLA", M___, [
    "c->AD = 0x0100 | c->S++;\nNEXT_CYCLE;",
    "c->AD = 0x0100 | c->S;\nc->A = c->DL;\n_NZ(c->A);\nNEXT_CYCLE;",
    "NEXT_OPCODE;"
])

# Macro-based load operations (single cycle after addressing mode)
OP_LDA_MACRO = ("LDA", M_R_, ["c->A = c->DL;\n_NZ(c->A);\nNEXT_OPCODE;"])
OP_LDX_MACRO = ("LDX", M_R_, ["c->X = c->DL;\n_NZ(c->X);\nNEXT_OPCODE;"])
OP_LDY_MACRO = ("LDY", M_R_, ["c->Y = c->DL;\n_NZ(c->Y);\nNEXT_OPCODE;"])

#-------------------------------------------------------------------------------
# Instruction table: [operation, addressing_mode]
#-------------------------------------------------------------------------------
def get_hardware_accurate_addr_mode(operation, base_addr_mode):
    """Select appropriate addressing mode variant based on memory access pattern for hardware accuracy"""
    mem_access = operation[1]  # memory access is at index 1
    
    # For addressing modes that have read/write variants, choose based on memory access
    # Read operations (M_R_) use the base mode (read-optimized page boundary crossing)
    # Write/RMW operations (M__W, M_RW) use the _W variant (always takes extra cycle)
    if base_addr_mode == AM_ABX:
        return AM_ABX if mem_access == M_R_ else AM_ABX_W
    elif base_addr_mode == AM_ABY:
        return AM_ABY if mem_access == M_R_ else AM_ABY_W
    elif base_addr_mode == AM_IDY:
        return AM_IDY if mem_access == M_R_ else AM_IDY_W
    
    # For all other modes, return the original mode
    return base_addr_mode

ops = [
    # cc = 00
    [
        [[OP_BRK,AM_NON],[OP_JSR,AM_JSR],[OP_RTI,AM_NON],[OP_RTS,AM_NON],[OP_NOP_I,AM_NON],[OP_LDY,AM_IMM],[OP_CPY,AM_IMM],[OP_CPX,AM_IMM]],
        [[OP_NOP_R,AM_ZER],[OP_BIT,AM_ZER],[OP_NOP_R,AM_ZER],[OP_NOP_R,AM_ZER],[OP_STY,AM_ZER],[OP_LDY,AM_ZER],[OP_CPY,AM_ZER],[OP_CPX,AM_ZER]],
        [[OP_PHP,AM_NON],[OP_PLP,AM_NON],[OP_PHA,AM_NON],[OP_PLA,AM_NON],[OP_DEY,AM_NON],[OP_TAY,AM_NON],[OP_INY,AM_NON],[OP_INX,AM_NON]],
        [[OP_NOP_R,AM_ABS],[OP_BIT,AM_ABS],[OP_JMP,AM_JMP],[OP_JMP_I,AM_JMP],[OP_STY,AM_ABS],[OP_LDY,AM_ABS],[OP_CPY,AM_ABS],[OP_CPX,AM_ABS]],
        [[OP_BPL,AM_NON],[OP_BMI,AM_NON],[OP_BVC,AM_NON],[OP_BVS,AM_NON],[OP_BCC,AM_NON],[OP_BCS,AM_NON],[OP_BNE,AM_NON],[OP_BEQ,AM_NON]],
        [[OP_NOP_R,AM_ZPX],[OP_NOP_R,AM_ZPX],[OP_NOP_R,AM_ZPX],[OP_NOP_R,AM_ZPX],[OP_STY,AM_ZPX],[OP_LDY,AM_ZPX],[OP_NOP_R,AM_ZPX],[OP_NOP_R,AM_ZPX]],
        [[OP_CLC,AM_NON],[OP_SEC,AM_NON],[OP_CLI,AM_NON],[OP_SEI,AM_NON],[OP_TYA,AM_NON],[OP_CLV,AM_NON],[OP_CLD,AM_NON],[OP_SED,AM_NON]],
        [[OP_NOP_R,AM_ABX],[OP_NOP_R,AM_ABX],[OP_NOP_R,AM_ABX],[OP_NOP_R,AM_ABX],[OP_SHY,AM_ABX],[OP_LDY,AM_ABX],[OP_NOP_R,AM_ABX],[OP_NOP_R,AM_ABX]]
    ],
    # cc = 01
    [
        [[OP_ORA,AM_IDX],[OP_AND,AM_IDX],[OP_EOR,AM_IDX],[OP_ADC,AM_IDX],[OP_STA,AM_IDX],[OP_LDA,AM_IDX],[OP_CMP,AM_IDX],[OP_SBC,AM_IDX]],
        [[OP_ORA,AM_ZER],[OP_AND,AM_ZER],[OP_EOR,AM_ZER],[OP_ADC,AM_ZER],[OP_STA,AM_ZER],[OP_LDA,AM_ZER],[OP_CMP,AM_ZER],[OP_SBC,AM_ZER]],
        [[OP_ORA,AM_IMM],[OP_AND,AM_IMM],[OP_EOR,AM_IMM],[OP_ADC,AM_IMM],[OP_NOP_I,AM_NON],[OP_LDA,AM_IMM],[OP_CMP,AM_IMM],[OP_SBC,AM_IMM]],
        [[OP_ORA,AM_ABS],[OP_AND,AM_ABS],[OP_EOR,AM_ABS],[OP_ADC,AM_ABS],[OP_STA,AM_ABS],[OP_LDA,AM_ABS],[OP_CMP,AM_ABS],[OP_SBC,AM_ABS]],
        [[OP_ORA,AM_IDY],[OP_AND,AM_IDY],[OP_EOR,AM_IDY],[OP_ADC,AM_IDY],[OP_STA,AM_IDY],[OP_LDA,AM_IDY],[OP_CMP,AM_IDY],[OP_SBC,AM_IDY]],
        [[OP_ORA,AM_ZPX],[OP_AND,AM_ZPX],[OP_EOR,AM_ZPX],[OP_ADC,AM_ZPX],[OP_STA,AM_ZPX],[OP_LDA,AM_ZPX],[OP_CMP,AM_ZPX],[OP_SBC,AM_ZPX]],
        [[OP_ORA,AM_ABY],[OP_AND,AM_ABY],[OP_EOR,AM_ABY],[OP_ADC,AM_ABY],[OP_STA,AM_ABY],[OP_LDA,AM_ABY],[OP_CMP,AM_ABY],[OP_SBC,AM_ABY]],
        [[OP_ORA,AM_ABX],[OP_AND,AM_ABX],[OP_EOR,AM_ABX],[OP_ADC,AM_ABX],[OP_STA,AM_ABX],[OP_LDA,AM_ABX],[OP_CMP,AM_ABX],[OP_SBC,AM_ABX]]
    ],
    # cc = 02
    [
        [[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_NOP_I,AM_NON],[OP_LDX,AM_IMM],[OP_NOP_I,AM_NON],[OP_NOP_I,AM_NON]],
        [[OP_ASL_M,AM_ZER],[OP_ROL_M,AM_ZER],[OP_LSR_M,AM_ZER],[OP_ROR_M,AM_ZER],[OP_STX,AM_ZER],[OP_LDX,AM_ZER],[OP_DEC,AM_ZER],[OP_INC,AM_ZER]],
        [[OP_ASL_A,AM_NON],[OP_ROL_A,AM_NON],[OP_LSR_A,AM_NON],[OP_ROR_A,AM_NON],[OP_TXA,AM_NON],[OP_TAX,AM_NON],[OP_DEX,AM_NON],[OP_NOP_I,AM_NON]],
        [[OP_ASL_M,AM_ABS],[OP_ROL_M,AM_ABS],[OP_LSR_M,AM_ABS],[OP_ROR_M,AM_ABS],[OP_STX,AM_ABS],[OP_LDX,AM_ABS],[OP_DEC,AM_ABS],[OP_INC,AM_ABS]],
        [[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV]],
        [[OP_ASL_M,AM_ZPX],[OP_ROL_M,AM_ZPX],[OP_LSR_M,AM_ZPX],[OP_ROR_M,AM_ZPX],[OP_STX,AM_ZPY],[OP_LDX,AM_ZPY],[OP_DEC,AM_ZPX],[OP_INC,AM_ZPX]],
        [[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON],[OP_TXS,AM_NON],[OP_TSX,AM_NON],[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON]],
        [[OP_ASL_M,AM_ABX],[OP_ROL_M,AM_ABX],[OP_LSR_M,AM_ABX],[OP_ROR_M,AM_ABX],[OP_SHX,AM_ABY],[OP_LDX,AM_ABY],[OP_DEC,AM_ABX],[OP_INC,AM_ABX]]
    ],
    # cc = 03
    [
        [[OP_SLO,AM_IDX],[OP_RLA,AM_IDX],[OP_SRE,AM_IDX],[OP_RRA,AM_IDX],[OP_SAX,AM_IDX],[OP_LAX,AM_IDX],[OP_DCP,AM_IDX],[OP_ISC,AM_IDX]],
        [[OP_SLO,AM_ZER],[OP_RLA,AM_ZER],[OP_SRE,AM_ZER],[OP_RRA,AM_ZER],[OP_SAX,AM_ZER],[OP_LAX,AM_ZER],[OP_DCP,AM_ZER],[OP_ISC,AM_ZER]],
        [[OP_ANC,AM_IMM],[OP_ANC,AM_IMM],[OP_ASR,AM_IMM],[OP_ARR,AM_IMM],[OP_XAA,AM_IMM],[OP_LAX,AM_IMM],[OP_SBX,AM_IMM],[OP_SBC,AM_IMM]],
        [[OP_SLO,AM_ABS],[OP_RLA,AM_ABS],[OP_SRE,AM_ABS],[OP_RRA,AM_ABS],[OP_SAX,AM_ABS],[OP_LAX,AM_ABS],[OP_DCP,AM_ABS],[OP_ISC,AM_ABS]],
        [[OP_SLO,AM_IDY],[OP_RLA,AM_IDY],[OP_SRE,AM_IDY],[OP_RRA,AM_IDY],[OP_SHA,AM_IDY],[OP_LAX,AM_IDY],[OP_DCP,AM_IDY],[OP_ISC,AM_IDY]],
        [[OP_SLO,AM_ZPX],[OP_RLA,AM_ZPX],[OP_SRE,AM_ZPX],[OP_RRA,AM_ZPX],[OP_SAX,AM_ZPY],[OP_LAX,AM_ZPY],[OP_DCP,AM_ZPX],[OP_ISC,AM_ZPX]],
        [[OP_SLO,AM_ABY],[OP_RLA,AM_ABY],[OP_SRE,AM_ABY],[OP_RRA,AM_ABY],[OP_SHS,AM_ABY],[OP_LAS,AM_ABY],[OP_DCP,AM_ABY],[OP_ISC,AM_ABY]],
        [[OP_SLO,AM_ABX],[OP_RLA,AM_ABX],[OP_SRE,AM_ABX],[OP_RRA,AM_ABX],[OP_SHY,AM_ABY],[OP_LAX,AM_ABY],[OP_DCP,AM_ABX],[OP_ISC,AM_ABX]]
    ]
]

# Layout constants
NO_ADDR_SEQ = 0             # Direct opcode jump (no addressing mode)
ADDR_SEQ_BASE = 255         # Addressing modes start at 256

# Global state
continuation_sequences = {}  # label -> {index, cycles}
next_continuation_index = 0  # Will be set to CONT_SEQ_START in main()
opcode_groups = {}  # implementation_code -> [opcodes]

# New: Suffix optimization state
suffix_sequences = {}  # suffix_tuple -> {label, cycles, sources}
suffix_labels = {}     # label -> index (for constant generation)
SUFFIX_SEQ_START = 0   # Will be calculated after addressing modes

# Output arrays for clean emission
output_constants = []
output_lookup_table = []
output_opcode_cycles = []
output_cases_addrmode = []
output_shared_sequences = []
output_cases_continues = []

# Simplified dynamic constant tracking
label_to_index = {}      # Maps label names to their case indices
current_case_index = 256 # Start after opcodes (0-255)

# Shared fetch_next case
SHARED_FETCH_NEXT = "SHARED_FETCH_NEXT"

#-------------------------------------------------------------------------------
# Macro Support Functions
#-------------------------------------------------------------------------------

def expand_macro(code, context):
    """Expand NEXT_CYCLE and NEXT_OPCODE macros based on context"""
    if "NEXT_CYCLE" in code:
        # Replace NEXT_CYCLE with c->CI++
        code = code.replace("NEXT_CYCLE", "c->CI++")
    
    if "NEXT_OPCODE" in code:
        # Replace NEXT_OPCODE with c->CI = c->opcode (for addressing modes) or goto fetch_next (for operations)
        # Check context to determine which replacement to use
        if hasattr(context, 'get') and context.get('is_addressing_mode', False):
            code = code.replace("NEXT_OPCODE", "c->CI = c->opcode")
        else:
            code = code.replace("NEXT_OPCODE", "goto fetch_next")
    
    return code


def find_common_suffixes(all_cycles):
    """Find common suffix sequences across all cycle definitions"""
    suffixes = {}  # tuple_of_cycles -> [sources]
    
    for source, cycles in all_cycles.items():
        # Check all possible suffixes of this cycle sequence
        for i in range(1, len(cycles) + 1):
            suffix = tuple(cycles[-i:])  # Last i cycles as tuple
            if suffix not in suffixes:
                suffixes[suffix] = []
            suffixes[suffix].append(source)
    
    # Only keep suffixes that are shared by multiple sources
    common_suffixes = {suffix: sources for suffix, sources in suffixes.items()
                      if len(sources) > 1 and len(suffix) > 0}
    
    return common_suffixes

def generate_suffix_label(suffix_cycles):
    """Generate a label name for a common suffix"""
    # Create a descriptive label based on the suffix content
    last_cycle = suffix_cycles[-1]
    suffix_len = len(suffix_cycles)
    
    # Generate unique hash for this specific suffix
    import hashlib
    suffix_hash = hashlib.md5(''.join(suffix_cycles).encode()).hexdigest()[:4]
    
    if "goto fetch_next" in last_cycle:
        if suffix_len == 1:
            return f"S_FETCH_{suffix_hash.upper()}"
        else:
            return f"S_FETCH_{suffix_len}_{suffix_hash.upper()}"
    elif "c->PC = c->AD" in last_cycle and "irq_pip" in last_cycle:
        return f"S_BRANCH_{suffix_len}_{suffix_hash.upper()}"
    else:
        # Generic label with length and hash for uniqueness
        return f"S_CONT_{suffix_len}_{suffix_hash.upper()}"

def collect_opcode_cycles_only():
    """Collect cycle sequences ONLY from opcode operations for suffix analysis"""
    all_cycles = {}
    
    # Only collect from operations (not addressing modes)
    for cc in range(len(ops)):
        for bbb in range(len(ops[cc])):
            for aaa in range(len(ops[cc][bbb])):
                operation, _ = ops[cc][bbb][aaa]
                if len(operation[2]) > 1:  # Multi-cycle operation
                    # Use operation name + memory access as key for uniqueness
                    mem_suffix = {M___: "", M_R_: "_R", M__W: "_W", M_RW: "_RW"}[operation[1]]
                    key = f"OP_{operation[0]}{mem_suffix}"
                    if key not in all_cycles:  # Avoid duplicates
                        # DON'T EXPAND MACROS - keep original macro form for suffix analysis
                        all_cycles[key] = list(operation[2])  # Keep original cycles with macros
    
    return all_cycles

def analyze_and_create_suffix_sequences():
    """Analyze OPCODE cycles only and create optimized suffix sequences"""
    global suffix_sequences, suffix_labels, SUFFIX_SEQ_START
    
    # Collect cycle sequences from opcodes only (not addressing modes)
    all_cycles = collect_opcode_cycles_only()
    
    # Find common suffixes
    common_suffixes = find_common_suffixes(all_cycles)
    
    # Create suffix sequences for suffixes longer than 1 cycle and used by multiple sources
    suffix_sequences = {}
    for suffix_tuple, sources in common_suffixes.items():
        if len(suffix_tuple) > 1 and len(sources) > 1:
            label = generate_suffix_label(suffix_tuple)
            suffix_sequences[suffix_tuple] = {
                'label': label,
                'cycles': list(suffix_tuple),
                'sources': sources
            }
    
    # Build suffix_labels mapping for constant generation
    suffix_labels = {info['label']: 0 for info in suffix_sequences.values()}
    
    return suffix_sequences

def optimize_cycles_with_suffixes(cycles, suffix_sequences):
    """Replace cycle suffixes with jumps to shared suffix sequences"""
    optimized = list(cycles)  # Start with original cycles
    
    # Special case: Check for NEXT_CYCLE followed by NEXT_OPCODE pattern
    if len(optimized) >= 2 and "NEXT_CYCLE" in optimized[-2] and "NEXT_OPCODE" in optimized[-1]:
        # If the last cycle is just NEXT_OPCODE (which expands to goto fetch_next),
        # optimize by replacing NEXT_CYCLE with c->CI = SHARED_FETCH_NEXT
        expanded_last = expand_macro(optimized[-1], {})
        if expanded_last.strip() == "goto fetch_next;":
            # Replace the NEXT_CYCLE in the second-to-last cycle with jump to SHARED_FETCH_NEXT
            second_to_last = optimized[-2]
            if "NEXT_CYCLE" in second_to_last:
                optimized[-2] = second_to_last.replace("NEXT_CYCLE", f"c->CI = {SHARED_FETCH_NEXT}")
            # Remove the last cycle since it's now handled by SHARED_FETCH_NEXT
                optimized = optimized[:-1]
                return optimized
    
    # Apply suffix optimization: Check if the end of this cycle sequence matches any suffix
    if suffix_sequences:
        for suffix_tuple, suffix_info in suffix_sequences.items():
            suffix_len = len(suffix_tuple)
            # Check if this cycle sequence ends with this suffix
            if len(optimized) >= suffix_len:
                # Compare the last suffix_len cycles
                cycle_suffix = tuple(optimized[-suffix_len:])
                if cycle_suffix == suffix_tuple:
                    # Replace the suffix with a jump to the shared sequence
                    suffix_label = suffix_info['label']
                    # Remove the suffix cycles
                    optimized = optimized[:-suffix_len]
                    # Add a jump to the suffix label instead
                    if optimized:
                        # Replace NEXT_CYCLE in the last remaining cycle with the jump
                        last_cycle = optimized[-1]
                        if "NEXT_CYCLE" in last_cycle:
                            optimized[-1] = last_cycle.replace("NEXT_CYCLE", f"c->CI = {suffix_label}")
                        elif "c->CI++" in last_cycle:
                            optimized[-1] = last_cycle.replace("c->CI++", f"c->CI = {suffix_label}")
                        else:
                            # Add a new cycle that jumps to the suffix
                            optimized.append(f"c->CI = {suffix_label};")
                    else:
                        # This entire sequence is a suffix, replace with a single jump
                        optimized = [f"c->CI = {suffix_label};"]
                    break  # Only apply one suffix optimization per sequence
    
    return optimized

def assign_label_index(label, length=1):
    """Assign the next available index to a label and advance the counter"""
    global current_case_index
    if label not in label_to_index:
        label_to_index[label] = current_case_index
        current_case_index += length
    return label_to_index[label]

def generate_shared_fetch_next():
    """Generate the final shared fetch_next case"""
    assign_label_index(SHARED_FETCH_NEXT, 1)
    
    output_shared_sequences.append(f"        // {SHARED_FETCH_NEXT}: Universal fetch next opcode")
    output_shared_sequences.append(f"        case {SHARED_FETCH_NEXT}:")
    output_shared_sequences.append("            goto fetch_next;")
    output_shared_sequences.append("")

# Helper functions for direct ops array access
def get_ops_entry(op):
    """Get the full ops entry for an opcode with hardware-accurate addressing mode selection"""
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    operation, base_addr_mode = ops[cc][bbb][aaa]
    
    # Select hardware-accurate addressing mode variant based on memory access
    addr_mode = get_hardware_accurate_addr_mode(operation, base_addr_mode)
    
    return operation, addr_mode

def get_or_create_continuation(cycles, label):
    """Get existing continuation sequence index or create new one"""
    global next_continuation_index
    
    # Use label as key to avoid conflicts between different instructions
    if label not in continuation_sequences:
        continuation_sequences[label] = {
            'index': next_continuation_index,
            'cycles': cycles
        }
        next_continuation_index += len(cycles)
    
    return continuation_sequences[label]['index']

def generate_label(mnemonic, mem_access, addressing_mode=None):
    """Generate unique label for operation cycle sequence"""
    mem_suffix = {M___: "", M_R_: "_R", M__W: "_W", M_RW: "_RW"}[mem_access]
    # Only add addressing mode suffix if it's a real addressing mode (not AM_NON, AM_JMP, AM_JSR, AM_INV)
    addr_suffix = ""
    if addressing_mode and addressing_mode not in [AM_NON, AM_JMP, AM_JSR, AM_INV]:
        # Use the const name instead of acronym to avoid invalid C identifiers
        addr_suffix = f"_{addressing_mode[2].replace('ADDR_', '')}"
    return f"C_{mnemonic}{mem_suffix}{addr_suffix}"

def analyze_continuation_needs(op):
    """Determine if opcode needs a continuation sequence"""
    operation, addr_mode = get_ops_entry(op)
    
    # All operations now have unified cycle lists
    cycles = operation[2]  # cycles are at index 2
    
    # Special case: Check if this is a two-cycle operation where the second cycle is just NEXT_OPCODE
    if len(cycles) == 2:
        second_cycle = cycles[1]
        expanded_second = expand_macro(second_cycle, {})
        if expanded_second.strip() == "goto fetch_next;":
            # This will be optimized to c->CI = SHARED_FETCH_NEXT, no continuation needed
            return None
    
    # If operation has more than 1 cycle, create continuation sequence
    if len(cycles) > 1:
        # Generate unique label for this operation + addressing mode combination
        label = generate_label(operation[0], operation[1], addr_mode if addr_mode != AM_NON else None)
        get_or_create_continuation(cycles[1:], label)  # Skip first cycle
        return label
    
    return None

def generate_opcode_implementation(op):
    """Generate the implementation code for this opcode's first cycle only"""
    operation, addr_mode = get_ops_entry(op)
    cycles = operation[2]  # cycles are at index 2
    
    # Always return the first cycle
    first_cycle = cycles[0]
    
    # Special case: Check if this is a two-cycle operation where the second cycle is just NEXT_OPCODE
    if len(cycles) == 2:
        second_cycle = cycles[1]
        expanded_second = expand_macro(second_cycle, {})
        if expanded_second.strip() == "goto fetch_next;":
            # This is the pattern: NEXT_CYCLE + NEXT_OPCODE -> optimize to c->CI = SHARED_FETCH_NEXT
            first_cycle = first_cycle.replace("NEXT_CYCLE", f"c->CI = {SHARED_FETCH_NEXT}")
            first_cycle = first_cycle.replace("c->CI++", f"c->CI = {SHARED_FETCH_NEXT}")
            return first_cycle
    
    # If there are more cycles, replace NEXT_CYCLE and c->CI++ with c->CI = label
    if len(cycles) > 1:
        label = generate_label(operation[0], operation[1], addr_mode if addr_mode != AM_NON else None)
        # Replace NEXT_CYCLE macro with c->CI = label
        first_cycle = first_cycle.replace("NEXT_CYCLE", f"c->CI = {label}")
        # Also replace any remaining c->CI++ with c->CI = label
        first_cycle = first_cycle.replace("c->CI++", f"c->CI = {label}")
    else:
        # Single cycle - expand macros and handle appropriately
        expanded_first = expand_macro(first_cycle, {})
        if expanded_first.strip() == "goto fetch_next;":
            # Replace NEXT_OPCODE/goto fetch_next with jump to SHARED_FETCH_NEXT
            first_cycle = f"c->CI = {SHARED_FETCH_NEXT};"
        # For single cycles that are NOT goto fetch_next, don't apply suffix optimization
        # This prevents creating redundant S_FETCH_* labels for simple operations
        # The original cycle implementation should be used as-is
    
    return first_cycle

def format_code(code):
    """Format code using embedded newlines and tabs for indentation"""
    lines = code.split('\n')
    formatted_lines = []
    
    for line in lines:
        line = line.rstrip()  # Remove trailing whitespace but preserve leading tabs
        if line:  # Only add non-empty lines
            formatted_lines.append('            ' + line)
    
    return formatted_lines

def generate_addressing_modes():
    """Generate shared addressing mode sequences and assign their indices"""
    
    # Generate addressing mode sequences from definitions
    for addr_mode in ADDRESSING_MODES:
        acronym, long_name, const_name, cycles = addr_mode
        
        # Skip modes with no cycles (they execute directly in opcode case)
        if len(cycles) == 0:
            continue
        
        # Assign index for this addressing mode label
        assign_label_index(const_name, len(cycles))
        
        # Emit long name comment before the cycles
        output_cases_addrmode.append(f"        // {acronym}: {long_name}")
        
        for cycle_idx, cycle_code in enumerate(cycles):
            cycle_num = cycle_idx + 1
            
            output_cases_addrmode.append(f"        case {const_name} + {cycle_idx}:  // {acronym} cycle {cycle_num}")
            
            # Expand macros and format the code with proper indentation
            # Pass addressing mode context for proper NEXT_OPCODE expansion
            context = {'is_addressing_mode': True}
            expanded_code = expand_macro(cycle_code, context)
            lines = format_code(expanded_code)
            for line in lines:
                output_cases_addrmode.append(line)
            
            # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
            if not expanded_code.strip().endswith('goto fetch_next;'):
                output_cases_addrmode.append("            break;")
            
        output_cases_addrmode.append("")

def generate_continuations():
    """Generate shared multi-cycle continuation sequences and assign their indices"""
    if not continuation_sequences:
        return
        
    # Sort by original index to maintain order
    sorted_seqs = sorted(continuation_sequences.items(),
                        key=lambda x: x[1]['index'])
    
    for label, seq_info in sorted_seqs:
        cycles = seq_info['cycles']
        
        # Apply suffix optimization to the continuation cycles
        optimized_cycles = optimize_cycles_with_suffixes(cycles, suffix_sequences)
        
        # Assign index for this label AFTER optimization to get correct length
        assign_label_index(label, len(optimized_cycles))
        
        # Only emit if we have remaining cycles after optimization
        if optimized_cycles:
            output_cases_continues.append(f"        // {label} continuation")
            for i, cycle_code in enumerate(optimized_cycles):
                output_cases_continues.append(f"        case {label} + {i}:")
                
                # Format the expanded code with proper indentation
                expanded_code = expand_macro(cycle_code, {})
                lines = format_code(expanded_code)
                for line in lines:
                    output_cases_continues.append(line)
                
                # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
                if not expanded_code.strip().endswith('goto fetch_next;'):
                    output_cases_continues.append("            break;")
            output_cases_continues.append("")

def generate_suffix_sequences():
    """Generate shared suffix sequence cases and assign their indices"""
    if not suffix_sequences:
        return
        
    # Sort by label for consistent ordering
    for suffix_tuple, suffix_info in sorted(suffix_sequences.items(),
                                          key=lambda x: x[1]['label']):
        label = suffix_info['label']
        cycles = suffix_info['cycles']
        sources = suffix_info['sources']
        
        # Check if this suffix is just a single "goto fetch_next" - skip it since SHARED_FETCH_NEXT handles this
        if len(cycles) == 1:
            expanded_first = expand_macro(cycles[0], {})
            if expanded_first.strip() == "goto fetch_next;":
                # Skip this suffix - operations should use SHARED_FETCH_NEXT instead
                continue
        
        # Check if this suffix is actually used by any continuation sequence
        # We need to check if optimize_cycles_with_suffixes would create jumps to this suffix
        suffix_actually_used = False
        for cont_label, cont_info in continuation_sequences.items():
            # Apply the same optimization logic to see if it would create a jump
            original_cycles = cont_info['cycles']
            optimized_cycles = optimize_cycles_with_suffixes(original_cycles, {suffix_tuple: suffix_info})
            
            # Check if the optimization created a jump to this suffix label
            for cycle in optimized_cycles:
                if suffix_info['label'] in cycle:
                    suffix_actually_used = True
                    break
            if suffix_actually_used:
                break
        
        if not suffix_actually_used:
            # This suffix is not actually used by any continuation sequence
            continue
        
        # Assign index for this label
        assign_label_index(label, len(cycles))
        
        # Optimize cycles: if last cycle is only "goto fetch_next", merge it with previous cycle
        optimized_cycles = []
        for i, cycle_code in enumerate(cycles):
            expanded_code = expand_macro(cycle_code, {})
            
            # Check if this is the last cycle and it's only "goto fetch_next"
            if i == len(cycles) - 1 and expanded_code.strip() == "goto fetch_next;":
                # If we have a previous cycle, modify it to use goto fetch_next instead of incrementing
                if optimized_cycles:
                    prev_cycle = optimized_cycles[-1]
                    # Replace NEXT_CYCLE or c->CI++ with goto fetch_next
                    if "NEXT_CYCLE" in prev_cycle:
                        optimized_cycles[-1] = prev_cycle.replace("NEXT_CYCLE", "goto fetch_next")
                    elif "c->CI++" in prev_cycle:
                        optimized_cycles[-1] = prev_cycle.replace("c->CI++", "goto fetch_next")
                    else:
                        # Add goto fetch_next at the end of the previous cycle
                        optimized_cycles[-1] = prev_cycle + ";\ngoto fetch_next"
                # Skip this cycle since it's been merged
                continue
            else:
                optimized_cycles.append(cycle_code)
        
        # Only emit if we have remaining cycles after optimization
        if optimized_cycles:
            output_shared_sequences.append(f"        // {label}: {len(optimized_cycles)} cycles, used by {len(sources)} sources")
            output_shared_sequences.append(f"        // Sources: {', '.join(sources[:3])}{'...' if len(sources) > 3 else ''}")
            
            for i, cycle_code in enumerate(optimized_cycles):
                output_shared_sequences.append(f"        case {label} + {i}:")
                
                # Format the expanded code with proper indentation
                expanded_code = expand_macro(cycle_code, {})
                lines = format_code(expanded_code)
                for line in lines:
                    output_shared_sequences.append(line)
                
                # Only emit break if the code doesn't end with goto fetch_next
                if not expanded_code.strip().endswith('goto fetch_next;'):
                    output_shared_sequences.append("            break;")
            
            output_shared_sequences.append("")

def generate_all_constants():
    """Generate all constant declarations from the collected labels"""
    # Generate layout constants
    output_constants.append("#define ADDR_NON      0   // Direct opcode execution (no addressing mode)")
    output_constants.append(f"#define ADDR_SEQ_BASE {ADDR_SEQ_BASE}")
    
    # Generate constants for addressing modes with base corrections
    addr_labels = [label for label in label_to_index.keys() if label.startswith('ADDR_')]
    if addr_labels:
        output_constants.append("")
        for addr_mode in ADDRESSING_MODES:
            const_name = addr_mode[2]
            if const_name in label_to_index:
                actual_index = label_to_index[const_name]
                output_constants.append(f"#define {const_name:<18} {actual_index:<3}")
            elif len(addr_mode[3]) == 0:
                # Modes with no cycles use ADDR_NON (0)
                output_constants.append(f"#define {const_name:<18} 0   // Direct opcode execution (no addressing mode)")
    
    output_constants.append("")
    
    # Generate continuation sequence constants
    continuation_labels = [label for label in label_to_index.keys() if label.startswith('C_')]
    if continuation_labels:
        output_constants.append("// Continuation sequence constants")
        # Sort by index value instead of alphabetically
        for label in sorted(continuation_labels, key=lambda x: label_to_index[x]):
            index = label_to_index[label]
            output_constants.append(f"#define {label:<18} {index}")
        output_constants.append("")
    
    # Generate shared sequence constants
    shared_labels = [label for label in label_to_index.keys() if label.startswith('S_') or label == SHARED_FETCH_NEXT]
    if shared_labels:
        output_constants.append("// Shared sequence constants")
        # Sort by index value instead of alphabetically
        for label in sorted(shared_labels, key=lambda x: label_to_index[x]):
            index = label_to_index[label]
            output_constants.append(f"#define {label:<18} {index}")
        output_constants.append("")

def generate_lookup_table():
    """Generate opcode_addr_start lookup table with base-corrected addressing mode offsets"""
    for op in range(256):
        operation, addr_mode = get_ops_entry(op)
        
        # Get the addressing mode label and check if it has cycles
        if len(addr_mode[3]) > 0:  # Has cycles - needs base correction
            # Calculate base-corrected offset (subtract ADDR_SEQ_BASE for lookup table)
            const_name = f"{addr_mode[2]:<10} - ADDR_SEQ_BASE"  # Use the const name like "ADDR_IMM"
        else:
            # No cycles - use ADDR_NON
            const_name = "ADDR_NON"
        
        # Format with comma except for last element
        comma = "," if op < 255 else " "
        const_comma = f"{const_name}{comma}"

        # Show memory access type in comment for clarity
        mem_access_str = {M___: "---", M_R_: "R", M__W: "W", M_RW: "RW"}[operation[1]]
        output_lookup_table.append(f"    {const_comma:<27}  // 0x{op:02X}: {operation[0]} [{mem_access_str}] {addr_mode[1]}")
    
def generate_opcode_cycles():
    """Generate opcode-specific cases with fallthrough optimization"""
    global opcode_groups
    
    # Group opcodes by implementation
    opcode_groups = {}
    for op in range(256):
        code = generate_opcode_implementation(op)
        if code not in opcode_groups:
            opcode_groups[code] = []
        opcode_groups[code].append(op)
    
    # Emit grouped cases
    emitted = set()
    for op in range(256):
        if op in emitted:
            continue
        
        code = generate_opcode_implementation(op)
        opcodes = opcode_groups[code]
        
        # Emit all opcodes that share this implementation
        for opc in sorted(opcodes):
            operation, addr_mode = get_ops_entry(opc)
            addr_acronym = addr_mode[0]
            cycle_num = len(addr_mode[3]) + 1
            mem_access_str = {M___: "---", M_R_: "R", M__W: "W", M_RW: "RW"}[operation[1]]
            output_opcode_cycles.append(f"        case 0x{opc:02X}:  // {operation[0]} [{mem_access_str}] {addr_acronym} cycle {cycle_num}")
            emitted.add(opc)
        
        # Check if this should be replaced with jump to shared fetch_next
        expanded_code = expand_macro(code, {})
        if expanded_code.strip() == "goto fetch_next;":
            output_opcode_cycles.append(f"            c->CI = {SHARED_FETCH_NEXT};")
            output_opcode_cycles.append("            break;")
        else:
            # Format the expanded code with proper indentation - don't try to optimize with suffix sequences here
            # All single-cycle "goto fetch_next" operations should use SHARED_FETCH_NEXT directly
            lines = format_code(expanded_code)
            for line in lines:
                output_opcode_cycles.append(line)
            
            # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
            if not expanded_code.strip().endswith('goto fetch_next;'):
                output_opcode_cycles.append("            break;")
        output_opcode_cycles.append("")

def l(s):
    """Output a line"""
    print(s)

def write_decoder_file():
    """Write the complete decoder file with all generated content"""
    # Header
    l("/*")
    l(" * AUTO-GENERATED by fam65xx_gen.py")
    l(" * 65xx Family CPU Decoder")
    l(" * Optimized for maximum performance and 100% hardware accuracy")
    l(f" * ")
    l(f" * Layout:")
    
    # Find addressing mode range
    addr_indices = [idx for label, idx in label_to_index.items() if label.startswith('ADDR_')]
    if addr_indices:
        addr_start = min(addr_indices)
        addr_end = max(addr_indices) + max(len(addr_mode[3]) for addr_mode in ADDRESSING_MODES if len(addr_mode[3]) > 0) - 1
        addr_range = f"[{addr_start}-{addr_end}]"
    else:
        addr_range = "[256]"
    
    # Find the range of shared sequences (continuations + shared)
    shared_indices = [idx for label, idx in label_to_index.items() if not label.startswith('ADDR_')]
    if shared_indices:
        shared_start = min(shared_indices)
        shared_end = current_case_index - 1
        shared_range = f"[{shared_start}-{shared_end}]"
    else:
        shared_range = f"[{current_case_index}]"
    
    l(f" *   [0-255]     : Opcode-specific cycles")
    l(f" *   {addr_range:<11} : Shared addressing mode sequences")
    l(f" *   {shared_range:<11} : Shared sequences")
    l(f" * Total cases   : {current_case_index}")
    l(" */")
    l("")
    
    # Constants
    l("// Layout constants")
    l("// [0-255]   : Opcode-specific cycles")
    l("// [256+]    : Shared addressing mode sequences")
    for line in output_constants:
        l(line)

    # Lookup table
    l("// Lookup table: addressing mode start index for each opcode")
    l("const uint8_t opcode_addr_start[256] = {")
    for line in output_lookup_table:
        l(line)
    l("};")
    l("")
    
    # Decoder function
    l("// Decoder switch statement")
    l("static inline uint64_t _fam65xx_decode(fam65xx_t* c, uint64_t pins) {")
    l("    switch (c->CI) {")
    l("")
    
    # Opcode cases
    l("        // ==========================================")
    l("        // [0-255] OPCODE-SPECIFIC CYCLES")
    l("        // ==========================================")
    l("")
    for line in output_opcode_cycles:
        l(line)

    # Addressing mode cases
    addr_indices = [idx for label, idx in label_to_index.items() if label.startswith('ADDR_')]
    if addr_indices:
        addr_start = min(addr_indices)
        addr_end = max(addr_indices) + max(len(addr_mode[3]) for addr_mode in ADDRESSING_MODES if len(addr_mode[3]) > 0) - 1
        l("        // ==========================================")
        l(f"        // [{addr_start}-{addr_end}] SHARED ADDRESSING SEQUENCES")
        l("        // ==========================================")
    else:
        l("        // ==========================================")
        l("        // [256] SHARED ADDRESSING SEQUENCES")
        l("        // ==========================================")
    l("")
    for line in output_cases_addrmode:
        l(line)
    
    # Continuation cases
    if output_cases_continues:
        l("        // ==========================================")
        l("        // SHARED CONTINUATIONS")
        l("        // ==========================================")
        l("")
        for line in output_cases_continues:
            l(line)
    
    # Shared sequence cases
    if output_shared_sequences:
        l("        // ==========================================")
        l("        // SHARED SEQUENCES")
        l("        // ==========================================")
        l("")
        for line in output_shared_sequences:
            l(line)

    # Footer
    l("    default:")
    l("        break;")
    l("    }")
    l("    return pins;")
    l("")
    l("fetch_next:")
    l("    c->AD = c->PC++;")
    l("    pins |= FAM65XX_SYNC;")
    l("    return pins;")
    l("}")

def analyze_opcodes():
    """Analyze opcodes and collect continuation/suffix sequences"""
    # Analyze suffix sequences for optimization (opcode cycles only)
    analyze_and_create_suffix_sequences()
    
    # First pass: analyze all opcodes to discover continuation sequences
    for op in range(256):
        analyze_continuation_needs(op)

def main():
    """Main function to generate the decoder file with streamlined generation order"""
    global output_constants, output_lookup_table, output_opcode_cycles
    global output_cases_addrmode, output_shared_sequences, output_cases_continues
    global current_case_index
    
    # Step 1: Analyze opcodes and collect sequences (but don't generate constants yet)
    analyze_opcodes()
    # Step 2: Generate cases in optimal order, building label map as we go
    # 2a. Generate addressing modes first (start at 256, no labels needed)
    generate_addressing_modes()
    # 2b. Generate continuation sequences (build labels as we go)
    generate_continuations()
    # 2c. Generate shared sequences (build labels as we go)
    generate_suffix_sequences()
    # 2d. Generate shared fetch_next as the last case
    generate_shared_fetch_next()
    # Step 3: Now generate all constants from the collected labels
    generate_all_constants()
    # Step 4: Generate lookup table (can now reference all constants)
    generate_lookup_table()
    # Step 5: Generate opcode cycles (can reference all labels)
    generate_opcode_cycles()
    # Step 6: Write the complete decoder file
    write_decoder_file()

if __name__ == '__main__':
    main()
