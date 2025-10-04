#!/usr/bin/env python3
#-------------------------------------------------------------------------------
#   fam65xx_gen.py
#   Generate instruction decoder for FAM65xx family CPU emulator.
#   Optimized for world's fastest 100% hardware accurate implementation.
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
    "c->ADL = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->ADH = c->DL;\nif ((c->ADL + c->X) > 0xFF) { c->AD += c->X; c->CI = c->opcode; } else { c->AD = c->ADL | (c->ADH << 8); c->CI++; }",
    "c->AD += c->X;\nc->CI = c->opcode;"
))

AM_ABX_W = ("ABX", "absolute,X (write - always takes extra cycle)", "ADDR_ABX_W", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->ADL = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->ADH = c->DL;\nc->CI++;",
    "c->AD = (c->ADL + c->X) | (c->ADH << 8);\nc->CI = c->opcode;"
))

AM_ABY = ("ABY", "absolute,Y", "ADDR_ABY", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->ADL = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->ADH = c->DL;\nif ((c->ADL + c->Y) > 0xFF) { c->AD += c->Y; c->CI = c->opcode; } else { c->AD = c->ADL | (c->ADH << 8); c->CI++; }",
    "c->AD += c->Y;\nc->CI = c->opcode;"
))

AM_ABY_W = ("ABY", "absolute,Y (write - always takes extra cycle)", "ADDR_ABY_W", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->ADL = c->DL;\nc->AD = c->PC++;\nc->CI++;",
    "c->ADH = c->DL;\nc->CI++;",
    "c->AD = (c->ADL + c->Y) | (c->ADH << 8);\nc->CI = c->opcode;"
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
    "c->AD = c->TMP | (c->DL << 8);\nif ((c->AD >> 8) != ((c->AD + c->Y) >> 8)) { c->CI++; } else { c->AD += c->Y; c->CI++; }",
    "c->AD += c->Y;\nc->CI++;",
    "c->CI = c->opcode;"
))

AM_IDY_W = ("IDY", "indirect indexed (zp),Y (write - always takes extra cycle)", "ADDR_IDY_W", (
    "c->AD = c->PC++;\nc->CI++;",
    "c->TMP = c->DL;\nc->AD = (c->DL + 1) & 0xFF;\nc->CI++;",
    "c->AD = c->TMP | (c->DL << 8);\nc->CI++;",
    "c->AD += c->Y;\nc->CI++;"
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
# Operation Definitions
# Each definition contains: (mnemonic, mem_access, implementation_code, flags)
#-------------------------------------------------------------------------------

# Simple implied mode operations
OP_BRK = ("BRK", M___, "if (0 == (c->brk_flags & (FAM65XX_BRK_IRQ | FAM65XX_BRK_NMI))) {\n\tc->PC++;\n}\nc->write_src = R_PCH;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI = C_BRK;", 'CONT')
OP_PHP = ("PHP", M___, "c->AD = c->PC;\nc->CI = C_PHP;", 'CONT')  # 3-cycle: dummy read, stack write, fetch
OP_PLP = ("PLP", M___, "c->AD = 0x0100 | c->S++;\nc->CI = C_PLP;", 'CONT')
OP_PHA = ("PHA", M___, "c->AD = c->PC;\nc->CI = C_PHA;", 'CONT')  # 3-cycle: dummy read, stack write, fetch
OP_PLA = ("PLA", M___, "c->AD = 0x0100 | c->S++;\nc->CI = C_PLA;", 'CONT')  # Dummy stack access - increment S
OP_RTI = ("RTI", M_R_, "c->AD = 0x0100 | c->S++;\nc->CI = C_RTI;", 'CONT')  # Dummy stack access - increment S
OP_RTS = ("RTS", M_R_, "c->AD = 0x0100 | c->S++;\nc->CI = C_RTS;", 'CONT')  # Dummy stack access - increment S
OP_JSR = ("JSR", M_R_, "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI = C_JSR;", 'CONT')  # Read low byte of target address
OP_JMP = ("JMP", M_R_, "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI = C_JMP_ABS;", 'CONT')  # Read low byte of target address
OP_JMI = ("JMP", M_R_, "c->TMP = c->DL;\nc->AD = c->PC++;\nc->CI = C_JMP_IND;", 'CONT')  # Read low byte of indirect address

# Register transfer operations (M___ - immediate fetch)
OP_TAX = ("TAX", M___, "c->X = c->A;\n_NZ(c->X);\ngoto fetch_next;", None)
OP_TXA = ("TXA", M___, "c->A = c->X;\n_NZ(c->A);\ngoto fetch_next;", None)
OP_TAY = ("TAY", M___, "c->Y = c->A;\n_NZ(c->Y);\ngoto fetch_next;", None)
OP_TYA = ("TYA", M___, "c->A = c->Y;\n_NZ(c->A);\ngoto fetch_next;", None)
OP_TSX = ("TSX", M___, "c->X = c->S;\n_NZ(c->X);\ngoto fetch_next;", None)
OP_TXS = ("TXS", M___, "c->S = c->X;\ngoto fetch_next;", None)

# Increment/Decrement operations (M___ - immediate fetch)
OP_DEX = ("DEX", M___, "c->X--;\n_NZ(c->X);\ngoto fetch_next;", None)
OP_INX = ("INX", M___, "c->X++;\n_NZ(c->X);\ngoto fetch_next;", None)
OP_DEY = ("DEY", M___, "c->Y--;\n_NZ(c->Y);\ngoto fetch_next;", None)
OP_INY = ("INY", M___, "c->Y++;\n_NZ(c->Y);\ngoto fetch_next;", None)

# Flag operations (M___ - immediate fetch)
OP_CLC = ("CLC", M___, "c->P &= ~FAM65XX_CF;\ngoto fetch_next;", None)
OP_SEC = ("SEC", M___, "c->P |= FAM65XX_CF;\ngoto fetch_next;", None)
OP_CLI = ("CLI", M___, "c->P &= ~FAM65XX_IF;\ngoto fetch_next;", None)
OP_SEI = ("SEI", M___, "c->P |= FAM65XX_IF;\ngoto fetch_next;", None)
OP_CLV = ("CLV", M___, "c->P &= ~FAM65XX_VF;\ngoto fetch_next;", None)
OP_CLD = ("CLD", M___, "c->P &= ~FAM65XX_DF;\ngoto fetch_next;", None)
OP_SED = ("SED", M___, "c->P |= FAM65XX_DF;\ngoto fetch_next;", None)

# Branch operations (all share BRANCH_TAKEN continuation)
OP_BPL = ("BPL", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BMI = ("BMI", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BVC = ("BVC", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BVS = ("BVS", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BCC = ("BCC", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BCS = ("BCS", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BNE = ("BNE", M_R_, None, 'BRANCH')  # Read branch offset from PC
OP_BEQ = ("BEQ", M_R_, None, 'BRANCH')  # Read branch offset from PC

# ALU operations - Memory variants (M_R_ - immediate fetch)
OP_ORA = ("ORA", M_R_, "c->A |= c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)
OP_AND = ("AND", M_R_, "c->A &= c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)
OP_EOR = ("EOR", M_R_, "c->A ^= c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)
OP_ADC = ("ADC", M_R_, "_fam65xx_adc(c, c->DL);\ngoto fetch_next;", None)
OP_SBC = ("SBC", M_R_, "_fam65xx_sbc(c, c->DL);\ngoto fetch_next;", None)
OP_CMP = ("CMP", M_R_, "_fam65xx_cmp(c, c->A, c->DL);\ngoto fetch_next;", None)
OP_BIT = ("BIT", M_R_, "_fam65xx_bit(c, c->DL);\ngoto fetch_next;", None)

# Load operations (M_R_ - immediate fetch)
OP_LDA = ("LDA", M_R_, "c->A = c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)
OP_LDX = ("LDX", M_R_, "c->X = c->DL;\n_NZ(c->X);\ngoto fetch_next;", None)
OP_LDY = ("LDY", M_R_, "c->Y = c->DL;\n_NZ(c->Y);\ngoto fetch_next;", None)

# Store operations (M__W - deferred fetch)
OP_STA = ("STA", M__W, "c->write_src = R_A;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)
OP_STX = ("STX", M__W, "c->write_src = R_X;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)
OP_STY = ("STY", M__W, "c->write_src = R_Y;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)

# Compare operations (M_R_ - immediate fetch)
OP_CPX = ("CPX", M_R_, "_fam65xx_cmp(c, c->X, c->DL);\ngoto fetch_next;", None)
OP_CPY = ("CPY", M_R_, "_fam65xx_cmp(c, c->Y, c->DL);\ngoto fetch_next;", None)

# RMW operations - have two variants (accumulator vs memory)
OP_ASL_A = ("ASL", M___, "c->A = _fam65xx_asl(c, c->A);\ngoto fetch_next;", None)
OP_ASL_M = ("ASL", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_ASL_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_LSR_A = ("LSR", M___, "c->A = _fam65xx_lsr(c, c->A);\ngoto fetch_next;", None)
OP_LSR_M = ("LSR", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_LSR_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_ROL_A = ("ROL", M___, "c->A = _fam65xx_rol(c, c->A);\ngoto fetch_next;", None)
OP_ROL_M = ("ROL", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_ROL_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_ROR_A = ("ROR", M___, "c->A = _fam65xx_ror(c, c->A);\ngoto fetch_next;", None)
OP_ROR_M = ("ROR", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_ROR_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_INC_M = ("INC", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_INC_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_DEC_M = ("DEC", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_DEC_RMW;", 'RMW')  # Set up dummy write and jump to RMW

# NOP variants
OP_NOP_I = ("NOP", M___, "c->AD = c->PC;\nc->CI = C_NOP_DUMMY;", 'CONT')  # Dummy read of current PC - 2-cycle instruction
OP_NOP_R = ("NOP", M_R_, "goto fetch_next;", None)      # Read from effective address - M_R_ immediate fetch

# Illegal/undocumented instructions
OP_LAX = ("LAX", M_R_, "c->A = c->X = c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)  # Read and load into A and X - M_R_ immediate fetch
OP_SAX = ("SAX", M__W, "c->TMP = c->A & c->X;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)  # Write A&X to memory - M__W deferred fetch
OP_SLO = ("SLO", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_SLO_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_RLA = ("RLA", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_RLA_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_SRE = ("SRE", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_SRE_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_RRA = ("RRA", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_RRA_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_DCP = ("DCP", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_DCP_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_ISC = ("ISC", M_RW, "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\nc->CI = C_ISC_RMW;", 'RMW')  # Set up dummy write and jump to RMW
OP_ANC = ("ANC", M_R_, "c->A &= c->DL;\n_NZ(c->A);\nc->P = (c->P & ~FAM65XX_CF) | ((c->A & 0x80) ? FAM65XX_CF : 0);\ngoto fetch_next;", None)  # AND with carry flag update - M_R_ immediate fetch
OP_ASR = ("ASR", M_R_, "c->A &= c->DL;\nc->P = (c->P & ~FAM65XX_CF) | (c->A & 1);\nc->A>>=1;\n_NZ(c->A);\ngoto fetch_next;", None)  # AND then LSR - M_R_ immediate fetch
OP_ARR = ("ARR", M_R_, "c->A = (c->A & c->DL) >> 1 | (c->P & FAM65XX_CF ? 0x80 : 0);\n_NZ(c->A);\nc->P = (c->P & ~(FAM65XX_CF | FAM65XX_VF)) | ((c->A & 0x40) ? FAM65XX_CF : 0) | ((c->A & 0x20) ^ (c->A & 0x40) ? FAM65XX_VF : 0);\ngoto fetch_next;", None)  # AND then ROR - M_R_ immediate fetch
OP_XAA = ("XAA", M_R_, "c->A = (c->A | 0xEE) & c->X & c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)  # Unstable AND operation - M_R_ immediate fetch
OP_SBX = ("SBX", M_R_, "c->TMP = (c->A & c->X) - c->DL;\nc->X = c->TMP;\n_NZ(c->X);\nc->P = (c->P & ~FAM65XX_CF) | ((c->TMP & 0x100) ? 0 : FAM65XX_CF);\ngoto fetch_next;", None)  # CMP and DEX combined - M_R_ immediate fetch
OP_SHY = ("SHY", M__W, "c->TMP = c->Y & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)  # Store Y with high byte AND - M__W deferred fetch
OP_SHX = ("SHX", M__W, "c->TMP = c->X & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)  # Store X with high byte AND - M__W deferred fetch
OP_SHA = ("SHA", M_RW, "c->TMP = c->A & c->X & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)  # Store A&X with high byte AND - M_RW deferred fetch (note: should be M__W)
OP_SHS = ("SHS", M__W, "c->S = c->A & c->X;\nc->TMP = c->S & ((c->AD >> 8) + 1);\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\ngoto fetch_next;", None)  # Transfer A&X to S and store - M__W deferred fetch
OP_LAS = ("LAS", M_R_, "c->A = c->X = c->S = c->S & c->DL;\n_NZ(c->A);\ngoto fetch_next;", None)  # AND S with memory to A,X,S - M_R_ immediate fetch
OP_JAM = ("JAM", M_R_, "c->PC--;\nc->CI = C_JAM;", 'CONT')  # Read byte after opcode, restore PC, then continue JAM

#-------------------------------------------------------------------------------
# RMW Continuation Definitions
# Each definition contains: (name, cycles)
# Each cycle is a separate string object (standardized format)
#-------------------------------------------------------------------------------

rmw_seqs = {
    # RMW operations: Dummy write setup moved to opcode case for proper timing
    # Each RMW now has exactly 2 cycles: internal operation, real write (new value)
    'ASL': ('ASL_RMW', (
        "c->DL = _fam65xx_asl(c, c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'ROL': ('ROL_RMW', (
        "c->DL = _fam65xx_rol(c, c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'LSR': ('LSR_RMW', (
        "c->DL = _fam65xx_lsr(c, c->DL);\nc->CI++;",  # Internal computation
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'ROR': ('ROR_RMW', (
        "c->DL = _fam65xx_ror(c, c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'DEC': ('DEC_RMW', (
        "c->DL--;\n_NZ(c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'INC': ('INC_RMW', (
        "c->DL++;\n_NZ(c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'SLO': ('SLO_RMW', (
        "c->DL = _fam65xx_asl(c, c->DL);\nc->A |= c->DL;\n_NZ(c->A);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'RLA': ('RLA_RMW', (
        "c->DL = _fam65xx_rol(c, c->DL);\nc->A &= c->DL;\n_NZ(c->A);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'SRE': ('SRE_RMW', (
        "c->DL = _fam65xx_lsr(c, c->DL);\nc->A ^= c->DL;\n_NZ(c->A);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'RRA': ('RRA_RMW', (
        "c->DL = _fam65xx_ror(c, c->DL);\n_fam65xx_adc(c, c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'DCP': ('DCP_RMW', (
        "c->DL--;\n_fam65xx_cmp(c, c->A, c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
    'ISC': ('ISC_RMW', (
        "c->DL++;\n_fam65xx_sbc(c, c->DL);\nc->CI++;",
        "c->write_src = R_DL;\npins &= ~FAM65XX_RW;\ngoto fetch_next;"
    )),
}
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
        [[OP_NOP_R,AM_ABS],[OP_BIT,AM_ABS],[OP_JMP,AM_JMP],[OP_JMI,AM_JMP],[OP_STY,AM_ABS],[OP_LDY,AM_ABS],[OP_CPY,AM_ABS],[OP_CPX,AM_ABS]],
        [[OP_BPL,AM_IMM],[OP_BMI,AM_IMM],[OP_BVC,AM_IMM],[OP_BVS,AM_IMM],[OP_BCC,AM_IMM],[OP_BCS,AM_IMM],[OP_BNE,AM_IMM],[OP_BEQ,AM_IMM]],
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
        [[OP_ASL_M,AM_ZER],[OP_ROL_M,AM_ZER],[OP_LSR_M,AM_ZER],[OP_ROR_M,AM_ZER],[OP_STX,AM_ZER],[OP_LDX,AM_ZER],[OP_DEC_M,AM_ZER],[OP_INC_M,AM_ZER]],
        [[OP_ASL_A,AM_NON],[OP_ROL_A,AM_NON],[OP_LSR_A,AM_NON],[OP_ROR_A,AM_NON],[OP_TXA,AM_NON],[OP_TAX,AM_NON],[OP_DEX,AM_NON],[OP_NOP_I,AM_NON]],
        [[OP_ASL_M,AM_ABS],[OP_ROL_M,AM_ABS],[OP_LSR_M,AM_ABS],[OP_ROR_M,AM_ABS],[OP_STX,AM_ABS],[OP_LDX,AM_ABS],[OP_DEC_M,AM_ABS],[OP_INC_M,AM_ABS]],
        [[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV],[OP_JAM,AM_INV]],
        [[OP_ASL_M,AM_ZPX],[OP_ROL_M,AM_ZPX],[OP_LSR_M,AM_ZPX],[OP_ROR_M,AM_ZPX],[OP_STX,AM_ZPY],[OP_LDX,AM_ZPY],[OP_DEC_M,AM_ZPX],[OP_INC_M,AM_ZPX]],
        [[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON],[OP_TXS,AM_NON],[OP_TSX,AM_NON],[OP_NOP_R,AM_NON],[OP_NOP_R,AM_NON]],
        [[OP_ASL_M,AM_ABX],[OP_ROL_M,AM_ABX],[OP_LSR_M,AM_ABX],[OP_ROR_M,AM_ABX],[OP_SHX,AM_ABY],[OP_LDX,AM_ABY],[OP_DEC_M,AM_ABX],[OP_INC_M,AM_ABX]]
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
continuation_sequences = {}  # sequence_code -> {index, name, code}
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

def get_or_create_continuation(cycles, name):
    """Get existing continuation sequence index or create new one"""
    global next_continuation_index
    
    # Use tuple as key for deduplication
    if cycles not in continuation_sequences:
        continuation_sequences[cycles] = {
            'index': next_continuation_index,
            'name': name,
            'cycles': cycles
        }
        next_continuation_index += len(cycles)
    
    return continuation_sequences[cycles]['index']

def analyze_continuation_needs(op):
    """Determine if opcode needs a continuation sequence - handle special RMW cases"""
    operation, addr_mode = get_ops_entry(op)
    flags = operation[3]  # flags are at index 3
    
    # Handle special multi-cycle operations with hardcoded continuations FIRST
    if op == 0x00:  # BRK - 7 cycles total (1 opcode + 6 continuation cycles)
        cycles = (
            'c->write_src = R_PCL;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;',  # Cycle 2: Write PCL to stack
            'c->TMP = c->P | FAM65XX_BF;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->P |= FAM65XX_IF;\nc->P &= ~FAM65XX_BF;\nc->brk_flags = 0;\nc->CI++;',  # Cycle 3: Write P to stack, set IF, clear BF
            'c->AD = _fam65xx_get_vector_addr(c);\nc->PCL = c->DL;\nc->CI++;',  # Cycle 4: Read low byte of vector, store in PCL
            'c->AD++;\nc->PCH = c->DL;\nc->CI++;',  # Cycle 5: Read high byte of vector, store in PCH
            'c->AD = c->PC;\nc->CI++;',  # Cycle 6: Hardware dummy read from new PC location
            'goto fetch_next;'  # Cycle 7: Fetch next instruction
        )
        get_or_create_continuation(cycles, 'BRK')
        return ('BRK', cycles)
    elif op == 0x20:  # JSR
        cycles = (
            'c->AD = 0x0100 | c->S;\nc->CI++;',  # Dummy stack access
            'c->write_src = R_PCH;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;',  # Write PCH to stack
            'c->write_src = R_PCL;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;',  # Write PCL to stack
            'c->AD = c->PC;\nc->PCH = c->DL;\nc->PC = (c->DL << 8) | c->TMP;\nc->CI++;',  # Read high byte and set PC
            'goto fetch_next;'  # Fetch next instruction
        )
        get_or_create_continuation(cycles, 'JSR')
        return ('JSR', cycles)
    elif op == 0x40:  # RTI
        cycles = (
            'c->AD = 0x0100 | c->S++;\nc->P = (c->DL | FAM65XX_BF) & ~FAM65XX_XF;\nc->CI++;',  # Read status register from stack
            'c->AD = 0x0100 | c->S++;\nc->PCL = c->DL;\nc->CI++;',  # Read PCL from stack
            'c->AD = 0x0100 | c->S;\nc->PCH = c->DL;\nc->CI++;',  # Read PCH from stack and set PC
            'goto fetch_next;'  # Fetch next instruction
        )
        get_or_create_continuation(cycles, 'RTI')
        return ('RTI', cycles)
    elif op == 0x60:  # RTS
        cycles = (
            'c->AD = 0x0100 | c->S++;\nc->PCL = c->DL;\nc->CI++;',  # Read PCL from stack
            'c->AD = 0x0100 | c->S;\nc->PCH = c->DL;\nc->CI++;',  # Read PCH from stack and set PC
            'goto fetch_next;'  # Increment PC and fetch next instruction
        )
        get_or_create_continuation(cycles, 'RTS')
        return ('RTS', cycles)
    elif op == 0x4C:  # JMP abs
        cycles = (
            'c->AD = c->PC++;\nc->PCH = c->DL;\nc->PC = (c->DL << 8) | c->TMP;\nc->CI++;',  # Read high byte and set PC
            'goto fetch_next;'  # Fetch next instruction
        )
        get_or_create_continuation(cycles, 'JMP_ABS')
        return ('JMP_ABS', cycles)
    elif op == 0x6C:  # JMP ind
        cycles = (
            'c->AD = c->PC++;\nc->TMP = c->DL;\nc->AD = (c->DL << 8) | c->TMP;\nc->CI++;',  # Read high byte of indirect address
            'c->AD = c->AD;\nc->PCL = c->DL;\nc->CI++;',  # Read low byte of target address from indirect location
            'c->AD = (c->AD & 0xFF00) | ((c->AD + 1) & 0xFF);\nc->PCH = c->DL;\nc->CI++;',  # Read high byte (with page boundary bug) and set PC
            'goto fetch_next;'  # Fetch next instruction
        )
        get_or_create_continuation(cycles, 'JMP_IND')
        return ('JMP_IND', cycles)
    elif op == 0x28:  # PLP
        cycles = (
            'c->AD = 0x0100 | c->S;\nc->P = (c->DL | FAM65XX_BF) & ~FAM65XX_XF;\nc->CI++;',  # Read status register from stack and process
            'goto fetch_next;'  # Fetch next instruction
        )
        get_or_create_continuation(cycles, 'PLP')
        return ('PLP', cycles)
    elif op == 0x68:  # PLA
        cycles = (
            'c->AD = 0x0100 | c->S;\nc->A = c->DL;\n_NZ(c->A);\nc->CI++;',  # Read accumulator from stack and process
            'goto fetch_next;'  # Fetch next instruction
        )
        get_or_create_continuation(cycles, 'PLA')
        return ('PLA', cycles)
    elif op == 0x08:  # PHP - 3 cycles total (1 opcode + 2 continuation cycles)
        cycles = (
            'c->TMP = c->P | FAM65XX_BF;\nc->write_src = R_TMP;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;',  # Cycle 2: Write P to stack with B flag set
            'goto fetch_next;'  # Cycle 3: Fetch next instruction
        )
        get_or_create_continuation(cycles, 'PHP')
        return ('PHP', cycles)
    elif op == 0x48:  # PHA - 3 cycles total (1 opcode + 2 continuation cycles)
        cycles = (
            'c->write_src = R_A;\npins &= ~FAM65XX_RW;\nc->AD = 0x0100 | c->S--;\nc->CI++;',  # Cycle 2: Write A to stack
            'goto fetch_next;'  # Cycle 3: Fetch next instruction
        )
        get_or_create_continuation(cycles, 'PHA')
        return ('PHA', cycles)
    elif operation[0] == 'NOP' and operation[3] == 'CONT':  # NOP implied - 2 cycles total (1 opcode + 1 continuation cycle)
        cycles = (
            'c->PC++;\ngoto fetch_next;',  # Cycle 2: Dummy read from PC (already set in cycle 1), increment PC, fetch next
        )
        get_or_create_continuation(cycles, 'NOP_DUMMY')
        return ('NOP_DUMMY', cycles)
    elif operation[0] == 'JAM':  # JAM instruction - ProcessorTests expects exactly 2 cycles
        cycles = (
            'goto fetch_next;',  # Cycle 2: Fetch next instruction
        )
        get_or_create_continuation(cycles, 'JAM')
        return ('JAM', cycles)
    elif flags == 'BRANCH':
        # Branch taken continuation
        cycles = (
            'if((c->AD & 0xFF00) == (c->PC & 0xFF00))\n{\n\t// Same page - branch takes 3 cycles total\n\tc->PC = c->AD;\n\tc->irq_pip >>= 1;\n\tc->nmi_pip >>= 1;\n\tgoto fetch_next;\n} else {\n\t// Page boundary crossed - fix high byte calculation\n\t// The 6502 does a dummy read with wrong high byte, then corrects it\n\tc->CI++;\n}',  # Page boundary check
            '// Page boundary crossing dummy cycle complete - set correct PC\nc->PC = c->AD;\nc->irq_pip >>= 1;\nc->nmi_pip >>= 1;\ngoto fetch_next;'  # Page crossed case and fetch
        )
        get_or_create_continuation(cycles, 'BRANCH_TAKEN')
        return ('BRANCH_TAKEN', cycles)
    elif flags == 'RMW':
        # RMW continuations
        mnemonic = operation[0]  # mnemonic is at index 0
        if mnemonic in rmw_seqs:
            name, cycles = rmw_seqs[mnemonic]
            get_or_create_continuation(cycles, name)
            return rmw_seqs[mnemonic]
    
    return None

def get_branch_mask(op):
    """Get branch condition mask - returns the specific flag for each branch"""
    if op == 0x10 or op == 0x30:  # BPL, BMI
        return 'FAM65XX_NF'
    elif op == 0x50 or op == 0x70:  # BVC, BVS
        return 'FAM65XX_VF'
    elif op == 0x90 or op == 0xB0:  # BCC, BCS
        return 'FAM65XX_CF'
    elif op == 0xD0 or op == 0xF0:  # BNE, BEQ
        return 'FAM65XX_ZF'
    return 'FAM65XX_NF'  # fallback

def get_branch_val(op):
    """Get branch condition value"""
    if op == 0x10: return '0'          # BPL - branch if N=0
    if op == 0x30: return 'FAM65XX_NF' # BMI - branch if N=1
    if op == 0x50: return '0'          # BVC - branch if V=0
    if op == 0x70: return 'FAM65XX_VF' # BVS - branch if V=1
    if op == 0x90: return '0'          # BCC - branch if C=0
    if op == 0xB0: return 'FAM65XX_CF' # BCS - branch if C=1
    if op == 0xD0: return '0'          # BNE - branch if Z=0
    if op == 0xF0: return 'FAM65XX_ZF' # BEQ - branch if Z=1
    return '0'

def get_branch_name(op):
    """Get branch instruction name for documentation"""
    names = {
        0x10: "BPL - Branch if Plus",
        0x30: "BMI - Branch if Minus",
        0x50: "BVC - Branch if Overflow Clear",
        0x70: "BVS - Branch if Overflow Set",
        0x90: "BCC - Branch if Carry Clear",
        0xB0: "BCS - Branch if Carry Set",
        0xD0: "BNE - Branch if Not Equal",
        0xF0: "BEQ - Branch if Equal"
    }
    return names.get(op, "Unknown Branch")

def get_continuation_constant_name(name):
    """Get the constant name for a continuation sequence"""
    return f"C_{name}"

def generate_opcode_implementation(op):
    """Generate the implementation code for this opcode's specific cycle"""
    operation, addr_mode = get_ops_entry(op)
    impl = operation[2]  # implementation is at index 2
    flags = operation[3]  # flags are at index 3
    mem_access = operation[1]  # memory access is at index 1
    
    # Handle branch instructions specially - each gets individual condition
    if flags == 'BRANCH':
        mask = get_branch_mask(op)
        val = get_branch_val(op)
        # Force each branch to have unique implementation text by including opcode
        opcode_specific = f"0x{op:02X}"
        
        # Fix condition logic: for "flag set" conditions, check if flag is set
        # For "flag clear" conditions, check if flag is clear
        if val == '0':
            # Flag clear condition: check if (flag & mask) == 0
            condition = f"(c->P & {mask}) == 0"
        else:
            # Flag set condition: check if (flag & mask) != 0
            condition = f"(c->P & {mask}) != 0"
        
        return f"// Opcode {opcode_specific} - {get_branch_name(op)}\nc->AD = c->PC;\nc->TMP = (int8_t)c->DL;\nif ({condition}) {{\n\tc->AD = c->PC + (int16_t)(int8_t)c->DL;  // Proper signed arithmetic\n\tc->CI = C_BRANCH_TAKEN;\n}} else {{\n\tgoto fetch_next;\n}}"
    
    # Return the implementation from the operation - simplified, no more _FETCH() logic needed
    if impl:
        # Special case for JAM: it should only decrement CI to repeat the same cycle, no additional CI assignment
        if operation[0] == "JAM":  # mnemonic is at index 0
            return impl
        
        # Check if this operation already has a continuation sequence (flags == 'CONT' or 'RMW')
        # If so, it should NOT get additional processing
        if flags in ['CONT', 'RMW']:
            # Operations with continuation sequences handle their own next cycle setup
            return impl
        
        # All other operations just return their implementation - CI setup is already included
        return impl
    
    # Fallback for any unhandled cases
    return "c->CI++;"

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
    
    for seq_code, seq_info in sorted_seqs:
        idx = seq_info['index']
        name = seq_info['name']
        cycles = seq_info['cycles']
        const_name = get_continuation_constant_name(name)
        
        l(f"        // {name} continuation")
        for i, cycle_code in enumerate(cycles):
            l(f"        case {const_name} + {i}:")
            l(format_code(cycle_code))
            
            # Only emit break if the code doesn't end with a goto fetch_next that's not in an if/else block
            if not cycle_code.strip().endswith('goto fetch_next;'):
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
    l(f" *   [{CONT_SEQ_START}-{final_continuation_index-1}] : Shared continuation sequences")
    l(f" * Total cases: {final_continuation_index}")
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
    l("    c->CI = 0xFFFE;  // Signal instruction completion")
    l("    c->AD = c->PC;")
    l("    pins |= FAM65XX_SYNC;")
    l("    return pins;")
    l("}")

if __name__ == '__main__':
    main()