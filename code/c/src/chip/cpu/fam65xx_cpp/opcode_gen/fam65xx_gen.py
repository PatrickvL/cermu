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
    "c->AD = c->PC++;\nc->CI = c->opcode;",
))

AM_ZER = ("ZP", "zero page", "ADDR_ZER", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->AD = c->DL;\nc->CI = c->opcode;"
))

AM_ZPX = ("ZPX", "zero page,X", "ADDR_ZPX", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->AD = (c->DL + c->X) & 0xFF;\nc->CI = c->opcode;"
))

AM_ZPY = ("ZPY", "zero page,Y", "ADDR_ZPY", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->AD = (c->DL + c->Y) & 0xFF;\nc->CI = c->opcode;"
))

AM_ABS = ("ABS", "absolute", "ADDR_ABS", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->ADL = c->TMP;\nc->ADH = c->DL;\nc->CI = c->opcode;"
))

AM_ABX = ("ABX", "absolute,X", "ADDR_ABX", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "{\n\tuint16_t sum = c->TMP + c->X;\n\tif (sum > 0xFF) {\n\t\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\t\tc->CI = c->opcode;\n\t} else {\n\t\tc->AD = sum | (c->DL << 8); c->CI++;\n\t}\n}",
    "c->CI = c->opcode;"
))

AM_ABX_W = ("ABX", "absolute,X (write - always takes extra cycle)", "ADDR_ABX_W", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "{\n\tuint16_t sum = c->TMP + c->X;\n\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\tc->CI = c->opcode;\n}"
))

AM_ABY = ("ABY", "absolute,Y", "ADDR_ABY", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "{\n\tuint16_t sum = c->TMP + c->Y;\n\tif (sum > 0xFF) {\n\t\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\t\tc->CI = c->opcode;\n\t} else {\n\t\tc->AD = sum | (c->DL << 8);\n\t\tc->CI++;\n\t}\n}",
    "c->CI = c->opcode;"
))

AM_ABY_W = ("ABY", "absolute,Y (write - always takes extra cycle)", "ADDR_ABY_W", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "{\n\tuint16_t sum = c->TMP + c->Y;\n\tc->AD = (sum & 0xFF) | (((c->DL + (sum >> 8)) & 0xFF) << 8);\n\tc->CI = c->opcode;\n}"
))

AM_IDX = ("IDX", "indexed indirect (zp,X)", "ADDR_IDX", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->AD = (c->DL + c->X) & 0xFF;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = (c->AD + 1) & 0xFF;\nc->CI++;",
    "c->AD = (c->DL << 8) | c->TMP;\nc->CI++;",
    "c->CI = c->opcode;"
))

AM_IDY = ("IDY", "indirect indexed (zp),Y", "ADDR_IDY", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = (c->DL + 1) & 0xFF;\nc->CI++;",
    "c->AD = c->TMP | (c->DL << 8);\nif ((c->AD >> 8) != ((c->AD + c->Y) >> 8)) {\n\tc->CI++;\n} else {\n\tc->AD += c->Y; c->CI++;\n}",
    "c->AD += c->Y;\nc->CI++;",
    "c->CI = c->opcode;"
))

AM_IDY_W = ("IDY", "indirect indexed (zp),Y (write - always takes extra cycle)", "ADDR_IDY_W", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = (c->DL + 1) & 0xFF;\nc->CI++;",
    "c->AD = c->TMP | (c->DL << 8);\nc->CI++;",
    "c->AD += c->Y;\nc->CI++;",
    "c->CI = c->opcode;"
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

def generate_label(mnemonic, mem_access, addressing_mode=None):
    """Generate unique label for operation cycle sequence"""
    mem_suffix = {M___: "", M_R_: "_R", M__W: "_W", M_RW: "_RW"}[mem_access]
    # Only add addressing mode suffix if it's a real addressing mode (not AM_NON, AM_JMP, AM_JSR, AM_INV)
    addr_suffix = ""
    if addressing_mode and addressing_mode not in [AM_NON, AM_JMP, AM_JSR, AM_INV]:
        # Use the const name instead of acronym to avoid invalid C identifiers
        addr_suffix = f"_{addressing_mode[2].replace('ADDR_', '')}"
    return f"C_{mnemonic}{mem_suffix}{addr_suffix}"

# Simple implied mode operations
OP_BRK = ("BRK", M___, [
    "if (0 == (c->brk_flags & (FAM65XX_BRK_IRQ | FAM65XX_BRK_NMI))) {\n\tc->PC++;\n}\nc->write_src = R_PCH;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;",
    "c->write_src = R_PCL;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;",
    "c->TMP = c->P | FAM65XX_BF;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->P |= FAM65XX_IF;\nc->P &= ~FAM65XX_BF;\nc->brk_flags = 0;\nc->CI++;",
    "c->AD = _fam65xx_get_vector_addr(c);\nc->PCL = c->DL;\nc->CI++;",
    "c->AD++;\nc->PCH = c->DL;\nc->CI++;",
    "c->AD = c->PC;\nc->CI++;",
    "goto fetch_next;"
])

OP_PHP = ("PHP", M___, [
    "c->AD = c->PC;\nc->CI++;",
    "c->TMP = c->P | FAM65XX_BF;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;",
    "goto fetch_next;"
])

OP_PLP = ("PLP", M___, [
    "c->AD = 0x0100 | c->S++;\nc->CI++;",
    "c->AD = 0x0100 | c->S;\nc->P = (c->DL | FAM65XX_BF) & ~FAM65XX_XF;\nc->CI++;",
    "goto fetch_next;"
])

OP_PHA = ("PHA", M___, [
    "c->AD = c->PC;\nc->CI++;",
    "c->write_src = R_A;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;",
    "goto fetch_next;"
])

OP_PLA = ("PLA", M___, [
    "c->AD = 0x0100 | c->S++;\nc->CI++;",
    "c->AD = 0x0100 | c->S;\nc->A = c->DL;\n_NZ(c->A);\nc->CI++;",
    "goto fetch_next;"
])

OP_RTI = ("RTI", M_R_, [
    "c->AD = 0x0100 | c->S++;\nc->CI++;",
    "c->AD = 0x0100 | c->S++;\nc->P = (c->DL | FAM65XX_BF) & ~FAM65XX_XF;\nc->CI++;",
    "c->AD = 0x0100 | c->S++;\nc->PCL = c->DL;\nc->CI++;",
    "c->AD = 0x0100 | c->S;\nc->PCH = c->DL;\nc->CI++;",
    "goto fetch_next;"
])

OP_RTS = ("RTS", M_R_, [
    "c->AD = 0x0100 | c->S++;\nc->CI++;",
    "c->AD = 0x0100 | c->S++;\nc->PCL = c->DL;\nc->CI++;",
    "c->AD = 0x0100 | c->S;\nc->PCH = c->DL;\nc->CI++;",
    "goto fetch_next;"
])

OP_JSR = ("JSR", M_R_, [
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->AD = 0x0100 | c->S;\nc->CI++;",
    "c->write_src = R_PCH;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;",
    "c->write_src = R_PCL;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;",
    "c->AD = c->PC;\nc->PCH = c->DL;\nc->PC = (c->DL << 8) | c->TMP;\nc->CI++;",
    "goto fetch_next;"
])

OP_JMP = ("JMP", M_R_, [
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->AD = c->PC++;\nc->PCH = c->DL;\nc->PC = (c->DL << 8) | c->TMP;\nc->CI++;",
    "goto fetch_next;"
])

OP_JMP_I = ("JMP", M_R_, [
    "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->AD = c->PC++;\nc->TMP = c->DL;\nc->AD = (c->DL << 8) | c->TMP;\nc->CI++;",
    "c->AD = c->AD;\nc->PCL = c->DL;\nc->CI++;",
    "c->AD = (c->AD & 0xFF00) | ((c->AD + 1) & 0xFF);\nc->PCH = c->DL;\nc->CI++;",
    "goto fetch_next;"
])

# Register transfer operations (single cycle - immediate completion)
OP_TAX = ("TAX", M___, ["c->X = c->A;\n_NZ(c->X);\ngoto fetch_next;"])
OP_TXA = ("TXA", M___, ["c->A = c->X;\n_NZ(c->A);\ngoto fetch_next;"])
OP_TAY = ("TAY", M___, ["c->Y = c->A;\n_NZ(c->Y);\ngoto fetch_next;"])
OP_TYA = ("TYA", M___, ["c->A = c->Y;\n_NZ(c->A);\ngoto fetch_next;"])
OP_TSX = ("TSX", M___, ["c->X = c->S;\n_NZ(c->X);\ngoto fetch_next;"])
OP_TXS = ("TXS", M___, ["c->S = c->X;\ngoto fetch_next;"])

# Increment/Decrement operations (single cycle - immediate completion)
OP_DEX = ("DEX", M___, ["c->X--;\n_NZ(c->X);\ngoto fetch_next;"])
OP_INX = ("INX", M___, ["c->X++;\n_NZ(c->X);\ngoto fetch_next;"])
OP_DEY = ("DEY", M___, ["c->Y--;\n_NZ(c->Y);\ngoto fetch_next;"])
OP_INY = ("INY", M___, ["c->Y++;\n_NZ(c->Y);\ngoto fetch_next;"])

# Flag operations (single cycle - immediate completion)
OP_CLC = ("CLC", M___, ["c->P &= ~FAM65XX_CF;\ngoto fetch_next;"])
OP_SEC = ("SEC", M___, ["c->P |= FAM65XX_CF;\ngoto fetch_next;"])
OP_CLI = ("CLI", M___, ["c->P &= ~FAM65XX_IF;\ngoto fetch_next;"])
OP_SEI = ("SEI", M___, ["c->P |= FAM65XX_IF;\ngoto fetch_next;"])
OP_CLV = ("CLV", M___, ["c->P &= ~FAM65XX_VF;\ngoto fetch_next;"])
OP_CLD = ("CLD", M___, ["c->P &= ~FAM65XX_DF;\ngoto fetch_next;"])
OP_SED = ("SED", M___, ["c->P |= FAM65XX_DF;\ngoto fetch_next;"])

# Data-driven branch operations
OP_BPL = ("BPL", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_NF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BMI = ("BMI", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_NF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BVC = ("BVC", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_VF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BVS = ("BVS", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_VF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BCC = ("BCC", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_CF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BCS = ("BCS", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_CF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BNE = ("BNE", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_ZF) == 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

OP_BEQ = ("BEQ", M_R_, [
    "c->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ((c->P & FAM65XX_ZF) != 0) {\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;\n\tc->CI++;\n} else {\n\tgoto fetch_next;\n}",
    "if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\tc->CI++;\n}",
    "c->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;"
])

# ALU operations - Memory variants (single cycle after addressing mode)
OP_ORA = ("ORA", M_R_, ["c->A |= c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_AND = ("AND", M_R_, ["c->A &= c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_EOR = ("EOR", M_R_, ["c->A ^= c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_ADC = ("ADC", M_R_, ["_fam65xx_adc(c, c->DL);\ngoto fetch_next;"])
OP_SBC = ("SBC", M_R_, ["_fam65xx_sbc(c, c->DL);\ngoto fetch_next;"])
OP_CMP = ("CMP", M_R_, ["_fam65xx_cmp(c, c->A, c->DL);\ngoto fetch_next;"])
OP_BIT = ("BIT", M_R_, ["_fam65xx_bit(c, c->DL);\ngoto fetch_next;"])

# Immediate mode variants (read operand directly and execute)
OP_ORA_IMM = ("ORA", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A |= c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_AND_IMM = ("AND", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A &= c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_EOR_IMM = ("EOR", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A ^= c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_ADC_IMM = ("ADC", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;_fam65xx_adc(c, c->DL);\ngoto fetch_next;"])
OP_SBC_IMM = ("SBC", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;_fam65xx_sbc(c, c->DL);\ngoto fetch_next;"])
OP_CMP_IMM = ("CMP", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;_fam65xx_cmp(c, c->A, c->DL);\ngoto fetch_next;"])

# Load operations (single cycle after addressing mode)
OP_LDA = ("LDA", M_R_, ["c->A = c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_LDX = ("LDX", M_R_, ["c->X = c->DL;\n_NZ(c->X);\ngoto fetch_next;"])
OP_LDY = ("LDY", M_R_, ["c->Y = c->DL;\n_NZ(c->Y);\ngoto fetch_next;"])

# Immediate load operations (read operand directly and execute)
OP_LDA_IMM = ("LDA", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A = c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_LDX_IMM = ("LDX", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->X = c->DL;\n_NZ(c->X);\ngoto fetch_next;"])
OP_LDY_IMM = ("LDY", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->Y = c->DL;\n_NZ(c->Y);\ngoto fetch_next;"])

# Store operations (single cycle after addressing mode)
OP_STA = ("STA", M__W, ["c->write_src = R_A;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])
OP_STX = ("STX", M__W, ["c->write_src = R_X;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])
OP_STY = ("STY", M__W, ["c->write_src = R_Y;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])

# Compare operations (single cycle after addressing mode)
OP_CPX = ("CPX", M_R_, ["_fam65xx_cmp(c, c->X, c->DL);\ngoto fetch_next;"])
OP_CPY = ("CPY", M_R_, ["_fam65xx_cmp(c, c->Y, c->DL);\ngoto fetch_next;"])

# Immediate compare operations
OP_CPX_IMM = ("CPX", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;_fam65xx_cmp(c, c->X, c->DL);\ngoto fetch_next;"])
OP_CPY_IMM = ("CPY", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;_fam65xx_cmp(c, c->Y, c->DL);\ngoto fetch_next;"])

# RMW operations - have two variants (accumulator vs memory)
OP_ASL_A = ("ASL", M___, [
    "c->A = _fam65xx_asl(c, c->A);\nc->CI++;",
    "goto fetch_next;"
])

OP_ASL_M = ("ASL", M_RW, [
    "c->TMP = _fam65xx_asl(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_LSR_A = ("LSR", M___, [
    "c->A = _fam65xx_lsr(c, c->A);\nc->CI++;",
    "goto fetch_next;"
])

OP_LSR_M = ("LSR", M_RW, [
    "c->TMP = _fam65xx_lsr(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_ROL_A = ("ROL", M___, [
    "c->A = _fam65xx_rol(c, c->A);\nc->CI++;",
    "goto fetch_next;"
])

OP_ROL_M = ("ROL", M_RW, [
    "c->TMP = _fam65xx_rol(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_ROR_A = ("ROR", M___, [
    "c->A = _fam65xx_ror(c, c->A);\nc->CI++;",
    "goto fetch_next;"
])

OP_ROR_M = ("ROR", M_RW, [
    "c->TMP = _fam65xx_ror(c, c->DL);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_INC = ("INC", M_RW, [
    "c->TMP = c->DL + 1;\n_NZ(c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_DEC = ("DEC", M_RW, [
    "c->TMP = c->DL - 1;\n_NZ(c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

# NOP variants
OP_NOP_I = ("NOP", M___, [
    "c->AD = c->PC;\nc->CI++;",
    "goto fetch_next;"
])

OP_NOP_R = ("NOP", M_R_, ["goto fetch_next;"])

# Illegal/undocumented instructions
OP_LAX = ("LAX", M_R_, ["c->A = c->X = c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_SAX = ("SAX", M__W, ["c->TMP = c->A & c->X;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])

OP_SLO = ("SLO", M_RW, [
    "c->TMP = _fam65xx_asl(c, c->DL);\nc->A |= c->TMP;\n_NZ(c->A);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_RLA = ("RLA", M_RW, [
    "c->TMP = _fam65xx_rol(c, c->DL);\nc->A &= c->TMP;\n_NZ(c->A);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_SRE = ("SRE", M_RW, [
    "c->TMP = _fam65xx_lsr(c, c->DL);\nc->A ^= c->TMP;\n_NZ(c->A);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_RRA = ("RRA", M_RW, [
    "c->TMP = _fam65xx_ror(c, c->DL);\n_fam65xx_adc(c, c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_DCP = ("DCP", M_RW, [
    "c->TMP = c->DL - 1;\n_fam65xx_cmp(c, c->A, c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_ISC = ("ISC", M_RW, [
    "c->TMP = c->DL + 1;\n_fam65xx_sbc(c, c->TMP);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->CI++;",
    "goto fetch_next;"
])

OP_ANC = ("ANC", M_R_, ["c->A &= c->DL;\n_NZ(c->A);\nc->P = (c->P & ~FAM65XX_CF) | ((c->A & 0x80) ? FAM65XX_CF : 0);\ngoto fetch_next;"])
OP_ASR = ("ASR", M_R_, ["c->A &= c->DL;\nc->P = (c->P & ~FAM65XX_CF) | (c->A & 1);\nc->A>>=1;\n_NZ(c->A);\ngoto fetch_next;"])
OP_ARR = ("ARR", M_R_, ["c->A = (c->A & c->DL) >> 1 | (c->P & FAM65XX_CF ? 0x80 : 0);\n_NZ(c->A);\nc->P = (c->P & ~(FAM65XX_CF | FAM65XX_VF)) | ((c->A & 0x40) ? FAM65XX_CF : 0) | ((c->A & 0x20) ^ (c->A & 0x40) ? FAM65XX_VF : 0);\ngoto fetch_next;"])
OP_XAA = ("XAA", M_R_, ["c->A = (c->A | 0xEE) & c->X & c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_SBX = ("SBX", M_R_, ["c->TMP = (c->A & c->X) - c->DL;\nc->X = c->TMP;\n_NZ(c->X);\nc->P = (c->P & ~FAM65XX_CF) | ((c->TMP & 0x100) ? 0 : FAM65XX_CF);\ngoto fetch_next;"])

# Immediate mode illegal operations (read operand directly and execute)
OP_ANC_IMM = ("ANC", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A &= c->DL;\n_NZ(c->A);\nc->P = (c->P & ~FAM65XX_CF) | ((c->A & 0x80) ? FAM65XX_CF : 0);\ngoto fetch_next;"])
OP_ASR_IMM = ("ASR", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A &= c->DL;\nc->P = (c->P & ~FAM65XX_CF) | (c->A & 1);\nc->A>>=1;\n_NZ(c->A);\ngoto fetch_next;"])
OP_ARR_IMM = ("ARR", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A = (c->A & c->DL) >> 1 | (c->P & FAM65XX_CF ? 0x80 : 0);\n_NZ(c->A);\nc->P = (c->P & ~(FAM65XX_CF | FAM65XX_VF)) | ((c->A & 0x40) ? FAM65XX_CF : 0) | ((c->A & 0x20) ^ (c->A & 0x40) ? FAM65XX_VF : 0);\ngoto fetch_next;"])
OP_XAA_IMM = ("XAA", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A = (c->A | 0xEE) & c->X & c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_SBX_IMM = ("SBX", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->TMP = (c->A & c->X) - c->DL;\nc->X = c->TMP;\n_NZ(c->X);\nc->P = (c->P & ~FAM65XX_CF) | ((c->TMP & 0x100) ? 0 : FAM65XX_CF);\ngoto fetch_next;"])
OP_LAX_IMM = ("LAX", M_R_, ["c->AD = c->PC;\nc->CI++;", "c->PC++;\nc->A = c->X = c->DL;\n_NZ(c->A);\ngoto fetch_next;"])
OP_SHY = ("SHY", M__W, ["c->TMP = c->Y & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])
OP_SHX = ("SHX", M__W, ["c->TMP = c->X & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])
OP_SHA = ("SHA", M_RW, ["c->TMP = c->A & c->X & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])
OP_SHS = ("SHS", M__W, ["c->S = c->A & c->X;\nc->TMP = c->S & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"])
OP_LAS = ("LAS", M_R_, ["c->A = c->X = c->S = c->S & c->DL;\n_NZ(c->A);\ngoto fetch_next;"])

OP_JAM = ("JAM", M_R_, [
    "c->PC--;\nc->CI++;",
    "goto fetch_next;"
])

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
ADDR_SEQ_BASE = 255         # Addressing modes start at offset 1, so first mode at 256 (no gap after opcodes 0-255)

# These will be calculated dynamically in main() after addressing modes are defined
ADDR_MODE_INDICES = {}
ADDR_SEQ_END = 0
CONT_SEQ_START = 0

# Global state
continuation_sequences = {}  # label -> {index, cycles}
next_continuation_index = 0  # Will be set to CONT_SEQ_START in main()
opcode_groups = {}  # implementation_code -> [opcodes]

def l(s):
    """Output a line"""
    print(s)

def format_code(code):
    """Format code using embedded newlines and tabs for indentation"""
    lines = code.split('\n')
    formatted_lines = []
    
    for line in lines:
        line = line.rstrip()  # Remove trailing whitespace but preserve leading tabs
        if line:  # Only add non-empty lines
            formatted_lines.append('            ' + line)
    
    return '\n'.join(formatted_lines)

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

def analyze_continuation_needs(op):
    """Determine if opcode needs a continuation sequence"""
    operation, addr_mode = get_ops_entry(op)
    
    # All operations now have unified cycle lists
    cycles = operation[2]  # cycles are at index 2
    
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
    
    # If there are more cycles, replace c->CI++ with c->CI = label
    if len(cycles) > 1:
        label = generate_label(operation[0], operation[1], addr_mode if addr_mode != AM_NON else None)
        # Replace any c->CI++ with c->CI = label
        first_cycle = first_cycle.replace("c->CI++", f"c->CI = {label}")
    
    return first_cycle

def calculate_addressing_mode_offsets():
    """Calculate addressing mode offsets dynamically based on cycle counts"""
    global ADDR_MODE_INDICES, ADDR_SEQ_END, CONT_SEQ_START
    
    addr_mode_indices = {}
    current_offset = 1  # Start at offset 1 (0 is reserved for ADDR_NON)
    
    # Process addressing mode objects that have cycles
    for addr_mode in ADDRESSING_MODES:
        cycle_count = len(addr_mode[3])  # cycles are at index 3
        if cycle_count > 0:
            # Only assign offset to modes with cycles
            addr_mode_indices[addr_mode] = current_offset
            current_offset += cycle_count
        else:
            # Modes with 0 cycles (like AM_IMM) get ADDR_NON (0)
            addr_mode_indices[addr_mode] = 0
    
    # Update global variables
    ADDR_MODE_INDICES = addr_mode_indices
    ADDR_SEQ_END = ADDR_SEQ_BASE + current_offset
    CONT_SEQ_START = ADDR_SEQ_END

def generate_addressing_constants():
    """Generate addressing mode offset constants"""
    l("// Layout constants")
    l("// [0-255]   : Opcode-specific cycles")
    l("// [256+]    : Shared addressing mode sequences")
    l("#define ADDR_NON     0   // Direct opcode execution (no addressing mode)")
    l(f"#define ADDR_SEQ_BASE {ADDR_SEQ_BASE}")
    
    # Generate constants for addressing modes using definitions
    for addr_mode in ADDRESSING_MODES:
        if addr_mode in ADDR_MODE_INDICES:
            const_name = addr_mode[2]
            offset = ADDR_MODE_INDICES[addr_mode]
            if offset == 0:
                l(f"#define {const_name:<12} {offset:<3} // Direct opcode execution (0 cycles)")
            else:
                actual_index = ADDR_SEQ_BASE + offset
                l(f"#define {const_name:<12} {offset:<3} // Index {actual_index}")
    
    l("")

def generate_lookup_table():
    """Generate opcode_addr_start lookup table"""
    # Create reverse mapping from offset to constant name
    offset_to_const = {0: "ADDR_NON"}
    
    for addr_mode in ADDRESSING_MODES:
        if addr_mode in ADDR_MODE_INDICES:
            const_name = addr_mode[2]
            offset = ADDR_MODE_INDICES[addr_mode]
            offset_to_const[offset] = const_name
    
    l("// Lookup table: addressing mode start index for each opcode")
    l("const uint8_t opcode_addr_start[256] = {")
    
    for op in range(256):
        operation, addr_mode = get_ops_entry(op)
        
        if addr_mode in ADDR_MODE_INDICES:
            offset = ADDR_MODE_INDICES[addr_mode]
            const_name = offset_to_const[offset]
        else:
            # Special addressing modes (AM_NON, AM_JMP, AM_JSR, AM_INV) and fallback
            offset = NO_ADDR_SEQ
            const_name = "ADDR_NON"
        
        # Format with comma except for last element
        comma = "," if op < 255 else " "
        
        # Show memory access type in comment for clarity
        mem_access_str = {M___: "---", M_R_: "R", M__W: "W", M_RW: "RW"}[operation[1]]
        l(f"    {const_name}{comma}  // 0x{op:02X}: {operation[0]} [{mem_access_str}] {addr_mode[1]}")
    
    l("};")
    l("")

def generate_opcode_cases():
    """Generate opcode-specific cases with fallthrough optimization"""
    global opcode_groups
    
    # Group opcodes by implementation
    opcode_groups = {}
    for op in range(256):
        code = generate_opcode_implementation(op)
        if code not in opcode_groups:
            opcode_groups[code] = []
        opcode_groups[code].append(op)
    
    l("        // ==========================================")
    l("        // [0-255] OPCODE-SPECIFIC CYCLES")
    l("        // ==========================================")
    l("")
    
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
            l(f"        case 0x{opc:02X}:  // {operation[0]} [{mem_access_str}] {addr_acronym} cycle {cycle_num}")
            emitted.add(opc)
        
        l(format_code(code))
        
        # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
        if not code.strip().endswith('goto fetch_next;'):
            l(f"            break;")
        l("")

def generate_addressing_modes():
    """Generate shared addressing mode sequences using definitions"""
    l("        // ==========================================")
    l(f"        // [256-{ADDR_SEQ_END-1}] SHARED ADDRESSING SEQUENCES")
    l("        // ==========================================")
    l("")
    
    # Generate addressing mode sequences from definitions
    for addr_mode in ADDRESSING_MODES:
        acronym, long_name, const_name, cycles = addr_mode
        
        # Skip modes with no cycles (they execute directly in opcode case)
        if len(cycles) == 0:
            continue
        
        # Emit long name comment before the cycles
        l(f"        // {acronym}: {long_name}")
        
        for cycle_idx, cycle_code in enumerate(cycles):
            cycle_num = cycle_idx + 1
            
            l(f"        case ADDR_SEQ_BASE + {const_name} + {cycle_idx}:  // {acronym} cycle {cycle_num}")
            l(format_code(cycle_code))
            
            # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
            if not cycle_code.strip().endswith('goto fetch_next;'):
                l("            break;")
            
        l("")

def generate_continuations():
    """Generate shared multi-cycle continuation sequences"""
    l("        // ==========================================")
    l(f"        // [{CONT_SEQ_START}-{next_continuation_index-1}] SHARED CONTINUATIONS")
    l("        // ==========================================")
    l("")
    
    # Sort by index to emit in order without gaps
    sorted_seqs = sorted(continuation_sequences.items(),
                        key=lambda x: x[1]['index'])
    
    for label, seq_info in sorted_seqs:
        idx = seq_info['index']
        cycles = seq_info['cycles']
        
        l(f"        // {label} continuation")
        for i, cycle_code in enumerate(cycles):
            l(f"        case {label} + {i}:")
            l(format_code(cycle_code))
            
            # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
            if not cycle_code.strip().endswith('goto fetch_next;'):
                l("            break;")
        l("")

def generate_continuation_constants():
    """Generate constant declarations for continuation sequences"""
    l("// Continuation sequence constants")
    for label, seq_info in sorted(continuation_sequences.items(),
                                     key=lambda x: x[1]['index']):
        l(f"#define {label:<16} {seq_info['index']}")
    l("")

def main():
    # Calculate addressing mode layout dynamically
    calculate_addressing_mode_offsets()
    
    # Initialize continuation index after layout is calculated
    global next_continuation_index
    next_continuation_index = CONT_SEQ_START
    
    # First pass: analyze all opcodes to discover continuation sequences
    for op in range(256):
        analyze_continuation_needs(op)
    
    # Calculate final continuation index for correct comment
    final_continuation_index = next_continuation_index
    
    l("/*")
    l(" * AUTO-GENERATED by fam65xx_gen.py")
    l(" * 65xx Family CPU Decoder")
    l(" * Optimized for maximum performance and 100% hardware accuracy")
    l(f" * ")
    l(f" * Layout:")
    l(f" *   [0-255]   : Opcode-specific cycles")
    l(f" *   [256-{ADDR_SEQ_END-1}] : Shared addressing mode sequences")
    l(f" *   [{CONT_SEQ_START}-{final_continuation_index-1}] : Shared continuation sequences")
    l(f" * Total cases : {final_continuation_index}")
    l(" */")
    l("")
    
    generate_continuation_constants()
    generate_addressing_constants()
    generate_lookup_table()
    
    l("// Decoder switch statement")
    l("static inline uint64_t _fam65xx_decode(fam65xx_t* c, uint64_t pins) {")
    l("    switch (c->CI) {")
    l("")
    
    generate_opcode_cases()
    generate_addressing_modes()
    generate_continuations()
    
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

if __name__ == '__main__':
    main()
