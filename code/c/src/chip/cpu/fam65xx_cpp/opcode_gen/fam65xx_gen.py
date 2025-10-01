#!/usr/bin/env python3
#-------------------------------------------------------------------------------
#   fam65xx_gen.py
#   Generate instruction decoder for FAM65xx family CPU emulator.
#   Optimized for world's fastest 100% hardware accurate implementation.
#-------------------------------------------------------------------------------

# Flag bits
CF = (1<<0)
ZF = (1<<1)
IF = (1<<2)
DF = (1<<3)
BF = (1<<4)
XF = (1<<5)
VF = (1<<6)
NF = (1<<7)

# Addressing mode constants
A____ = 0       # no addressing mode
A_IMM = 1       # immediate
A_ZER = 2       # zero-page
A_ZPX = 3       # zp,X
A_ZPY = 4       # zp,Y
A_ABS = 5       # abs
A_ABX = 6       # abs,X
A_ABY = 7       # abs,Y
A_IDX = 8       # (zp,X)
A_IDY = 9       # (zp),Y
A_JMP = 10      # (abs) special JMP
A_JSR = 11      # special JSR abs
A_INV = 12      # invalid instruction

# Memory access modes
M___ = 0        # no memory access
M_R_ = 1        # read access
M__W = 2        # write access
M_RW = 3        # read-modify-write

# Layout constants
ADDR_SEQ_BASE = 255
NO_ADDR_SEQ = 0         # Direct opcode jump (no addressing mode)

# These will be calculated dynamically in main() after addressing modes are defined
ADDR_MODE_INDICES = {}
ADDR_SEQ_END = 0
CONT_SEQ_START = 0

#-------------------------------------------------------------------------------
# Addressing Mode Definitions
# Each definition contains: (acronym, long_name, const_name, cycles_list)
# Each cycle is a string containing the cycle code
#-------------------------------------------------------------------------------

ADDR_IMM = ("IMM", "immediate", "ADDR_IMM", [
    "BUS_READ(c->PC++);\nc->AD = BUS_DATA();\nc->IR = c->opcode;"
])

ADDR_ZER = ("ZP", "zero page", "ADDR_ZER", [
    "BUS_READ(c->PC++);",
    "c->AD = BUS_DATA();\nc->IR = c->opcode;"
])

ADDR_ZPX = ("ZPX", "zero page,X", "ADDR_ZPX", [
    "BUS_READ(c->PC++);",
    "c->AD = BUS_DATA();\nBUS_INTERNAL(c->AD);",
    "c->AD = (c->AD + c->X) & 0xFF;\nc->IR = c->opcode;"
])

ADDR_ZPY = ("ZPY", "zero page,Y", "ADDR_ZPY", [
    "BUS_READ(c->PC++);",
    "c->AD = BUS_DATA();\nBUS_INTERNAL(c->AD);",
    "c->AD = (c->AD + c->Y) & 0xFF;\nc->IR = c->opcode;"
])

ADDR_ABS = ("ABS", "absolute", "ADDR_ABS", [
    "BUS_READ(c->PC++);\nc->AD = BUS_DATA();",
    "BUS_READ(c->PC++);\nc->AD |= BUS_DATA() << 8;",
    "c->IR = c->opcode;"
])

ADDR_ABX = ("ABX", "absolute,X", "ADDR_ABX", [
    "BUS_READ(c->PC++);\nc->AD = BUS_DATA();",
    "BUS_READ(c->PC++);\nc->AD |= BUS_DATA() << 8;",
    "BUS_READ((c->AD & 0xFF00) | ((c->AD + c->X) & 0xFF));\nif (((c->AD >> 8) == ((c->AD + c->X) >> 8))) {\n\tc->AD += c->X;\n\tc->IR = c->opcode;\n}",
    "c->AD += c->X;\nc->IR = c->opcode;"
])

ADDR_ABY = ("ABY", "absolute,Y", "ADDR_ABY", [
    "BUS_READ(c->PC++);\nc->AD = BUS_DATA();",
    "BUS_READ(c->PC++);\nc->AD |= BUS_DATA() << 8;",
    "BUS_READ((c->AD & 0xFF00) | ((c->AD + c->Y) & 0xFF));\nif (((c->AD >> 8) == ((c->AD + c->Y) >> 8))) {\n\tc->AD += c->Y;\n\tc->IR = c->opcode;\n}",
    "c->AD += c->Y;\nc->IR = c->opcode;"
])

ADDR_IDX = ("IDX", "indexed indirect (zp,X)", "ADDR_IDX", [
    "BUS_READ(c->PC++);",
    "c->AD = BUS_DATA();\nBUS_INTERNAL(c->AD);",
    "c->AD = (c->AD + c->X) & 0xFF;\nBUS_READ(c->AD);",
    "BUS_READ((c->AD + 1) & 0xFF);\nc->AD = BUS_DATA();",
    "c->AD |= BUS_DATA() << 8;\nc->IR = c->opcode;"
])

ADDR_IDY = ("IDY", "indirect indexed (zp),Y", "ADDR_IDY", [
    "BUS_READ(c->PC++);",
    "c->AD = BUS_DATA();\nBUS_READ(c->AD);",
    "BUS_READ((c->AD + 1) & 0xFF);\nc->AD = BUS_DATA();",
    "c->AD |= BUS_DATA() << 8;\nBUS_READ((c->AD & 0xFF00) | ((c->AD + c->Y) & 0xFF));\nif (((c->AD >> 8) == ((c->AD + c->Y) >> 8))) {\n\tc->AD += c->Y;\n\tc->IR = c->opcode;\n}",
    "c->AD += c->Y;\nc->IR = c->opcode;"
])

# Addressing mode lookup table
ADDRESSING_MODES = {
    A_IMM: ADDR_IMM,
    A_ZER: ADDR_ZER,
    A_ZPX: ADDR_ZPX,
    A_ZPY: ADDR_ZPY,
    A_ABS: ADDR_ABS,
    A_ABX: ADDR_ABX,
    A_ABY: ADDR_ABY,
    A_IDX: ADDR_IDX,
    A_IDY: ADDR_IDY,
}

# Global state
continuation_sequences = {}  # sequence_code -> {index, name, code}
next_continuation_index = 0  # Will be set to CONT_SEQ_START in main()
opcode_groups = {}  # implementation_code -> [opcodes]

#-------------------------------------------------------------------------------
# Mnemonic Symbol Definitions
# Each symbol contains: (name, mem_access, implementation_code, flags)
#-------------------------------------------------------------------------------

# Simple implied mode operations
OP_BRK = ("BRK", M___, "if (0 == (c->brk_flags & (FAM65XX_BRK_IRQ | FAM65XX_BRK_NMI))) {\n\tc->PC++;\n}\nBUS_WRITE(0x0100 | c->S--, c->PC >> 8);\nc->IR = C_BRK;", 'CONT')
OP_PHP = ("PHP", M__W, "BUS_WRITE(0x0100 | c->S--, c->P | FAM65XX_XF);\n_FETCH();", None)
OP_PLP = ("PLP", M___, "BUS_INTERNAL(0x0100 | c->S++);\nc->IR = C_PLP;", 'CONT')
OP_PHA = ("PHA", M__W, "BUS_WRITE(0x0100 | c->S--, c->A);\n_FETCH();", None)
OP_PLA = ("PLA", M___, "BUS_INTERNAL(0x0100 | c->S++);\nc->IR = C_PLA;", 'CONT')
OP_RTI = ("RTI", M_R_, "BUS_INTERNAL(0x0100 | c->S++);\nc->IR = C_RTI;", 'CONT')
OP_RTS = ("RTS", M_R_, "BUS_INTERNAL(0x0100 | c->S++);\nc->IR = C_RTS;", 'CONT')
OP_JSR = ("JSR", M_R_, "BUS_READ(c->PC++);\nc->AD = BUS_DATA();\nc->IR = C_JSR;", 'CONT')
OP_JMP = ("JMP", M_R_, "BUS_READ(c->PC++);\nc->AD = BUS_DATA();\nc->IR = C_JMP_ABS;", 'CONT')
OP_JMI = ("JMP", M_R_, "BUS_READ(c->PC++);\nc->AD = BUS_DATA();\nc->IR = C_JMP_IND;", 'CONT')  # JMP indirect

# Register transfer operations
OP_TAX = ("TAX", M___, "BUS_INTERNAL(c->PC);\nc->X = c->A;\n_NZ(c->X);\n_FETCH();", None)
OP_TXA = ("TXA", M___, "BUS_INTERNAL(c->PC);\nc->A = c->X;\n_NZ(c->A);\n_FETCH();", None)
OP_TAY = ("TAY", M___, "BUS_INTERNAL(c->PC);\nc->Y = c->A;\n_NZ(c->Y);\n_FETCH();", None)
OP_TYA = ("TYA", M___, "BUS_INTERNAL(c->PC);\nc->A = c->Y;\n_NZ(c->A);\n_FETCH();", None)
OP_TSX = ("TSX", M___, "BUS_INTERNAL(c->PC);\nc->X = c->S;\n_NZ(c->X);\n_FETCH();", None)
OP_TXS = ("TXS", M___, "BUS_INTERNAL(c->PC);\nc->S = c->X;\n_FETCH();", None)

# Increment/Decrement operations
OP_DEX = ("DEX", M___, "BUS_INTERNAL(c->PC);\nc->X--;\n_NZ(c->X);\n_FETCH();", None)
OP_INX = ("INX", M___, "BUS_INTERNAL(c->PC);\nc->X++;\n_NZ(c->X);\n_FETCH();", None)
OP_DEY = ("DEY", M___, "BUS_INTERNAL(c->PC);\nc->Y--;\n_NZ(c->Y);\n_FETCH();", None)
OP_INY = ("INY", M___, "BUS_INTERNAL(c->PC);\nc->Y++;\n_NZ(c->Y);\n_FETCH();", None)

# Flag operations
OP_CLC = ("CLC", M___, "BUS_INTERNAL(c->PC);\nc->P &= ~FAM65XX_CF;\n_FETCH();", None)
OP_SEC = ("SEC", M___, "BUS_INTERNAL(c->PC);\nc->P |= FAM65XX_CF;\n_FETCH();", None)
OP_CLI = ("CLI", M___, "BUS_INTERNAL(c->PC);\nc->P &= ~FAM65XX_IF;\n_FETCH();", None)
OP_SEI = ("SEI", M___, "BUS_INTERNAL(c->PC);\nc->P |= FAM65XX_IF;\n_FETCH();", None)
OP_CLV = ("CLV", M___, "BUS_INTERNAL(c->PC);\nc->P &= ~FAM65XX_VF;\n_FETCH();", None)
OP_CLD = ("CLD", M___, "BUS_INTERNAL(c->PC);\nc->P &= ~FAM65XX_DF;\n_FETCH();", None)
OP_SED = ("SED", M___, "BUS_INTERNAL(c->PC);\nc->P |= FAM65XX_DF;\n_FETCH();", None)

# Branch operations (all share BRANCH_TAKEN continuation)
OP_BPL = ("BPL", M_R_, None, 'BRANCH')
OP_BMI = ("BMI", M_R_, None, 'BRANCH')
OP_BVC = ("BVC", M_R_, None, 'BRANCH')
OP_BVS = ("BVS", M_R_, None, 'BRANCH')
OP_BCC = ("BCC", M_R_, None, 'BRANCH')
OP_BCS = ("BCS", M_R_, None, 'BRANCH')
OP_BNE = ("BNE", M_R_, None, 'BRANCH')
OP_BEQ = ("BEQ", M_R_, None, 'BRANCH')

# ALU operations (memory read)
OP_ORA = ("ORA", M_R_, "BUS_READ(c->AD);\nc->A |= BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_AND = ("AND", M_R_, "BUS_READ(c->AD);\nc->A &= BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_EOR = ("EOR", M_R_, "BUS_READ(c->AD);\nc->A ^= BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_ADC = ("ADC", M_R_, "BUS_READ(c->AD);\n_fam65xx_adc(c, BUS_DATA());\n_FETCH();", None)
OP_SBC = ("SBC", M_R_, "BUS_READ(c->AD);\n_fam65xx_sbc(c, BUS_DATA());\n_FETCH();", None)
OP_CMP = ("CMP", M_R_, "BUS_READ(c->AD);\n_fam65xx_cmp(c, c->A, BUS_DATA());\n_FETCH();", None)
OP_BIT = ("BIT", M_R_, "BUS_READ(c->AD);\n_fam65xx_bit(c, BUS_DATA());\n_FETCH();", None)

# Load operations
OP_LDA = ("LDA", M_R_, "BUS_READ(c->AD);\nc->A = BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_LDX = ("LDX", M_R_, "BUS_READ(c->AD);\nc->X = BUS_DATA();\n_NZ(c->X);\n_FETCH();", None)
OP_LDY = ("LDY", M_R_, "BUS_READ(c->AD);\nc->Y = BUS_DATA();\n_NZ(c->Y);\n_FETCH();", None)

# Store operations
OP_STA = ("STA", M__W, "BUS_WRITE(c->AD, c->A);\n_FETCH();", None)
OP_STX = ("STX", M__W, "BUS_WRITE(c->AD, c->X);\n_FETCH();", None)
OP_STY = ("STY", M__W, "BUS_WRITE(c->AD, c->Y);\n_FETCH();", None)

# Compare operations
OP_CPX = ("CPX", M_R_, "BUS_READ(c->AD);\n_fam65xx_cmp(c, c->X, BUS_DATA());\n_FETCH();", None)
OP_CPY = ("CPY", M_R_, "BUS_READ(c->AD);\n_fam65xx_cmp(c, c->Y, BUS_DATA());\n_FETCH();", None)

# RMW operations - have two variants (accumulator vs memory)
OP_ASL_A = ("ASL", M___, "BUS_INTERNAL(c->PC);\nc->A = _fam65xx_asl(c, c->A);\n_FETCH();", None)
OP_ASL_M = ("ASL", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_ASL_RMW;", 'RMW')
OP_LSR_A = ("LSR", M___, "BUS_INTERNAL(c->PC);\nc->A = _fam65xx_lsr(c, c->A);\n_FETCH();", None)
OP_LSR_M = ("LSR", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_LSR_RMW;", 'RMW')
OP_ROL_A = ("ROL", M___, "BUS_INTERNAL(c->PC);\nc->A = _fam65xx_rol(c, c->A);\n_FETCH();", None)
OP_ROL_M = ("ROL", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_ROL_RMW;", 'RMW')
OP_ROR_A = ("ROR", M___, "BUS_INTERNAL(c->PC);\nc->A = _fam65xx_ror(c, c->A);\n_FETCH();", None)
OP_ROR_M = ("ROR", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_ROR_RMW;", 'RMW')
OP_INC_M = ("INC", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_INC_RMW;", 'RMW')
OP_DEC_M = ("DEC", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_DEC_RMW;", 'RMW')

# NOP variants
OP_NOP_I = ("NOP", M___, "BUS_INTERNAL(c->PC);\n_FETCH();", None)  # Implied NOP
OP_NOP_R = ("NOP", M_R_, "BUS_READ(c->AD);\n_FETCH();", None)      # NOPs that read

# Illegal/undocumented instructions
OP_LAX = ("LAX", M_R_, "BUS_READ(c->AD);\nc->A = c->X = BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_SAX = ("SAX", M__W, "BUS_WRITE(c->AD, c->A & c->X);\n_FETCH();", None)
OP_SLO = ("SLO", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_SLO_RMW;", 'RMW')
OP_RLA = ("RLA", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_RLA_RMW;", 'RMW')
OP_SRE = ("SRE", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_SRE_RMW;", 'RMW')
OP_RRA = ("RRA", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_RRA_RMW;", 'RMW')
OP_DCP = ("DCP", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_DCP_RMW;", 'RMW')
OP_ISC = ("ISC", M_RW, "BUS_READ(c->AD);\nc->AD = BUS_DATA();\nc->IR = C_ISC_RMW;", 'RMW')
OP_ANC = ("ANC", M_R_, "BUS_READ(c->AD);\nc->A &= BUS_DATA();\n_NZ(c->A);\nc->P = (c->P & ~FAM65XX_CF) | ((c->A & 0x80) ? FAM65XX_CF : 0);\n_FETCH();", None)
OP_ASR = ("ASR", M_R_, "BUS_READ(c->AD);\nc->A &= BUS_DATA();\nc->P = (c->P & ~FAM65XX_CF) | (c->A & 1);\nc->A>>=1;\n_NZ(c->A);\n_FETCH();", None)
OP_ARR = ("ARR", M_R_, "BUS_READ(c->AD);\nc->A = (c->A & BUS_DATA()) >> 1 | (c->P & FAM65XX_CF ? 0x80 : 0);\n_NZ(c->A);\nc->P = (c->P & ~(FAM65XX_CF | FAM65XX_VF)) | ((c->A & 0x40) ? FAM65XX_CF : 0) | ((c->A & 0x20) ^ (c->A & 0x40) ? FAM65XX_VF : 0);\n_FETCH();", None)
OP_XAA = ("XAA", M_R_, "BUS_READ(c->AD);\nc->A = (c->A | 0xEE) & c->X & BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_SBX = ("SBX", M_R_, "BUS_READ(c->AD);\n{\n\tuint16_t t = (c->A & c->X) - BUS_DATA();\n\tc->X = t;\n_NZ(c->X);\nc->P = (c->P & ~FAM65XX_CF) | ((t & 0x100) ? 0 : FAM65XX_CF);\n}\n_FETCH();", None)
OP_SHY = ("SHY", M__W, "BUS_WRITE(c->AD, c->Y & ((c->AD >> 8) + 1));\n_FETCH();", None)
OP_SHX = ("SHX", M__W, "BUS_WRITE(c->AD, c->X & ((c->AD >> 8) + 1));\n_FETCH();", None)
OP_SHA = ("SHA", M_RW, "BUS_WRITE(c->AD, c->A & c->X & ((c->AD >> 8) + 1));\n_FETCH();", None)
OP_SHS = ("SHS", M__W, "c->S = c->A & c->X;\nBUS_WRITE(c->AD, c->S & ((c->AD >> 8) + 1));\n_FETCH();", None)
OP_LAS = ("LAS", M_R_, "BUS_READ(c->AD);\nc->A = c->X = c->S = c->S & BUS_DATA();\n_NZ(c->A);\n_FETCH();", None)
OP_JAM = ("JAM", M_RW, "BUS_READ(c->PC);\nc->IR--;", None)  # JAM locks up

#-------------------------------------------------------------------------------
# Instruction table: [mnemonic_symbol, addressing_mode]
#-------------------------------------------------------------------------------
ops = [
    # cc = 00
    [
        [[OP_BRK,A____],[OP_JSR,A_JSR],[OP_RTI,A____],[OP_RTS,A____],[OP_NOP_R,A_IMM],[OP_LDY,A_IMM],[OP_CPY,A_IMM],[OP_CPX,A_IMM]],
        [[OP_NOP_R,A_ZER],[OP_BIT,A_ZER],[OP_NOP_R,A_ZER],[OP_NOP_R,A_ZER],[OP_STY,A_ZER],[OP_LDY,A_ZER],[OP_CPY,A_ZER],[OP_CPX,A_ZER]],
        [[OP_PHP,A____],[OP_PLP,A____],[OP_PHA,A____],[OP_PLA,A____],[OP_DEY,A____],[OP_TAY,A____],[OP_INY,A____],[OP_INX,A____]],
        [[OP_NOP_R,A_ABS],[OP_BIT,A_ABS],[OP_JMP,A_JMP],[OP_JMI,A_JMP],[OP_STY,A_ABS],[OP_LDY,A_ABS],[OP_CPY,A_ABS],[OP_CPX,A_ABS]],
        [[OP_BPL,A_IMM],[OP_BMI,A_IMM],[OP_BVC,A_IMM],[OP_BVS,A_IMM],[OP_BCC,A_IMM],[OP_BCS,A_IMM],[OP_BNE,A_IMM],[OP_BEQ,A_IMM]],
        [[OP_NOP_R,A_ZPX],[OP_NOP_R,A_ZPX],[OP_NOP_R,A_ZPX],[OP_NOP_R,A_ZPX],[OP_STY,A_ZPX],[OP_LDY,A_ZPX],[OP_NOP_R,A_ZPX],[OP_NOP_R,A_ZPX]],
        [[OP_CLC,A____],[OP_SEC,A____],[OP_CLI,A____],[OP_SEI,A____],[OP_TYA,A____],[OP_CLV,A____],[OP_CLD,A____],[OP_SED,A____]],
        [[OP_NOP_R,A_ABX],[OP_NOP_R,A_ABX],[OP_NOP_R,A_ABX],[OP_NOP_R,A_ABX],[OP_SHY,A_ABX],[OP_LDY,A_ABX],[OP_NOP_R,A_ABX],[OP_NOP_R,A_ABX]]
    ],
    # cc = 01
    [
        [[OP_ORA,A_IDX],[OP_AND,A_IDX],[OP_EOR,A_IDX],[OP_ADC,A_IDX],[OP_STA,A_IDX],[OP_LDA,A_IDX],[OP_CMP,A_IDX],[OP_SBC,A_IDX]],
        [[OP_ORA,A_ZER],[OP_AND,A_ZER],[OP_EOR,A_ZER],[OP_ADC,A_ZER],[OP_STA,A_ZER],[OP_LDA,A_ZER],[OP_CMP,A_ZER],[OP_SBC,A_ZER]],
        [[OP_ORA,A_IMM],[OP_AND,A_IMM],[OP_EOR,A_IMM],[OP_ADC,A_IMM],[OP_NOP_R,A_IMM],[OP_LDA,A_IMM],[OP_CMP,A_IMM],[OP_SBC,A_IMM]],
        [[OP_ORA,A_ABS],[OP_AND,A_ABS],[OP_EOR,A_ABS],[OP_ADC,A_ABS],[OP_STA,A_ABS],[OP_LDA,A_ABS],[OP_CMP,A_ABS],[OP_SBC,A_ABS]],
        [[OP_ORA,A_IDY],[OP_AND,A_IDY],[OP_EOR,A_IDY],[OP_ADC,A_IDY],[OP_STA,A_IDY],[OP_LDA,A_IDY],[OP_CMP,A_IDY],[OP_SBC,A_IDY]],
        [[OP_ORA,A_ZPX],[OP_AND,A_ZPX],[OP_EOR,A_ZPX],[OP_ADC,A_ZPX],[OP_STA,A_ZPX],[OP_LDA,A_ZPX],[OP_CMP,A_ZPX],[OP_SBC,A_ZPX]],
        [[OP_ORA,A_ABY],[OP_AND,A_ABY],[OP_EOR,A_ABY],[OP_ADC,A_ABY],[OP_STA,A_ABY],[OP_LDA,A_ABY],[OP_CMP,A_ABY],[OP_SBC,A_ABY]],
        [[OP_ORA,A_ABX],[OP_AND,A_ABX],[OP_EOR,A_ABX],[OP_ADC,A_ABX],[OP_STA,A_ABX],[OP_LDA,A_ABX],[OP_CMP,A_ABX],[OP_SBC,A_ABX]]
    ],
    # cc = 02
    [
        [[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_NOP_R,A_IMM],[OP_LDX,A_IMM],[OP_NOP_R,A_IMM],[OP_NOP_R,A_IMM]],
        [[OP_ASL_M,A_ZER],[OP_ROL_M,A_ZER],[OP_LSR_M,A_ZER],[OP_ROR_M,A_ZER],[OP_STX,A_ZER],[OP_LDX,A_ZER],[OP_DEC_M,A_ZER],[OP_INC_M,A_ZER]],
        [[OP_ASL_A,A____],[OP_ROL_A,A____],[OP_LSR_A,A____],[OP_ROR_A,A____],[OP_TXA,A____],[OP_TAX,A____],[OP_DEX,A____],[OP_NOP_I,A____]],
        [[OP_ASL_M,A_ABS],[OP_ROL_M,A_ABS],[OP_LSR_M,A_ABS],[OP_ROR_M,A_ABS],[OP_STX,A_ABS],[OP_LDX,A_ABS],[OP_DEC_M,A_ABS],[OP_INC_M,A_ABS]],
        [[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV],[OP_JAM,A_INV]],
        [[OP_ASL_M,A_ZPX],[OP_ROL_M,A_ZPX],[OP_LSR_M,A_ZPX],[OP_ROR_M,A_ZPX],[OP_STX,A_ZPY],[OP_LDX,A_ZPY],[OP_DEC_M,A_ZPX],[OP_INC_M,A_ZPX]],
        [[OP_NOP_R,A____],[OP_NOP_R,A____],[OP_NOP_R,A____],[OP_NOP_R,A____],[OP_TXS,A____],[OP_TSX,A____],[OP_NOP_R,A____],[OP_NOP_R,A____]],
        [[OP_ASL_M,A_ABX],[OP_ROL_M,A_ABX],[OP_LSR_M,A_ABX],[OP_ROR_M,A_ABX],[OP_SHX,A_ABY],[OP_LDX,A_ABY],[OP_DEC_M,A_ABX],[OP_INC_M,A_ABX]]
    ],
    # cc = 03
    [
        [[OP_SLO,A_IDX],[OP_RLA,A_IDX],[OP_SRE,A_IDX],[OP_RRA,A_IDX],[OP_SAX,A_IDX],[OP_LAX,A_IDX],[OP_DCP,A_IDX],[OP_ISC,A_IDX]],
        [[OP_SLO,A_ZER],[OP_RLA,A_ZER],[OP_SRE,A_ZER],[OP_RRA,A_ZER],[OP_SAX,A_ZER],[OP_LAX,A_ZER],[OP_DCP,A_ZER],[OP_ISC,A_ZER]],
        [[OP_ANC,A_IMM],[OP_ANC,A_IMM],[OP_ASR,A_IMM],[OP_ARR,A_IMM],[OP_XAA,A_IMM],[OP_LAX,A_IMM],[OP_SBX,A_IMM],[OP_SBC,A_IMM]],
        [[OP_SLO,A_ABS],[OP_RLA,A_ABS],[OP_SRE,A_ABS],[OP_RRA,A_ABS],[OP_SAX,A_ABS],[OP_LAX,A_ABS],[OP_DCP,A_ABS],[OP_ISC,A_ABS]],
        [[OP_SLO,A_IDY],[OP_RLA,A_IDY],[OP_SRE,A_IDY],[OP_RRA,A_IDY],[OP_SHA,A_IDY],[OP_LAX,A_IDY],[OP_DCP,A_IDY],[OP_ISC,A_IDY]],
        [[OP_SLO,A_ZPX],[OP_RLA,A_ZPX],[OP_SRE,A_ZPX],[OP_RRA,A_ZPX],[OP_SAX,A_ZPY],[OP_LAX,A_ZPY],[OP_DCP,A_ZPX],[OP_ISC,A_ZPX]],
        [[OP_SLO,A_ABY],[OP_RLA,A_ABY],[OP_SRE,A_ABY],[OP_RRA,A_ABY],[OP_SHS,A_ABY],[OP_LAS,A_ABY],[OP_DCP,A_ABY],[OP_ISC,A_ABY]],
        [[OP_SLO,A_ABX],[OP_RLA,A_ABX],[OP_SRE,A_ABX],[OP_RRA,A_ABX],[OP_SHY,A_ABY],[OP_LAX,A_ABY],[OP_DCP,A_ABX],[OP_ISC,A_ABX]]
    ]
]

def l(s):
    """Output a line"""
    print(s)

def get_mnemonic_symbol(op):
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    return ops[cc][bbb][aaa][0]

def get_addr_mode(op):
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    return ops[cc][bbb][aaa][1]

def get_mnemonic(op):
    return get_mnemonic_symbol(op)[0]

def get_mem_access(op):
    return get_mnemonic_symbol(op)[1]

def get_implementation(op):
    return get_mnemonic_symbol(op)[2]

def get_flags(op):
    return get_mnemonic_symbol(op)[3]

def get_or_create_continuation(sequence_code, name):
    """Get existing continuation sequence index or create new one"""
    global next_continuation_index
    
    if sequence_code not in continuation_sequences:
        # Split on 'break;' to get individual cycles
        cycles = [c.strip() for c in sequence_code.split('break;') if c.strip()]
        
        continuation_sequences[sequence_code] = {
            'index': next_continuation_index,
            'name': name,
            'cycles': cycles
        }
        next_continuation_index += len(cycles)
    
    return continuation_sequences[sequence_code]['index']

def analyze_continuation_needs(op):
    """Determine if opcode needs a continuation sequence - handle special RMW cases"""
    flags = get_flags(op)
    
    # Handle special multi-cycle operations with hardcoded continuations FIRST
    if op == 0x00:  # BRK
        seq = ('BUS_WRITE(0x0100 | c->S--, c->P | FAM65XX_XF);\nif (c->brk_flags & FAM65XX_BRK_RESET) {\n\tc->AD = 0xFFFC;\n} else {\n\tif (c->brk_flags & FAM65XX_BRK_NMI) {\n\t\tc->AD = 0xFFFA;\n\t} else {\n\t\tc->AD = 0xFFFE;\n\t}\n};\nbreak;' +
               'BUS_READ(c->AD++);\nc->P |= (FAM65XX_IF | FAM65XX_BF);\nc->brk_flags = 0; break;' +
               'BUS_READ(c->AD);\nc->AD = BUS_DATA();\nbreak;' +
               'c->PC = (BUS_DATA() << 8) | c->AD;\n_FETCH();')
        get_or_create_continuation(seq, 'BRK')
        return ('BRK', seq)
    elif op == 0x20:  # JSR
        seq = ('BUS_INTERNAL(0x0100 | c->S);\nbreak;' +
               'BUS_WRITE(0x0100 | c->S--, c->PC >> 8);\nbreak;' +
               'BUS_WRITE(0x0100 | c->S--, c->PC);\nbreak;' +
               'BUS_READ(c->PC);\nbreak;' +
               'c->PC = (BUS_DATA() << 8) | c->AD;\n_FETCH();')
        get_or_create_continuation(seq, 'JSR')
        return ('JSR', seq)
    elif op == 0x40:  # RTI
        seq = ('BUS_READ(0x0100 | c->S++);\nbreak;' +
               'BUS_READ(0x0100 | c->S++);\nc->P = (BUS_DATA() | FAM65XX_BF) & ~FAM65XX_XF;\nbreak;' +
               'BUS_READ(0x0100 | c->S);\nc->AD = BUS_DATA();\nbreak;' +
               'c->PC = (BUS_DATA() << 8) | c->AD;\n_FETCH();')
        get_or_create_continuation(seq, 'RTI')
        return ('RTI', seq)
    elif op == 0x60:  # RTS
        seq = ('BUS_READ(0x0100 | c->S++);\nbreak;' +
               'BUS_READ(0x0100 | c->S);\nc->AD = BUS_DATA();\nbreak;' +
               'c->PC = (BUS_DATA() << 8) | c->AD;\nbreak;' +
               'BUS_READ(c->PC++);\n_FETCH();')
        get_or_create_continuation(seq, 'RTS')
        return ('RTS', seq)
    elif op == 0x4C:  # JMP abs
        seq = ('BUS_READ(c->PC++);\nc->AD |= BUS_DATA() << 8;\nbreak;' +
               'c->PC = c->AD;\n_FETCH();')
        get_or_create_continuation(seq, 'JMP_ABS')
        return ('JMP_ABS', seq)
    elif op == 0x6C:  # JMP ind
        seq = ('BUS_READ(c->PC++);\nc->AD |= BUS_DATA() << 8;\nbreak;' +
               'BUS_READ(c->AD);\nbreak;' +
               'BUS_READ((c->AD & 0xFF00) | ((c->AD + 1) & 0xFF));\nc->AD = BUS_DATA();\nbreak;' +
               'c->PC = (BUS_DATA() << 8) | c->AD;\n_FETCH();')
        get_or_create_continuation(seq, 'JMP_IND')
        return ('JMP_IND', seq)
    elif op == 0x28:  # PLP
        seq = ('BUS_READ(0x0100 | c->S);\nbreak;' +
               'c->P = (BUS_DATA() | FAM65XX_BF) & ~FAM65XX_XF;\n_FETCH();')
        get_or_create_continuation(seq, 'PLP')
        return ('PLP', seq)
    elif op == 0x68:  # PLA
        seq = ('BUS_READ(0x0100 | c->S);\nbreak;' +
               'c->A = BUS_DATA();\n_NZ(c->A);\n_FETCH();')
        get_or_create_continuation(seq, 'PLA')
        return ('PLA', seq)
    elif flags == 'BRANCH':
        # Branch taken continuation
        seq = ('BUS_INTERNAL((c->PC & 0xFF00) | (c->AD & 0xFF));\nif((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\t_FETCH();\n};\nbreak;' +
               'c->PC = c->AD;\n_FETCH();')
        get_or_create_continuation(seq, 'BRANCH_TAKEN')
        return ('BRANCH_TAKEN', seq)
    elif flags == 'RMW':
        # RMW continuations
        mnemonic = get_mnemonic(op)
        rmw_seqs = {
            'ASL': ('ASL_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_asl(c, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'ROL': ('ROL_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_rol(c, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'LSR': ('LSR_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_lsr(c, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'ROR': ('ROR_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_ror(c, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'DEC': ('DEC_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD--;\n_NZ(c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'INC': ('INC_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD++;\n_NZ(c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'SLO': ('SLO_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_asl(c, c->AD);\nc->A |= c->AD;\n_NZ(c->A);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'RLA': ('RLA_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_rol(c, c->AD);\nc->A &= c->AD;\n_NZ(c->A);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'SRE': ('SRE_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_lsr(c, c->AD);\nc->A ^= c->AD;\n_NZ(c->A);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'RRA': ('RRA_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD = _fam65xx_ror(c, c->AD);\n_fam65xx_adc(c, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'DCP': ('DCP_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD--;\n_fam65xx_cmp(c, c->A, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
            'ISC': ('ISC_RMW', 'BUS_WRITE(c->AD, c->AD);\nbreak; c->AD++;\n_fam65xx_sbc(c, c->AD);\nbreak; BUS_WRITE(c->AD, c->AD);\n_FETCH();'),
        }
        if mnemonic in rmw_seqs:
            name, seq = rmw_seqs[mnemonic]
            get_or_create_continuation(seq, name)
            return rmw_seqs[mnemonic]
    
    return None

def get_branch_mask(op):
    """Get branch condition mask"""
    aaa = (op >> 5) & 7
    masks = ['FAM65XX_NF', 'FAM65XX_VF', 'FAM65XX_CF', 'FAM65XX_ZF']
    return masks[aaa - 4]

def get_branch_val(op):
    """Get branch condition value"""
    return '0' if op in [0x10, 0x50, 0x90, 0xD0] else get_branch_mask(op)

def get_continuation_constant_name(name):
    """Get the constant name for a continuation sequence"""
    return f"C_{name}"

def get_indent(level):
    """Helper function to generate indentation string"""
    return '            ' + '    ' * level

def format_code(code):
    """Format code using embedded newlines and tabs for indentation"""
    lines = code.split('\n')
    formatted_lines = []
    
    for line in lines:
        line = line.rstrip()  # Remove trailing whitespace but preserve leading tabs
        if line:  # Only add non-empty lines
            formatted_lines.append('            ' + line)
    
    return '\n'.join(formatted_lines)

def generate_opcode_implementation(op):
    """Generate the implementation code for this opcode's specific cycle"""
    impl = get_implementation(op)
    flags = get_flags(op)
    
    # Handle branch instructions specially
    if flags == 'BRANCH':
        return f"BUS_READ(c->PC);\nc->AD = c->PC + (int8_t)BUS_DATA();\nif ((c->P & {get_branch_mask(op)}) == {get_branch_val(op)}) {{\n\tc->IR = C_BRANCH_TAKEN;\n}} else {{\n\t_FETCH();\n}}"
    
    # Return the implementation from the mnemonic symbol
    if impl:
        return impl
    
    # Fallback for any unhandled cases
    return "BUS_READ(c->PC);\nc->IR--;"

def get_addr_mode_acronym(addr_mode):
    """Get acronym for addressing mode"""
    if addr_mode in ADDRESSING_MODES:
        return ADDRESSING_MODES[addr_mode][0]
    return "---"

def get_addr_mode_const_name(addr_mode):
    """Get the constant name for an addressing mode"""
    if addr_mode in ADDRESSING_MODES:
        return ADDRESSING_MODES[addr_mode][2]
    return "ADDR_NON"

def calculate_addressing_mode_offsets():
    """Calculate addressing mode offsets dynamically based on cycle counts"""
    global ADDR_MODE_INDICES, ADDR_SEQ_END, CONT_SEQ_START
    
    addr_mode_indices = {}
    current_offset = 1  # Start at offset 1 (after direct opcodes at 0)
    
    # Process addressing modes
    for addr_mode in ADDRESSING_MODES:
        addr_mode_indices[addr_mode] = current_offset
        cycle_count = len(ADDRESSING_MODES[addr_mode][3])  # cycles are at index 3
        current_offset += cycle_count
    
    # Update global variables
    ADDR_MODE_INDICES = addr_mode_indices
    ADDR_SEQ_END = ADDR_SEQ_BASE + current_offset
    CONT_SEQ_START = ADDR_SEQ_END

def get_addr_mode_cycle_count(addr_mode):
    """Get the number of cycles for an addressing mode"""
    if addr_mode in ADDRESSING_MODES:
        return len(ADDRESSING_MODES[addr_mode][3])
    return 0

def generate_addressing_constants():
    """Generate addressing mode offset constants"""
    l("// Addressing mode offset constants")
    l("// Offset 0 = direct opcode jump (no addressing mode)")
    l(f"// Other offsets use base correction of ADDR_SEQ_BASE ({ADDR_SEQ_BASE})")
    l("#define ADDR_NON     0   // Direct opcode execution (no addressing mode)")
    l(f"#define ADDR_SEQ_BASE {ADDR_SEQ_BASE}")
    
    # Generate constants for addressing modes using definitions
    for addr_mode in ADDRESSING_MODES.keys():
        const_name = get_addr_mode_const_name(addr_mode)
        offset = ADDR_MODE_INDICES[addr_mode]
        actual_index = ADDR_SEQ_BASE + offset
        l(f"#define {const_name:<12} {offset:<3} // Index {actual_index}")
    
    l("")

def generate_lookup_table():
    """Generate opcode_addr_start lookup table"""
    # Create reverse mapping from offset to constant name
    offset_to_const = {0: "ADDR_NON"}
    
    for addr_mode in ADDRESSING_MODES.keys():
        const_name = get_addr_mode_const_name(addr_mode)
        if addr_mode in ADDR_MODE_INDICES:
            offset = ADDR_MODE_INDICES[addr_mode]
            offset_to_const[offset] = const_name
    
    l("// Lookup table: addressing mode start index for each opcode")
    l("static const uint8_t opcode_addr_start[256] = {")
    
    for op in range(256):
        addr_mode = get_addr_mode(op)
        
        if addr_mode in ADDR_MODE_INDICES:
            offset = ADDR_MODE_INDICES[addr_mode]
        else:
            offset = NO_ADDR_SEQ
        
        const_name = offset_to_const.get(offset, str(offset))
        
        # Format with comma except for last element
        comma = "," if op < 255 else " "
        l(f"    {const_name}{comma}  // 0x{op:02X}: {get_mnemonic(op)}")
    
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
            addr_mode = get_addr_mode(opc)
            addr_acronym = get_addr_mode_acronym(addr_mode)
            
            # Calculate cycle number: 1 if ADDR_NON, otherwise 1 + addressing_cycles
            if addr_mode in ADDRESSING_MODES:
                cycle_num = 1 + get_addr_mode_cycle_count(addr_mode)
            else:
                cycle_num = 1
                
            l(f"        case 0x{opc:02X}:  // {get_mnemonic(opc)} {addr_acronym} cycle {cycle_num}")
            emitted.add(opc)
        
        l(format_code(code))
        l(f"            break;")
        l("")

def generate_addressing_modes():
    """Generate shared addressing mode sequences using definitions"""
    l("        // ==========================================")
    l(f"        // [256-{ADDR_SEQ_END-1}] SHARED ADDRESSING SEQUENCES")
    l("        // ==========================================")
    l("")
    
    # Generate addressing mode sequences from definitions
    for addr_mode, mode_def in ADDRESSING_MODES.items():
        acronym, long_name, const_name, cycles = mode_def
        
        # Emit long name comment before the cycles
        l(f"        // {acronym}: {long_name}")
        
        for cycle_idx, cycle_code in enumerate(cycles):
            cycle_num = cycle_idx + 1
            
            l(f"        case ADDR_SEQ_BASE + {const_name} + {cycle_idx}:  // {acronym} cycle {cycle_num}")
            l(format_code(cycle_code))
            l("            break;")
            
        l("")

def generate_continuations():
    """Generate shared multi-cycle continuation sequences"""
    l("        // ==========================================")
    l(f"        // [{CONT_SEQ_START}+] SHARED CONTINUATIONS")
    l("        // ==========================================")
    l("")
    
    # Sort by index to emit in order without gaps
    sorted_seqs = sorted(continuation_sequences.items(),
                        key=lambda x: x[1]['index'])
    
    for seq_code, seq_info in sorted_seqs:
        idx = seq_info['index']
        name = seq_info['name']
        cycles = seq_info['cycles']
        const_name = get_continuation_constant_name(name)
        
        l(f"        // {name} continuation")
        for i, cycle_code in enumerate(cycles):
            l(f"        case {const_name} + {i}:")
            l(format_code(cycle_code))
            l("            break;")
        l("")

def generate_continuation_constants():
    """Generate constant declarations for continuation sequences"""
    l("// Continuation sequence constants")
    for seq_code, seq_info in sorted(continuation_sequences.items(),
                                     key=lambda x: x[1]['index']):
        const_name = get_continuation_constant_name(seq_info['name'])
        l(f"#define {const_name:<16} {seq_info['index']}")
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
    l(f" *   [{CONT_SEQ_START}-{final_continuation_index-1}]  : Shared continuation sequences")
    l(f" * Total cases: {final_continuation_index}")
    l(" */")
    l("")
    
    generate_continuation_constants()
    generate_addressing_constants()
    generate_lookup_table()
    
    l("// Decoder switch statement")
    l("static inline uint64_t _fam65xx_decode(fam65xx_t* c, uint64_t pins) {")
    l("    switch (c->IR) {")
    l("")
    
    generate_opcode_cases()
    generate_addressing_modes()
    generate_continuations()
    
    l("    }")
    l("    c->IR++;")
    l("    return pins;")
    l("}")

if __name__ == '__main__':
    main()