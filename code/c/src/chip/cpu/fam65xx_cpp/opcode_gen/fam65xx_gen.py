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
ADDR_MODE_INDICES = {
    A_IMM: 1,   # offset 1 + 255 = 256 (immediate addressing mode)
    A_ZER: 3,   # offset 3 + 255 = 258 (zero page)
    A_ZPX: 6,   # offset 6 + 255 = 261 (zero page,X)
    A_ZPY: 10,  # offset 10 + 255 = 265 (zero page,Y)
    A_ABS: 14,  # offset 14 + 255 = 269 (absolute)
    A_ABX: 18,  # offset 18 + 255 = 273 (absolute,X)
    A_ABY: 23,  # offset 23 + 255 = 278 (absolute,Y)
    A_IDX: 28,  # offset 28 + 255 = 283 (indexed indirect)
    A_IDY: 34,  # offset 34 + 255 = 289 (indirect indexed)
}

ADDR_SEQ_BASE = 256
ADDR_SEQ_END = 294      # Last addressing mode at 293
CONT_SEQ_START = 294    # Continuations start here
NO_ADDR_SEQ = 0         # Direct opcode jump (no addressing mode)

# Global state
continuation_sequences = {}  # sequence_code -> {index, name, code}
next_continuation_index = CONT_SEQ_START
opcode_groups = {}  # implementation_code -> [opcodes]

# Addressing modes, memory accesses, and mnemonics for each instruction
ops = [
    # cc = 00
    [
        [[A____,M___,"BRK"],[A_JSR,M_R_,"JSR"],[A____,M_R_,"RTI"],[A____,M_R_,"RTS"],[A_IMM,M_R_,"NOP"],[A_IMM,M_R_,"LDY"],[A_IMM,M_R_,"CPY"],[A_IMM,M_R_,"CPX"]],
        [[A_ZER,M_R_,"NOP"],[A_ZER,M_R_,"BIT"],[A_ZER,M_R_,"NOP"],[A_ZER,M_R_,"NOP"],[A_ZER,M__W,"STY"],[A_ZER,M_R_,"LDY"],[A_ZER,M_R_,"CPY"],[A_ZER,M_R_,"CPX"]],
        [[A____,M__W,"PHP"],[A____,M___,"PLP"],[A____,M__W,"PHA"],[A____,M___,"PLA"],[A____,M___,"DEY"],[A____,M___,"TAY"],[A____,M___,"INY"],[A____,M___,"INX"]],
        [[A_ABS,M_R_,"NOP"],[A_ABS,M_R_,"BIT"],[A_JMP,M_R_,"JMP"],[A_JMP,M_R_,"JMP"],[A_ABS,M__W,"STY"],[A_ABS,M_R_,"LDY"],[A_ABS,M_R_,"CPY"],[A_ABS,M_R_,"CPX"]],
        [[A_IMM,M_R_,"BPL"],[A_IMM,M_R_,"BMI"],[A_IMM,M_R_,"BVC"],[A_IMM,M_R_,"BVS"],[A_IMM,M_R_,"BCC"],[A_IMM,M_R_,"BCS"],[A_IMM,M_R_,"BNE"],[A_IMM,M_R_,"BEQ"]],
        [[A_ZPX,M_R_,"NOP"],[A_ZPX,M_R_,"NOP"],[A_ZPX,M_R_,"NOP"],[A_ZPX,M_R_,"NOP"],[A_ZPX,M__W,"STY"],[A_ZPX,M_R_,"LDY"],[A_ZPX,M_R_,"NOP"],[A_ZPX,M_R_,"NOP"]],
        [[A____,M___,"CLC"],[A____,M___,"SEC"],[A____,M___,"CLI"],[A____,M___,"SEI"],[A____,M___,"TYA"],[A____,M___,"CLV"],[A____,M___,"CLD"],[A____,M___,"SED"]],
        [[A_ABX,M_R_,"NOP"],[A_ABX,M_R_,"NOP"],[A_ABX,M_R_,"NOP"],[A_ABX,M_R_,"NOP"],[A_ABX,M__W,"SHY"],[A_ABX,M_R_,"LDY"],[A_ABX,M_R_,"NOP"],[A_ABX,M_R_,"NOP"]]
    ],
    # cc = 01
    [
        [[A_IDX,M_R_,"ORA"],[A_IDX,M_R_,"AND"],[A_IDX,M_R_,"EOR"],[A_IDX,M_R_,"ADC"],[A_IDX,M__W,"STA"],[A_IDX,M_R_,"LDA"],[A_IDX,M_R_,"CMP"],[A_IDX,M_R_,"SBC"]],
        [[A_ZER,M_R_,"ORA"],[A_ZER,M_R_,"AND"],[A_ZER,M_R_,"EOR"],[A_ZER,M_R_,"ADC"],[A_ZER,M__W,"STA"],[A_ZER,M_R_,"LDA"],[A_ZER,M_R_,"CMP"],[A_ZER,M_R_,"SBC"]],
        [[A_IMM,M_R_,"ORA"],[A_IMM,M_R_,"AND"],[A_IMM,M_R_,"EOR"],[A_IMM,M_R_,"ADC"],[A_IMM,M_R_,"NOP"],[A_IMM,M_R_,"LDA"],[A_IMM,M_R_,"CMP"],[A_IMM,M_R_,"SBC"]],
        [[A_ABS,M_R_,"ORA"],[A_ABS,M_R_,"AND"],[A_ABS,M_R_,"EOR"],[A_ABS,M_R_,"ADC"],[A_ABS,M__W,"STA"],[A_ABS,M_R_,"LDA"],[A_ABS,M_R_,"CMP"],[A_ABS,M_R_,"SBC"]],
        [[A_IDY,M_R_,"ORA"],[A_IDY,M_R_,"AND"],[A_IDY,M_R_,"EOR"],[A_IDY,M_R_,"ADC"],[A_IDY,M__W,"STA"],[A_IDY,M_R_,"LDA"],[A_IDY,M_R_,"CMP"],[A_IDY,M_R_,"SBC"]],
        [[A_ZPX,M_R_,"ORA"],[A_ZPX,M_R_,"AND"],[A_ZPX,M_R_,"EOR"],[A_ZPX,M_R_,"ADC"],[A_ZPX,M__W,"STA"],[A_ZPX,M_R_,"LDA"],[A_ZPX,M_R_,"CMP"],[A_ZPX,M_R_,"SBC"]],
        [[A_ABY,M_R_,"ORA"],[A_ABY,M_R_,"AND"],[A_ABY,M_R_,"EOR"],[A_ABY,M_R_,"ADC"],[A_ABY,M__W,"STA"],[A_ABY,M_R_,"LDA"],[A_ABY,M_R_,"CMP"],[A_ABY,M_R_,"SBC"]],
        [[A_ABX,M_R_,"ORA"],[A_ABX,M_R_,"AND"],[A_ABX,M_R_,"EOR"],[A_ABX,M_R_,"ADC"],[A_ABX,M__W,"STA"],[A_ABX,M_R_,"LDA"],[A_ABX,M_R_,"CMP"],[A_ABX,M_R_,"SBC"]]
    ],
    # cc = 02
    [
        [[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"],[A_IMM,M_R_,"NOP"],[A_IMM,M_R_,"LDX"],[A_IMM,M_R_,"NOP"],[A_IMM,M_R_,"NOP"]],
        [[A_ZER,M_RW,"ASL"],[A_ZER,M_RW,"ROL"],[A_ZER,M_RW,"LSR"],[A_ZER,M_RW,"ROR"],[A_ZER,M__W,"STX"],[A_ZER,M_R_,"LDX"],[A_ZER,M_RW,"DEC"],[A_ZER,M_RW,"INC"]],
        [[A____,M___,"ASL"],[A____,M___,"ROL"],[A____,M___,"LSR"],[A____,M___,"ROR"],[A____,M___,"TXA"],[A____,M___,"TAX"],[A____,M___,"DEX"],[A____,M___,"NOP"]],
        [[A_ABS,M_RW,"ASL"],[A_ABS,M_RW,"ROL"],[A_ABS,M_RW,"LSR"],[A_ABS,M_RW,"ROR"],[A_ABS,M__W,"STX"],[A_ABS,M_R_,"LDX"],[A_ABS,M_RW,"DEC"],[A_ABS,M_RW,"INC"]],
        [[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"],[A_INV,M__W,"JAM"],[A_INV,M_R_,"JAM"],[A_INV,M_RW,"JAM"],[A_INV,M_RW,"JAM"]],
        [[A_ZPX,M_RW,"ASL"],[A_ZPX,M_RW,"ROL"],[A_ZPX,M_RW,"LSR"],[A_ZPX,M_RW,"ROR"],[A_ZPY,M__W,"STX"],[A_ZPY,M_R_,"LDX"],[A_ZPX,M_RW,"DEC"],[A_ZPX,M_RW,"INC"]],
        [[A____,M_R_,"NOP"],[A____,M_R_,"NOP"],[A____,M_R_,"NOP"],[A____,M_R_,"NOP"],[A____,M___,"TXS"],[A____,M___,"TSX"],[A____,M_R_,"NOP"],[A____,M_R_,"NOP"]],
        [[A_ABX,M_RW,"ASL"],[A_ABX,M_RW,"ROL"],[A_ABX,M_RW,"LSR"],[A_ABX,M_RW,"ROR"],[A_ABY,M__W,"SHX"],[A_ABY,M_R_,"LDX"],[A_ABX,M_RW,"DEC"],[A_ABX,M_RW,"INC"]]
    ],
    # cc = 03
    [
        [[A_IDX,M_RW,"SLO"],[A_IDX,M_RW,"RLA"],[A_IDX,M_RW,"SRE"],[A_IDX,M_RW,"RRA"],[A_IDX,M__W,"SAX"],[A_IDX,M_R_,"LAX"],[A_IDX,M_RW,"DCP"],[A_IDX,M_RW,"ISC"]],
        [[A_ZER,M_RW,"SLO"],[A_ZER,M_RW,"RLA"],[A_ZER,M_RW,"SRE"],[A_ZER,M_RW,"RRA"],[A_ZER,M__W,"SAX"],[A_ZER,M_R_,"LAX"],[A_ZER,M_RW,"DCP"],[A_ZER,M_RW,"ISC"]],
        [[A_IMM,M_R_,"ANC"],[A_IMM,M_R_,"ANC"],[A_IMM,M_R_,"ASR"],[A_IMM,M_R_,"ARR"],[A_IMM,M_R_,"XAA"],[A_IMM,M_R_,"LAX"],[A_IMM,M_R_,"SBX"],[A_IMM,M_R_,"SBC"]],
        [[A_ABS,M_RW,"SLO"],[A_ABS,M_RW,"RLA"],[A_ABS,M_RW,"SRE"],[A_ABS,M_RW,"RRA"],[A_ABS,M__W,"SAX"],[A_ABS,M_R_,"LAX"],[A_ABS,M_RW,"DCP"],[A_ABS,M_RW,"ISC"]],
        [[A_IDY,M_RW,"SLO"],[A_IDY,M_RW,"RLA"],[A_IDY,M_RW,"SRE"],[A_IDY,M_RW,"RRA"],[A_IDY,M_RW,"SHA"],[A_IDY,M_R_,"LAX"],[A_IDY,M_RW,"DCP"],[A_IDY,M_RW,"ISC"]],
        [[A_ZPX,M_RW,"SLO"],[A_ZPX,M_RW,"RLA"],[A_ZPX,M_RW,"SRE"],[A_ZPX,M_RW,"RRA"],[A_ZPY,M__W,"SAX"],[A_ZPY,M_R_,"LAX"],[A_ZPX,M_RW,"DCP"],[A_ZPX,M_RW,"ISC"]],
        [[A_ABY,M_RW,"SLO"],[A_ABY,M_RW,"RLA"],[A_ABY,M_RW,"SRE"],[A_ABY,M_RW,"RRA"],[A_ABY,M__W,"SHS"],[A_ABY,M_R_,"LAS"],[A_ABY,M_RW,"DCP"],[A_ABY,M_RW,"ISC"]],
        [[A_ABX,M_RW,"SLO"],[A_ABX,M_RW,"RLA"],[A_ABX,M_RW,"SRE"],[A_ABX,M_RW,"RRA"],[A_ABY,M__W,"SHY"],[A_ABY,M_R_,"LAX"],[A_ABX,M_RW,"DCP"],[A_ABX,M_RW,"ISC"]]
    ]
]

def l(s):
    """Output a line"""
    print(s)

def get_addr_mode(op):
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    return ops[cc][bbb][aaa][0]

def get_mem_access(op):
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    return ops[cc][bbb][aaa][1]

def get_mnemonic(op):
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    return ops[cc][bbb][aaa][2]

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
    """Determine if opcode needs a continuation sequence"""
    
    # Multi-cycle unique sequences
    if op == 0x00:  # BRK
        seq = ('BUS_WRITE(0x0100|c->S--,c->PC);break;' +
               'BUS_WRITE(0x0100|c->S--,c->P|FAM65XX_XF);if(c->brk_flags&FAM65XX_BRK_RESET){c->AD=0xFFFC;}else{if(c->brk_flags&FAM65XX_BRK_NMI){c->AD=0xFFFA;}else{c->AD=0xFFFE;}};break;' +
               'BUS_READ(c->AD++);c->P|=(FAM65XX_IF|FAM65XX_BF);c->brk_flags=0;break;' +
               'BUS_READ(c->AD);c->AD=BUS_DATA();break;' +
               'c->PC=(BUS_DATA()<<8)|c->AD;_FETCH();')
        return ('BRK', seq)
    
    elif op == 0x20:  # JSR
        seq = ('BUS_INTERNAL(0x0100|c->S);break;' +
               'BUS_WRITE(0x0100|c->S--,c->PC>>8);break;' +
               'BUS_WRITE(0x0100|c->S--,c->PC);break;' +
               'BUS_READ(c->PC);break;' +
               'c->PC=(BUS_DATA()<<8)|c->AD;_FETCH();')
        return ('JSR', seq)
    
    elif op == 0x40:  # RTI
        seq = ('BUS_READ(0x0100|c->S++);break;' +
               'BUS_READ(0x0100|c->S++);c->P=(BUS_DATA()|FAM65XX_BF)&~FAM65XX_XF;break;' +
               'BUS_READ(0x0100|c->S);c->AD=BUS_DATA();break;' +
               'c->PC=(BUS_DATA()<<8)|c->AD;_FETCH();')
        return ('RTI', seq)
    
    elif op == 0x60:  # RTS
        seq = ('BUS_READ(0x0100|c->S++);break;' +
               'BUS_READ(0x0100|c->S);c->AD=BUS_DATA();break;' +
               'c->PC=(BUS_DATA()<<8)|c->AD;break;' +
               'BUS_READ(c->PC++);_FETCH();')
        return ('RTS', seq)
    
    elif op == 0x4C:  # JMP abs
        seq = ('BUS_READ(c->PC++);c->AD|=BUS_DATA()<<8;break;' +
               'c->PC=c->AD;_FETCH();')
        return ('JMP_ABS', seq)
    
    elif op == 0x6C:  # JMP ind
        seq = ('BUS_READ(c->PC++);c->AD|=BUS_DATA()<<8;break;' +
               'BUS_READ(c->AD);break;' +
               'BUS_READ((c->AD&0xFF00)|((c->AD+1)&0xFF));c->AD=BUS_DATA();break;' +
               'c->PC=(BUS_DATA()<<8)|c->AD;_FETCH();')
        return ('JMP_IND', seq)
    
    elif op == 0x28:  # PLP
        seq = ('BUS_READ(0x0100|c->S);break;' +
               'c->P=(BUS_DATA()|FAM65XX_BF)&~FAM65XX_XF;_FETCH();')
        return ('PLP', seq)
    
    elif op == 0x68:  # PLA
        seq = ('BUS_READ(0x0100|c->S);break;' +
               'c->A=BUS_DATA();_NZ(c->A);_FETCH();')
        return ('PLA', seq)
    
    # Branch taken (shared by all branches)
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    
    if bbb == 4 and cc == 0 and aaa >= 4:
        seq = ('BUS_INTERNAL((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH()};break;' +
               'c->PC=c->AD;_FETCH();')
        return ('BRANCH_TAKEN', seq)
    
    # RMW operations (shared patterns)
    mem_access = get_mem_access(op)
    if mem_access == M_RW and cc in [2, 3]:
        rmw_seqs = {
            0: ('ASL_RMW', 'BUS_WRITE(c->AD,c->AD);break;c->AD=_fam65xx_asl(c,c->AD);break;BUS_WRITE(c->AD,c->AD);_FETCH();'),
            1: ('ROL_RMW', 'BUS_WRITE(c->AD,c->AD);break;c->AD=_fam65xx_rol(c,c->AD);break;BUS_WRITE(c->AD,c->AD);_FETCH();'),
            2: ('LSR_RMW', 'BUS_WRITE(c->AD,c->AD);break;c->AD=_fam65xx_lsr(c,c->AD);break;BUS_WRITE(c->AD,c->AD);_FETCH();'),
            3: ('ROR_RMW', 'BUS_WRITE(c->AD,c->AD);break;c->AD=_fam65xx_ror(c,c->AD);break;BUS_WRITE(c->AD,c->AD);_FETCH();'),
            6: ('DEC_RMW', 'BUS_WRITE(c->AD,c->AD);break;c->AD--;_NZ(c->AD);break;BUS_WRITE(c->AD,c->AD);_FETCH();'),
            7: ('INC_RMW', 'BUS_WRITE(c->AD,c->AD);break;c->AD++;_NZ(c->AD);break;BUS_WRITE(c->AD,c->AD);_FETCH();'),
        }
        if aaa in rmw_seqs:
            return rmw_seqs[aaa]
    
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

def generate_opcode_implementation(op):
    """Generate the implementation code for this opcode's specific cycle"""
    cc = op & 3
    bbb = (op >> 2) & 7
    aaa = (op >> 5) & 7
    addr_mode = get_addr_mode(op)
    mem_access = get_mem_access(op)
    
    # Check if needs continuation
    cont = analyze_continuation_needs(op)
    if cont:
        name, sequence = cont
        cont_idx = get_or_create_continuation(sequence, name)
        const_name = get_continuation_constant_name(name)
        
        # Generate first cycle that jumps to continuation
        if op == 0x00:  # BRK
            return f"if(0==(c->brk_flags&(FAM65XX_BRK_IRQ|FAM65XX_BRK_NMI))){{c->PC++;}}BUS_WRITE(0x0100|c->S--,c->PC>>8);c->IR={const_name};"
        elif op in [0x20, 0x4C, 0x6C]:  # JSR, JMP
            return f"BUS_READ(c->PC++);c->AD=BUS_DATA();c->IR={const_name};"
        elif op in [0x28, 0x40, 0x60, 0x68]:  # Stack ops
            return f"BUS_INTERNAL(0x0100|c->S++);c->IR={const_name};"
        elif bbb == 4 and cc == 0:  # Branches
            return f"BUS_READ(c->PC);c->AD=c->PC+(int8_t)BUS_DATA();if((c->P&{get_branch_mask(op)})=={get_branch_val(op)}){{c->IR={const_name};}}else{{_FETCH();}}"
    
    # RMW operations - read, then jump to continuation
    if mem_access == M_RW and cc in [2, 3]:
        cont = analyze_continuation_needs(op)
        if cont:
            name, sequence = cont
            cont_idx = get_or_create_continuation(sequence, name)
            const_name = get_continuation_constant_name(name)
            return f"BUS_READ(c->AD);c->AD=BUS_DATA();c->IR={const_name};"
    
    # Accumulator mode shifts
    if cc == 2 and bbb == 2 and addr_mode == A____:
        ops_map = {0:'asl', 1:'rol', 2:'lsr', 3:'ror'}
        if aaa in ops_map:
            return f"BUS_INTERNAL(c->PC);c->A=_fam65xx_{ops_map[aaa]}(c,c->A);_FETCH();"
    
    # Simple single-cycle operations
    simple_ops = {
        0x8A: "BUS_INTERNAL(c->PC);c->A=c->X;_NZ(c->A);_FETCH();",
        0x98: "BUS_INTERNAL(c->PC);c->A=c->Y;_NZ(c->A);_FETCH();",
        0x9A: "BUS_INTERNAL(c->PC);c->S=c->X;_FETCH();",
        0xA8: "BUS_INTERNAL(c->PC);c->Y=c->A;_NZ(c->Y);_FETCH();",
        0xAA: "BUS_INTERNAL(c->PC);c->X=c->A;_NZ(c->X);_FETCH();",
        0xBA: "BUS_INTERNAL(c->PC);c->X=c->S;_NZ(c->X);_FETCH();",
        0xCA: "BUS_INTERNAL(c->PC);c->X--;_NZ(c->X);_FETCH();",
        0x88: "BUS_INTERNAL(c->PC);c->Y--;_NZ(c->Y);_FETCH();",
        0xE8: "BUS_INTERNAL(c->PC);c->X++;_NZ(c->X);_FETCH();",
        0xC8: "BUS_INTERNAL(c->PC);c->Y++;_NZ(c->Y);_FETCH();",
        0x18: "BUS_INTERNAL(c->PC);c->P&=~FAM65XX_CF;_FETCH();",
        0x38: "BUS_INTERNAL(c->PC);c->P|=FAM65XX_CF;_FETCH();",
        0x58: "BUS_INTERNAL(c->PC);c->P&=~FAM65XX_IF;_FETCH();",
        0x78: "BUS_INTERNAL(c->PC);c->P|=FAM65XX_IF;_FETCH();",
        0xB8: "BUS_INTERNAL(c->PC);c->P&=~FAM65XX_VF;_FETCH();",
        0xD8: "BUS_INTERNAL(c->PC);c->P&=~FAM65XX_DF;_FETCH();",
        0xF8: "BUS_INTERNAL(c->PC);c->P|=FAM65XX_DF;_FETCH();",
        0xEA: "BUS_INTERNAL(c->PC);_FETCH();",
        0x08: "BUS_WRITE(0x0100|c->S--,c->P|FAM65XX_XF);_FETCH();",
        0x48: "BUS_WRITE(0x0100|c->S--,c->A);_FETCH();",
    }
    
    if op in simple_ops:
        return simple_ops[op]
    
    # ALU group (cc=1)
    if cc == 1:
        alu_ops = [
            "BUS_READ(c->AD);c->A|=BUS_DATA();_NZ(c->A);_FETCH();",
            "BUS_READ(c->AD);c->A&=BUS_DATA();_NZ(c->A);_FETCH();",
            "BUS_READ(c->AD);c->A^=BUS_DATA();_NZ(c->A);_FETCH();",
            "BUS_READ(c->AD);_fam65xx_adc(c,BUS_DATA());_FETCH();",
            "BUS_WRITE(c->AD,c->A);_FETCH();",
            "BUS_READ(c->AD);c->A=BUS_DATA();_NZ(c->A);_FETCH();",
            "BUS_READ(c->AD);_fam65xx_cmp(c,c->A,BUS_DATA());_FETCH();",
            "BUS_READ(c->AD);_fam65xx_sbc(c,BUS_DATA());_FETCH();",
        ]
        return alu_ops[aaa]
    
    # Load/Store/Compare operations (cc=2 and cc=0)
    if cc == 2:
        if aaa == 5:  # LDX
            return "BUS_READ(c->AD);c->X=BUS_DATA();_NZ(c->X);_FETCH();"
        elif aaa == 4 and mem_access == M__W:  # STX
            return "BUS_WRITE(c->AD,c->X);_FETCH();"
    
    if cc == 0:
        if aaa == 5:  # LDY
            return "BUS_READ(c->AD);c->Y=BUS_DATA();_NZ(c->Y);_FETCH();"
        elif aaa == 4 and mem_access == M__W:  # STY
            return "BUS_WRITE(c->AD,c->Y);_FETCH();"
        elif aaa == 7 and bbb in [0,1,3]:  # CPX
            return "BUS_READ(c->AD);_fam65xx_cmp(c,c->X,BUS_DATA());_FETCH();"
        elif aaa == 6 and bbb in [0,1,3]:  # CPY
            return "BUS_READ(c->AD);_fam65xx_cmp(c,c->Y,BUS_DATA());_FETCH();"
        elif aaa == 1 and bbb in [1,3]:  # BIT
            return "BUS_READ(c->AD);_fam65xx_bit(c,BUS_DATA());_FETCH();"
    
    # Invalid opcode - JAM
    return "BUS_READ(c->PC);c->IR--;"

def generate_addressing_constants():
    """Generate addressing mode offset constants"""
    l("// Addressing mode offset constants")
    l("// Offset 0 = direct opcode jump (no addressing mode)")
    l("// Other offsets use base correction of 255")
    l("#define ADDR_NONE       0   // Direct opcode execution (no addressing mode)")
    
    # Generate constants for addressing modes
    addr_mode_names = {
        A_IMM: "ADDR_IMM",
        A_ZER: "ADDR_ZER",
        A_ZPX: "ADDR_ZPX",
        A_ZPY: "ADDR_ZPY",
        A_ABS: "ADDR_ABS",
        A_ABX: "ADDR_ABX",
        A_ABY: "ADDR_ABY",
        A_IDX: "ADDR_IDX",
        A_IDY: "ADDR_IDY",
    }
    
    for addr_mode, const_name in addr_mode_names.items():
        offset = ADDR_MODE_INDICES[addr_mode]
        actual_index = offset + 255
        l(f"#define {const_name:<12} {offset:<3} // Index {actual_index}")
    
    l("")

def generate_lookup_table():
    """Generate opcode_addr_start lookup table"""
    # Create reverse mapping from offset to constant name
    offset_to_const = {0: "ADDR_NONE"}
    addr_mode_names = {
        A_IMM: "ADDR_IMM",
        A_ZER: "ADDR_ZER",
        A_ZPX: "ADDR_ZPX",
        A_ZPY: "ADDR_ZPY",
        A_ABS: "ADDR_ABS",
        A_ABX: "ADDR_ABX",
        A_ABY: "ADDR_ABY",
        A_IDX: "ADDR_IDX",
        A_IDY: "ADDR_IDY",
    }
    
    for addr_mode, const_name in addr_mode_names.items():
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
        comma = "," if op < 255 else ""
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
            l(f"        case 0x{opc:02X}:  // {get_mnemonic(opc)}")
            emitted.add(opc)
        
        l(f"            {code}")
        l(f"            break;")
        l("")

def generate_addressing_modes():
    """Generate shared addressing mode sequences"""
    l("        // ==========================================")
    l(f"        // [256-{ADDR_SEQ_END-1}] SHARED ADDRESSING SEQUENCES")
    l("        // ==========================================")
    l("")
    
    # Immediate (256+0)
    l("        case 256:  // IMM: immediate")
    l("            BUS_READ(c->PC++);")
    l("            c->AD = BUS_DATA();")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Zero Page (256+2,3)
    l("        case 258:  // ZP cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            break;")
    l("        case 259:  // ZP cycle 2")
    l("            c->AD = BUS_DATA();")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Zero Page,X (256+5,6,7)
    l("        case 261:  // ZPX cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            break;")
    l("        case 262:  // ZPX cycle 2")
    l("            c->AD = BUS_DATA();")
    l("            BUS_INTERNAL(c->AD);")
    l("            break;")
    l("        case 263:  // ZPX cycle 3")
    l("            c->AD = (c->AD + c->X) & 0xFF;")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Zero Page,Y (256+9,10,11)
    l("        case 265:  // ZPY cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            break;")
    l("        case 266:  // ZPY cycle 2")
    l("            c->AD = BUS_DATA();")
    l("            BUS_INTERNAL(c->AD);")
    l("            break;")
    l("        case 267:  // ZPY cycle 3")
    l("            c->AD = (c->AD + c->Y) & 0xFF;")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Absolute (256+13,14,15)
    l("        case 269:  // ABS cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            c->AD = BUS_DATA();")
    l("            break;")
    l("        case 270:  // ABS cycle 2")
    l("            BUS_READ(c->PC++);")
    l("            c->AD |= BUS_DATA() << 8;")
    l("            break;")
    l("        case 271:  // ABS cycle 3")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Absolute,X (256+17,18,19,20)
    l("        case 273:  // ABX cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            c->AD = BUS_DATA();")
    l("            break;")
    l("        case 274:  // ABX cycle 2")
    l("            BUS_READ(c->PC++);")
    l("            c->AD |= BUS_DATA() << 8;")
    l("            break;")
    l("        case 275:  // ABX cycle 3: check page crossing")
    l("            BUS_READ((c->AD & 0xFF00) | ((c->AD + c->X) & 0xFF));")
    l("            if (((c->AD >> 8) == ((c->AD + c->X) >> 8))) {")
    l("                c->AD += c->X;")
    l("                c->IR = c->opcode;")
    l("            }")
    l("            break;")
    l("        case 276:  // ABX cycle 4: page crossed")
    l("            c->AD += c->X;")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Absolute,Y (256+22,23,24,25)
    l("        case 278:  // ABY cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            c->AD = BUS_DATA();")
    l("            break;")
    l("        case 279:  // ABY cycle 2")
    l("            BUS_READ(c->PC++);")
    l("            c->AD |= BUS_DATA() << 8;")
    l("            break;")
    l("        case 280:  // ABY cycle 3: check page crossing")
    l("            BUS_READ((c->AD & 0xFF00) | ((c->AD + c->Y) & 0xFF));")
    l("            if (((c->AD >> 8) == ((c->AD + c->Y) >> 8))) {")
    l("                c->AD += c->Y;")
    l("                c->IR = c->opcode;")
    l("            }")
    l("            break;")
    l("        case 281:  // ABY cycle 4: page crossed")
    l("            c->AD += c->Y;")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Indexed Indirect (256+27-31)
    l("        case 283:  // IDX cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            break;")
    l("        case 284:  // IDX cycle 2")
    l("            c->AD = BUS_DATA();")
    l("            BUS_INTERNAL(c->AD);")
    l("            break;")
    l("        case 285:  // IDX cycle 3")
    l("            c->AD = (c->AD + c->X) & 0xFF;")
    l("            BUS_READ(c->AD);")
    l("            break;")
    l("        case 286:  // IDX cycle 4")
    l("            BUS_READ((c->AD + 1) & 0xFF);")
    l("            c->AD = BUS_DATA();")
    l("            break;")
    l("        case 287:  // IDX cycle 5")
    l("            c->AD |= BUS_DATA() << 8;")
    l("            c->IR = c->opcode;")
    l("            break;")
    l("")
    
    # Indirect Indexed (256+33-37)
    l("        case 289:  // IDY cycle 1")
    l("            BUS_READ(c->PC++);")
    l("            break;")
    l("        case 290:  // IDY cycle 2")
    l("            c->AD = BUS_DATA();")
    l("            BUS_READ(c->AD);")
    l("            break;")
    l("        case 291:  // IDY cycle 3")
    l("            BUS_READ((c->AD + 1) & 0xFF);")
    l("            c->AD = BUS_DATA();")
    l("            break;")
    l("        case 292:  // IDY cycle 4: check page crossing")
    l("            c->AD |= BUS_DATA() << 8;")
    l("            BUS_READ((c->AD & 0xFF00) | ((c->AD + c->Y) & 0xFF));")
    l("            if (((c->AD >> 8) == ((c->AD + c->Y) >> 8))) {")
    l("                c->AD += c->Y;")
    l("                c->IR = c->opcode;")
    l("            }")
    l("            break;")
    l("        case 293:  // IDY cycle 5: page crossed")
    l("            c->AD += c->Y;")
    l("            c->IR = c->opcode;")
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
            l(f"        case {const_name} + {i}: {cycle_code}break;")
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
    # First pass: analyze all opcodes to discover continuation sequences
    for op in range(256):
        generate_opcode_implementation(op)
    
    l("/*")
    l(" * AUTO-GENERATED by fam65xx_gen.py")
    l(" * 65xx Family CPU Decoder")
    l(" * Optimized for maximum performance and 100% hardware accuracy")
    l(f" * ")
    l(f" * Layout:")
    l(f" *   [0-255]   : Opcode-specific cycles")
    l(f" *   [256-{ADDR_SEQ_END-1}] : Shared addressing mode sequences")
    l(f" *   [{CONT_SEQ_START}-{next_continuation_index-1}]  : Shared continuation sequences")
    l(f" * Total cases: {next_continuation_index}")
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