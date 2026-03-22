#!/usr/bin/env python3
"""
mc6809_test_gen.py — Generate MC6809 SingleStepTests JSON test data

Generates test vectors for the MC6809 CPU by computing expected outputs
from the MC6809 instruction set specification. Each instruction gets
multiple test cases with randomized initial state and deterministic
expected final state.

Output format: JSON files compatible with the mc6809_processor_tests_runner,
one file per opcode (e.g., "86.json" for LDA immediate).

Usage:
    python3 mc6809_test_gen.py [output_directory] [--opcode XX] [--count N]
"""

import json
import os
import random
import sys
import struct
from pathlib import Path

# ============================================================================
# MC6809 Reference Emulator (minimal, for test vector generation)
# ============================================================================

# CC flag bits
CC_C = 0x01  # Carry
CC_V = 0x02  # Overflow
CC_Z = 0x04  # Zero
CC_N = 0x08  # Negative
CC_I = 0x10  # IRQ mask
CC_H = 0x20  # Half-carry
CC_F = 0x40  # FIRQ mask
CC_E = 0x80  # Entire state saved


class MC6809State:
    """CPU register state for a test case."""
    __slots__ = ['a', 'b', 'x', 'y', 'u', 's', 'pc', 'dp', 'cc']

    def __init__(self, a=0, b=0, x=0, y=0, u=0, s=0, pc=0, dp=0, cc=0):
        self.a = a & 0xFF
        self.b = b & 0xFF
        self.x = x & 0xFFFF
        self.y = y & 0xFFFF
        self.u = u & 0xFFFF
        self.s = s & 0xFFFF
        self.pc = pc & 0xFFFF
        self.dp = dp & 0xFF
        self.cc = cc & 0xFF

    @property
    def d(self):
        return (self.a << 8) | self.b

    @d.setter
    def d(self, val):
        val &= 0xFFFF
        self.a = (val >> 8) & 0xFF
        self.b = val & 0xFF

    def copy(self):
        return MC6809State(self.a, self.b, self.x, self.y, self.u, self.s,
                           self.pc, self.dp, self.cc)

    def to_dict(self):
        return {
            'a': self.a, 'b': self.b,
            'x': self.x, 'y': self.y,
            'u': self.u, 's': self.s,
            'pc': self.pc, 'dp': self.dp, 'cc': self.cc,
        }


def random_state(pc=None):
    """Generate a random initial CPU state."""
    s = MC6809State()
    s.a = random.randint(0, 255)
    s.b = random.randint(0, 255)
    s.x = random.randint(0, 0xFFFF)
    s.y = random.randint(0, 0xFFFF)
    s.u = random.randint(0x0200, 0x7FFF)  # Keep stacks in reasonable range
    s.s = random.randint(0x8000, 0xFEFF)  # Keep above vectors
    s.pc = pc if pc is not None else random.randint(0x0100, 0x7F00)
    s.dp = random.randint(0, 255)
    s.cc = random.randint(0, 0xFF)
    return s


def random_state_direct_safe(pc=None, instr_len=2):
    """Generate a state where DP page doesn't overlap instruction bytes."""
    s = random_state(pc)
    # Ensure DP page doesn't collide with instruction PC range
    pc_page = s.pc >> 8
    while s.dp == pc_page:
        s.dp = random.randint(0, 255)
    return s


# ============================================================================
# ALU Operations (compute flags + result)
# ============================================================================

def set_nz8(cc, val):
    """Set N and Z flags for 8-bit result."""
    cc &= ~(CC_N | CC_Z)
    if val == 0:
        cc |= CC_Z
    if val & 0x80:
        cc |= CC_N
    return cc


def set_nz16(cc, val):
    """Set N and Z flags for 16-bit result."""
    cc &= ~(CC_N | CC_Z)
    if val == 0:
        cc |= CC_Z
    if val & 0x8000:
        cc |= CC_N
    return cc


def alu_add8(a, b, carry_in, cc):
    """8-bit add with optional carry. Returns (result, new_cc)."""
    c = (cc & CC_C) if carry_in else 0
    result16 = a + b + c
    result = result16 & 0xFF
    cc &= ~(CC_H | CC_N | CC_Z | CC_V | CC_C)
    if result == 0: cc |= CC_Z
    if result & 0x80: cc |= CC_N
    if result16 & 0x100: cc |= CC_C
    if (a ^ b ^ result ^ (result16 >> 1)) & 0x80: cc |= CC_V
    if (a ^ b ^ result) & 0x10: cc |= CC_H
    return result, cc


def alu_sub8(a, b, borrow_in, cc):
    """8-bit subtract with optional borrow. Returns (result, new_cc)."""
    c = (cc & CC_C) if borrow_in else 0
    result16 = a - b - c
    result = result16 & 0xFF
    cc &= ~(CC_N | CC_Z | CC_V | CC_C)
    if result == 0: cc |= CC_Z
    if result & 0x80: cc |= CC_N
    if result16 & 0x100: cc |= CC_C  # Borrow
    if (a ^ b) & (a ^ result) & 0x80: cc |= CC_V
    return result, cc


def alu_and8(a, b, cc):
    result = a & b
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    return result, cc


def alu_or8(a, b, cc):
    result = a | b
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    return result, cc


def alu_eor8(a, b, cc):
    result = a ^ b
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    return result, cc


def alu_neg8(val, cc):
    result = (0 - val) & 0xFF
    cc = set_nz8(cc, result)
    cc &= ~CC_C
    if result != 0: cc |= CC_C
    cc &= ~CC_V
    if val == 0x80: cc |= CC_V
    return result, cc


def alu_com8(val, cc):
    result = (~val) & 0xFF
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    cc |= CC_C
    return result, cc


def alu_inc8(val, cc):
    result = (val + 1) & 0xFF
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    if val == 0x7F: cc |= CC_V
    return result, cc


def alu_dec8(val, cc):
    result = (val - 1) & 0xFF
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    if val == 0x80: cc |= CC_V
    return result, cc


def alu_clr8(cc):
    cc &= ~(CC_N | CC_V | CC_C)
    cc |= CC_Z
    return 0, cc


def alu_tst8(val, cc):
    cc = set_nz8(cc, val)
    cc &= ~CC_V
    return cc


def alu_lsr8(val, cc):
    cc &= ~CC_C
    if val & 0x01: cc |= CC_C
    result = val >> 1
    cc = set_nz8(cc, result)
    return result, cc


def alu_asr8(val, cc):
    cc &= ~CC_C
    if val & 0x01: cc |= CC_C
    result = (val >> 1) | (val & 0x80)
    cc = set_nz8(cc, result)
    return result, cc


def alu_asl8(val, cc):
    cc &= ~CC_C
    if val & 0x80: cc |= CC_C
    result = (val << 1) & 0xFF
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    if (val ^ result) & 0x80: cc |= CC_V
    cc &= ~CC_H
    if (val ^ result) & 0x10: cc |= CC_H
    return result, cc


def alu_rol8(val, cc):
    c = 1 if (cc & CC_C) else 0
    cc &= ~CC_C
    if val & 0x80: cc |= CC_C
    result = ((val << 1) | c) & 0xFF
    cc = set_nz8(cc, result)
    cc &= ~CC_V
    if (val ^ result) & 0x80: cc |= CC_V
    return result, cc


def alu_ror8(val, cc):
    c = 0x80 if (cc & CC_C) else 0
    cc &= ~CC_C
    if val & 0x01: cc |= CC_C
    result = (val >> 1) | c
    cc = set_nz8(cc, result)
    return result, cc


def alu_add16(a, b, cc):
    result32 = a + b
    result = result32 & 0xFFFF
    cc &= ~(CC_N | CC_Z | CC_V | CC_C)
    if result == 0: cc |= CC_Z
    if result & 0x8000: cc |= CC_N
    if result32 & 0x10000: cc |= CC_C
    if (a ^ b ^ result ^ (result32 >> 1)) & 0x8000: cc |= CC_V
    return result, cc


def alu_sub16(a, b, cc):
    result32 = a - b
    result = result32 & 0xFFFF
    cc &= ~(CC_N | CC_Z | CC_V | CC_C)
    if result == 0: cc |= CC_Z
    if result & 0x8000: cc |= CC_N
    if result32 & 0x10000: cc |= CC_C
    if (a ^ b) & (a ^ result) & 0x8000: cc |= CC_V
    return result, cc


def alu_ld16_flags(val, cc):
    cc = set_nz16(cc, val)
    cc &= ~CC_V
    return cc


def alu_daa(a, cc):
    correction = 0
    c = (cc & CC_C) != 0
    if (cc & CC_H) or (a & 0x0F) > 9:
        correction |= 0x06
    if c or a > 0x99 or (a > 0x93 and (a & 0x0F) > 9):
        correction |= 0x60
        c = True
    result = (a + correction) & 0xFF
    cc = set_nz8(cc, result)
    if c:
        cc |= CC_C
    return result, cc


def alu_mul(a, b, cc):
    result = a * b
    d = result & 0xFFFF
    cc &= ~CC_Z
    if d == 0: cc |= CC_Z
    cc &= ~CC_C
    if result & 0x80: cc |= CC_C
    return d, cc


def alu_sex(b, cc):
    a = 0xFF if (b & 0x80) else 0x00
    d = (a << 8) | b
    cc = set_nz16(cc, d)
    cc &= ~CC_V
    return a, cc


# ============================================================================
# Branch condition evaluation
# ============================================================================

def eval_cc(opcode, cc):
    """Evaluate branch condition by lower nibble of opcode."""
    cond = opcode & 0x0F
    if cond == 0x0: return True    # BRA
    if cond == 0x1: return False   # BRN
    if cond == 0x2: return not (cc & (CC_C | CC_Z))    # BHI
    if cond == 0x3: return bool(cc & (CC_C | CC_Z))    # BLS
    if cond == 0x4: return not (cc & CC_C)              # BCC
    if cond == 0x5: return bool(cc & CC_C)              # BCS
    if cond == 0x6: return not (cc & CC_Z)              # BNE
    if cond == 0x7: return bool(cc & CC_Z)              # BEQ
    if cond == 0x8: return not (cc & CC_V)              # BVC
    if cond == 0x9: return bool(cc & CC_V)              # BVS
    if cond == 0xA: return not (cc & CC_N)              # BPL
    if cond == 0xB: return bool(cc & CC_N)              # BMI
    # BGE: N^V=0
    if cond == 0xC:
        n = bool(cc & CC_N)
        v = bool(cc & CC_V)
        return n == v
    # BLT: N^V=1
    if cond == 0xD:
        n = bool(cc & CC_N)
        v = bool(cc & CC_V)
        return n != v
    # BGT: Z=0 AND N^V=0
    if cond == 0xE:
        n = bool(cc & CC_N)
        v = bool(cc & CC_V)
        return not (cc & CC_Z) and (n == v)
    # BLE: Z=1 OR N^V=1
    if cond == 0xF:
        n = bool(cc & CC_N)
        v = bool(cc & CC_V)
        return bool(cc & CC_Z) or (n != v)
    return False


# ============================================================================
# TFR/EXG register access
# ============================================================================

def get_tfr_reg(state, code):
    if code & 0x08:  # 8-bit
        idx = code & 0x07
        if idx == 0: return state.a
        if idx == 1: return state.b
        if idx == 2: return state.cc
        if idx == 3: return state.dp
        return 0xFF
    else:  # 16-bit
        idx = code & 0x07
        if idx == 0: return state.d
        if idx == 1: return state.x
        if idx == 2: return state.y
        if idx == 3: return state.u
        if idx == 4: return state.s
        if idx == 5: return state.pc
        return 0xFFFF


def set_tfr_reg(state, code, val):
    if code & 0x08:  # 8-bit
        val = val & 0xFF
        idx = code & 0x07
        if idx == 0: state.a = val
        elif idx == 1: state.b = val
        elif idx == 2: state.cc = val
        elif idx == 3: state.dp = val
    else:  # 16-bit
        val = val & 0xFFFF
        idx = code & 0x07
        if idx == 0: state.d = val
        elif idx == 1: state.x = val
        elif idx == 2: state.y = val
        elif idx == 3: state.u = val
        elif idx == 4: state.s = val
        elif idx == 5: state.pc = val


# ============================================================================
# Test case generation
# ============================================================================

def make_test(name, initial_state, initial_ram, final_state, final_ram):
    """Create a test case dict in SingleStepTests format."""
    init = initial_state.to_dict()
    init['ram'] = initial_ram
    final = final_state.to_dict()
    final['ram'] = final_ram
    return {'name': name, 'initial': init, 'final': final}


def signed8(val):
    """Convert uint8 to signed int8."""
    return val - 256 if val >= 128 else val


def signed16(val):
    """Convert uint16 to signed int16."""
    return val - 65536 if val >= 32768 else val


# ============================================================================
# Instruction generators
# ============================================================================

def gen_nop(count):
    """0x12: NOP — no operation, 2 cycles"""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        f.pc = (pc + 1) & 0xFFFF
        ram = [[pc, 0x12]]
        tests.append(make_test(f'NOP_{i}', s, ram, f, ram))
    return tests


def _gen_inherent_a(opcode, alu_fn, name, count):
    """Inherent instruction operating on register A."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        f.pc = (pc + 1) & 0xFFFF
        result, f.cc = alu_fn(s.a, s.cc)
        f.a = result
        ram = [[pc, opcode]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_inherent_b(opcode, alu_fn, name, count):
    """Inherent instruction operating on register B."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        f.pc = (pc + 1) & 0xFFFF
        result, f.cc = alu_fn(s.b, s.cc)
        f.b = result
        ram = [[pc, opcode]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_imm8_op(opcode, reg_name, alu_fn, name, count, store_result=True):
    """Immediate 8-bit ALU operation."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        operand = random.randint(0, 255)
        reg_val = getattr(s, reg_name)
        result, f.cc = alu_fn(reg_val, operand, s.cc)
        if store_result:
            setattr(f, reg_name, result)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_ld_imm8(opcode, reg_name, name, count):
    """Immediate 8-bit load."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        operand = random.randint(0, 255)
        setattr(f, reg_name, operand)
        f.cc = set_nz8(f.cc, operand)
        f.cc &= ~CC_V
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_direct8_op(opcode, reg_name, alu_fn, name, count, store_result=True):
    """Direct 8-bit ALU operation."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        operand = random.randint(0, 255)
        reg_val = getattr(s, reg_name)
        result, f.cc = alu_fn(reg_val, operand, s.cc)
        if store_result:
            setattr(f, reg_name, result)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, offset], [ea, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_ld_direct8(opcode, reg_name, name, count):
    """Direct 8-bit load."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        operand = random.randint(0, 255)
        setattr(f, reg_name, operand)
        f.cc = set_nz8(f.cc, operand)
        f.cc &= ~CC_V
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, offset], [ea, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_st_direct8(opcode, reg_name, name, count):
    """Direct 8-bit store."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        reg_val = getattr(s, reg_name)
        f.cc = set_nz8(f.cc, reg_val)
        f.cc &= ~CC_V
        f.pc = (pc + 2) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, offset]]
        ram_final = [[pc, opcode], [pc + 1, offset], [ea, reg_val]]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


def _gen_extended8_op(opcode, reg_name, alu_fn, name, count, store_result=True):
    """Extended 8-bit ALU operation."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ea = random.randint(0, 0xFFFF)
        # Avoid collision with instruction bytes
        while ea >= pc and ea <= pc + 3:
            ea = random.randint(0, 0xFFFF)
        operand = random.randint(0, 255)
        reg_val = getattr(s, reg_name)
        result, f.cc = alu_fn(reg_val, operand, s.cc)
        if store_result:
            setattr(f, reg_name, result)
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
               [ea, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_ld_extended8(opcode, reg_name, name, count):
    """Extended 8-bit load."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ea = random.randint(0, 0xFFFF)
        while ea >= pc and ea <= pc + 3:
            ea = random.randint(0, 0xFFFF)
        operand = random.randint(0, 255)
        setattr(f, reg_name, operand)
        f.cc = set_nz8(f.cc, operand)
        f.cc &= ~CC_V
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
               [ea, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_st_extended8(opcode, reg_name, name, count):
    """Extended 8-bit store."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ea = random.randint(0, 0xFFFF)
        while ea >= pc and ea <= pc + 3:
            ea = random.randint(0, 0xFFFF)
        reg_val = getattr(s, reg_name)
        f.cc = set_nz8(f.cc, reg_val)
        f.cc &= ~CC_V
        f.pc = (pc + 3) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF]]
        ram_final = ram_init + [[ea, reg_val]]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


def _gen_direct_rmw(opcode, alu_fn, name, count):
    """Direct-mode read-modify-write (NEG, COM, LSR, etc.)."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        operand = random.randint(0, 255)
        result, f.cc = alu_fn(operand, s.cc)
        f.pc = (pc + 2) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, offset], [ea, operand]]
        ram_final = [[pc, opcode], [pc + 1, offset], [ea, result]]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


def _gen_extended_rmw(opcode, alu_fn, name, count):
    """Extended-mode read-modify-write."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ea = random.randint(0, 0xFFFF)
        while ea >= pc and ea <= pc + 3:
            ea = random.randint(0, 0xFFFF)
        operand = random.randint(0, 255)
        result, f.cc = alu_fn(operand, s.cc)
        f.pc = (pc + 3) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
                    [ea, operand]]
        ram_final = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
                     [ea, result]]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


def _gen_direct_tst(opcode, name, count):
    """Direct TST (read-only, no writeback)."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        operand = random.randint(0, 255)
        f.cc = alu_tst8(operand, s.cc)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, offset], [ea, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_extended_tst(opcode, name, count):
    """Extended TST."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ea = random.randint(0, 0xFFFF)
        while ea >= pc and ea <= pc + 3:
            ea = random.randint(0, 0xFFFF)
        operand = random.randint(0, 255)
        f.cc = alu_tst8(operand, s.cc)
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
               [ea, operand]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def _gen_direct_clr(opcode, name, count):
    """Direct CLR."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        _, f.cc = alu_clr8(s.cc)
        f.pc = (pc + 2) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, offset], [ea, random.randint(0, 255)]]
        ram_final = [[pc, opcode], [pc + 1, offset], [ea, 0]]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


def _gen_extended_clr(opcode, name, count):
    """Extended CLR."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ea = random.randint(0, 0xFFFF)
        while ea >= pc and ea <= pc + 3:
            ea = random.randint(0, 0xFFFF)
        _, f.cc = alu_clr8(s.cc)
        f.pc = (pc + 3) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
                    [ea, random.randint(0, 255)]]
        ram_final = [[pc, opcode], [pc + 1, (ea >> 8) & 0xFF], [pc + 2, ea & 0xFF],
                     [ea, 0]]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


def gen_short_branch(opcode, count):
    """Short branch (0x20-0x2F)."""
    cond_names = ['BRA', 'BRN', 'BHI', 'BLS', 'BCC', 'BCS', 'BNE', 'BEQ',
                  'BVC', 'BVS', 'BPL', 'BMI', 'BGE', 'BLT', 'BGT', 'BLE']
    name = cond_names[opcode & 0x0F]
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        offset_u8 = random.randint(0, 255)
        offset = signed8(offset_u8)
        taken = eval_cc(opcode, s.cc)
        f.pc = (pc + 2) & 0xFFFF
        if taken:
            f.pc = (pc + 2 + offset) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, offset_u8]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def gen_lbra(count):
    """0x16: LBRA — Long branch always."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        off_hi = random.randint(0, 255)
        off_lo = random.randint(0, 255)
        offset = signed16((off_hi << 8) | off_lo)
        f.pc = (pc + 3 + offset) & 0xFFFF
        ram = [[pc, 0x16], [pc + 1, off_hi], [pc + 2, off_lo]]
        tests.append(make_test(f'LBRA_{i}', s, ram, f, ram))
    return tests


def gen_bsr(count):
    """0x8D: BSR — Branch to subroutine."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        offset_u8 = random.randint(0, 255)
        offset = signed8(offset_u8)
        ret_addr = (pc + 2) & 0xFFFF
        f.pc = (pc + 2 + offset) & 0xFFFF
        f.s = (s.s - 2) & 0xFFFF
        ram_init = [[pc, 0x8D], [pc + 1, offset_u8]]
        ram_final = ram_init + [
            [(s.s - 1) & 0xFFFF, ret_addr & 0xFF],
            [(s.s - 2) & 0xFFFF, (ret_addr >> 8) & 0xFF],
        ]
        tests.append(make_test(f'BSR_{i}', s, ram_init, f, ram_final))
    return tests


def gen_lbsr(count):
    """0x17: LBSR — Long branch to subroutine."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        off_hi = random.randint(0, 255)
        off_lo = random.randint(0, 255)
        offset = signed16((off_hi << 8) | off_lo)
        ret_addr = (pc + 3) & 0xFFFF
        f.pc = (pc + 3 + offset) & 0xFFFF
        f.s = (s.s - 2) & 0xFFFF
        ram_init = [[pc, 0x17], [pc + 1, off_hi], [pc + 2, off_lo]]
        ram_final = ram_init + [
            [(s.s - 1) & 0xFFFF, ret_addr & 0xFF],
            [(s.s - 2) & 0xFFFF, (ret_addr >> 8) & 0xFF],
        ]
        tests.append(make_test(f'LBSR_{i}', s, ram_init, f, ram_final))
    return tests


def gen_rts(count):
    """0x39: RTS — Return from subroutine."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        ret_hi = random.randint(0, 255)
        ret_lo = random.randint(0, 255)
        f.pc = (ret_hi << 8) | ret_lo
        f.s = (s.s + 2) & 0xFFFF
        ram = [[pc, 0x39], [s.s, ret_hi], [(s.s + 1) & 0xFFFF, ret_lo]]
        tests.append(make_test(f'RTS_{i}', s, ram, f, ram))
    return tests


def gen_abx(count):
    """0x3A: ABX — Add B to X (unsigned)."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        f.x = (s.x + s.b) & 0xFFFF
        f.pc = (pc + 1) & 0xFFFF
        ram = [[pc, 0x3A]]
        tests.append(make_test(f'ABX_{i}', s, ram, f, ram))
    return tests


def gen_mul(count):
    """0x3D: MUL — Multiply A × B → D."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        result, f.cc = alu_mul(s.a, s.b, s.cc)
        f.d = result
        f.pc = (pc + 1) & 0xFFFF
        ram = [[pc, 0x3D]]
        tests.append(make_test(f'MUL_{i}', s, ram, f, ram))
    return tests


def gen_sex(count):
    """0x1D: SEX — Sign Extend B → D."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        f.a, f.cc = alu_sex(s.b, s.cc)
        f.pc = (pc + 1) & 0xFFFF
        ram = [[pc, 0x1D]]
        tests.append(make_test(f'SEX_{i}', s, ram, f, ram))
    return tests


def gen_daa(count):
    """0x19: DAA — Decimal Adjust A."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        f.a, f.cc = alu_daa(s.a, s.cc)
        f.pc = (pc + 1) & 0xFFFF
        ram = [[pc, 0x19]]
        tests.append(make_test(f'DAA_{i}', s, ram, f, ram))
    return tests


def gen_orcc(count):
    """0x1A: ORCC — OR CC with immediate."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        operand = random.randint(0, 255)
        f.cc = s.cc | operand
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, 0x1A], [pc + 1, operand]]
        tests.append(make_test(f'ORCC_{i}', s, ram, f, ram))
    return tests


def gen_andcc(count):
    """0x1C: ANDCC — AND CC with immediate."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        operand = random.randint(0, 255)
        f.cc = s.cc & operand
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, 0x1C], [pc + 1, operand]]
        tests.append(make_test(f'ANDCC_{i}', s, ram, f, ram))
    return tests


def gen_exg(count):
    """0x1E: EXG — Exchange registers."""
    tests = []
    # Use specific register pairs to avoid complications
    pairs = [
        (0x01, 'D', 'X'), (0x02, 'D', 'Y'), (0x03, 'D', 'U'),
        (0x12, 'X', 'Y'), (0x13, 'X', 'U'),
        (0x89, 'A', 'B'), (0x8A, 'A', 'CC'), (0x8B, 'A', 'DP'),
        (0x9A, 'B', 'CC'), (0x9B, 'B', 'DP'),
    ]
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        pb, src_name, dst_name = random.choice(pairs)
        src_code = (pb >> 4) & 0x0F
        dst_code = pb & 0x0F
        src_val = get_tfr_reg(s, src_code)
        dst_val = get_tfr_reg(s, dst_code)
        set_tfr_reg(f, src_code, dst_val)
        set_tfr_reg(f, dst_code, src_val)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, 0x1E], [pc + 1, pb]]
        tests.append(make_test(f'EXG_{src_name}_{dst_name}_{i}', s, ram, f, ram))
    return tests


def gen_tfr(count):
    """0x1F: TFR — Transfer register."""
    tests = []
    pairs = [
        (0x01, 'D', 'X'), (0x02, 'D', 'Y'), (0x10, 'X', 'D'),
        (0x12, 'X', 'Y'), (0x21, 'Y', 'X'),
        (0x89, 'A', 'B'), (0x8A, 'A', 'CC'), (0x98, 'B', 'A'),
        (0x9B, 'B', 'DP'), (0xA8, 'CC', 'A'),
    ]
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        pb, src_name, dst_name = random.choice(pairs)
        src_code = (pb >> 4) & 0x0F
        dst_code = pb & 0x0F
        src_val = get_tfr_reg(s, src_code)
        set_tfr_reg(f, dst_code, src_val)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, 0x1F], [pc + 1, pb]]
        tests.append(make_test(f'TFR_{src_name}_{dst_name}_{i}', s, ram, f, ram))
    return tests


def gen_jmp_direct(count):
    """0x0E: JMP direct."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        f.pc = (s.dp << 8) | offset
        ram = [[pc, 0x0E], [pc + 1, offset]]
        tests.append(make_test(f'JMP_direct_{i}', s, ram, f, ram))
    return tests


def gen_jmp_extended(count):
    """0x7E: JMP extended."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        target = random.randint(0, 0xFFFF)
        f.pc = target
        ram = [[pc, 0x7E], [pc + 1, (target >> 8) & 0xFF], [pc + 2, target & 0xFF]]
        tests.append(make_test(f'JMP_extended_{i}', s, ram, f, ram))
    return tests


def gen_jsr_direct(count):
    """0x9D: JSR direct."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ret_addr = (pc + 2) & 0xFFFF
        f.pc = (s.dp << 8) | offset
        f.s = (s.s - 2) & 0xFFFF
        ram_init = [[pc, 0x9D], [pc + 1, offset]]
        ram_final = ram_init + [
            [(s.s - 1) & 0xFFFF, ret_addr & 0xFF],
            [(s.s - 2) & 0xFFFF, (ret_addr >> 8) & 0xFF],
        ]
        tests.append(make_test(f'JSR_direct_{i}', s, ram_init, f, ram_final))
    return tests


def gen_jsr_extended(count):
    """0xBD: JSR extended."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        target = random.randint(0, 0xFFFF)
        ret_addr = (pc + 3) & 0xFFFF
        f.pc = target
        f.s = (s.s - 2) & 0xFFFF
        ram_init = [[pc, 0xBD], [pc + 1, (target >> 8) & 0xFF], [pc + 2, target & 0xFF]]
        ram_final = ram_init + [
            [(s.s - 1) & 0xFFFF, ret_addr & 0xFF],
            [(s.s - 2) & 0xFFFF, (ret_addr >> 8) & 0xFF],
        ]
        tests.append(make_test(f'JSR_extended_{i}', s, ram_init, f, ram_final))
    return tests


# --- 16-bit immediate operations ---

def gen_imm16_op(opcode, name, op_fn, count):
    """Immediate 16-bit ALU operation on D."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        op_hi = random.randint(0, 255)
        op_lo = random.randint(0, 255)
        operand = (op_hi << 8) | op_lo
        result, f.cc = op_fn(s.d, operand, s.cc)
        f.d = result
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, op_hi], [pc + 2, op_lo]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def gen_cmpx_imm(count):
    """0x8C: CMPX immediate."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        op_hi = random.randint(0, 255)
        op_lo = random.randint(0, 255)
        operand = (op_hi << 8) | op_lo
        _, f.cc = alu_sub16(s.x, operand, s.cc)
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, 0x8C], [pc + 1, op_hi], [pc + 2, op_lo]]
        tests.append(make_test(f'CMPX_imm_{i}', s, ram, f, ram))
    return tests


def gen_ldd_imm(count):
    """0xCC: LDD immediate."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        hi = random.randint(0, 255)
        lo = random.randint(0, 255)
        f.a = hi
        f.b = lo
        f.cc = alu_ld16_flags((hi << 8) | lo, s.cc)
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, 0xCC], [pc + 1, hi], [pc + 2, lo]]
        tests.append(make_test(f'LDD_imm_{i}', s, ram, f, ram))
    return tests


def gen_ld16_imm(opcode, reg_name, name, count):
    """LDX/LDU immediate."""
    tests = []
    for i in range(count):
        s = random_state()
        f = s.copy()
        pc = s.pc
        hi = random.randint(0, 255)
        lo = random.randint(0, 255)
        val = (hi << 8) | lo
        setattr(f, reg_name, val)
        f.cc = alu_ld16_flags(val, s.cc)
        f.pc = (pc + 3) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, hi], [pc + 2, lo]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


# --- 16-bit direct operations ---

def gen_direct16_op(opcode, name, op_fn, count):
    """Direct 16-bit ALU operation on D."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        op_hi = random.randint(0, 255)
        op_lo = random.randint(0, 255)
        operand = (op_hi << 8) | op_lo
        result, f.cc = op_fn(s.d, operand, s.cc)
        f.d = result
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, offset],
               [ea, op_hi], [(ea + 1) & 0xFFFF, op_lo]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def gen_ldd_direct(count):
    """0xDC: LDD direct."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        hi = random.randint(0, 255)
        lo = random.randint(0, 255)
        val = (hi << 8) | lo
        f.d = val
        f.cc = alu_ld16_flags(val, s.cc)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, 0xDC], [pc + 1, offset],
               [ea, hi], [(ea + 1) & 0xFFFF, lo]]
        tests.append(make_test(f'LDD_direct_{i}', s, ram, f, ram))
    return tests


def gen_ld16_direct(opcode, reg_name, name, count):
    """LDX/LDU direct."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        hi = random.randint(0, 255)
        lo = random.randint(0, 255)
        val = (hi << 8) | lo
        setattr(f, reg_name, val)
        f.cc = alu_ld16_flags(val, s.cc)
        f.pc = (pc + 2) & 0xFFFF
        ram = [[pc, opcode], [pc + 1, offset],
               [ea, hi], [(ea + 1) & 0xFFFF, lo]]
        tests.append(make_test(f'{name}_{i}', s, ram, f, ram))
    return tests


def gen_st16_direct(opcode, reg_name, name, count):
    """STD/STX/STU direct."""
    tests = []
    for i in range(count):
        s = random_state_direct_safe()
        f = s.copy()
        pc = s.pc
        offset = random.randint(0, 255)
        ea = (s.dp << 8) | offset
        reg_val = getattr(s, reg_name) if reg_name != 'd' else s.d
        f.cc = alu_ld16_flags(reg_val, s.cc)
        f.pc = (pc + 2) & 0xFFFF
        ram_init = [[pc, opcode], [pc + 1, offset]]
        ram_final = ram_init + [
            [ea, (reg_val >> 8) & 0xFF],
            [(ea + 1) & 0xFFFF, reg_val & 0xFF],
        ]
        tests.append(make_test(f'{name}_{i}', s, ram_init, f, ram_final))
    return tests


# ============================================================================
# Opcode table — maps opcode to generator function
# ============================================================================

def _wrap_add8(a, b, cc): return alu_add8(a, b, False, cc)
def _wrap_adc8(a, b, cc): return alu_add8(a, b, True, cc)
def _wrap_sub8(a, b, cc): return alu_sub8(a, b, False, cc)
def _wrap_sbc8(a, b, cc): return alu_sub8(a, b, True, cc)
def _wrap_cmp8(a, b, cc):
    _, cc = alu_sub8(a, b, False, cc)
    return a, cc  # result discarded
def _wrap_bit8(a, b, cc):
    _, cc = alu_and8(a, b, cc)
    return a, cc  # result discarded


def build_opcode_generators(count):
    """Build dict mapping opcode -> list of test cases."""
    gens = {}

    # 0x00-0x0F: Direct RMW
    gens[0x00] = lambda: _gen_direct_rmw(0x00, alu_neg8, 'NEG_direct', count)
    gens[0x03] = lambda: _gen_direct_rmw(0x03, alu_com8, 'COM_direct', count)
    gens[0x04] = lambda: _gen_direct_rmw(0x04, alu_lsr8, 'LSR_direct', count)
    gens[0x06] = lambda: _gen_direct_rmw(0x06, alu_ror8, 'ROR_direct', count)
    gens[0x07] = lambda: _gen_direct_rmw(0x07, alu_asr8, 'ASR_direct', count)
    gens[0x08] = lambda: _gen_direct_rmw(0x08, alu_asl8, 'ASL_direct', count)
    gens[0x09] = lambda: _gen_direct_rmw(0x09, alu_rol8, 'ROL_direct', count)
    gens[0x0A] = lambda: _gen_direct_rmw(0x0A, alu_dec8, 'DEC_direct', count)
    gens[0x0C] = lambda: _gen_direct_rmw(0x0C, alu_inc8, 'INC_direct', count)
    gens[0x0D] = lambda: _gen_direct_tst(0x0D, 'TST_direct', count)
    gens[0x0E] = lambda: gen_jmp_direct(count)
    gens[0x0F] = lambda: _gen_direct_clr(0x0F, 'CLR_direct', count)

    # 0x12: NOP
    gens[0x12] = lambda: gen_nop(count)

    # 0x16: LBRA
    gens[0x16] = lambda: gen_lbra(count)
    # 0x17: LBSR
    gens[0x17] = lambda: gen_lbsr(count)

    # 0x19: DAA
    gens[0x19] = lambda: gen_daa(count)
    # 0x1A: ORCC
    gens[0x1A] = lambda: gen_orcc(count)
    # 0x1C: ANDCC
    gens[0x1C] = lambda: gen_andcc(count)
    # 0x1D: SEX
    gens[0x1D] = lambda: gen_sex(count)
    # 0x1E: EXG
    gens[0x1E] = lambda: gen_exg(count)
    # 0x1F: TFR
    gens[0x1F] = lambda: gen_tfr(count)

    # 0x20-0x2F: Short branches
    for opc in range(0x20, 0x30):
        gens[opc] = (lambda o: lambda: gen_short_branch(o, count))(opc)

    # 0x39: RTS
    gens[0x39] = lambda: gen_rts(count)
    # 0x3A: ABX
    gens[0x3A] = lambda: gen_abx(count)
    # 0x3D: MUL
    gens[0x3D] = lambda: gen_mul(count)

    # 0x40-0x4F: Inherent A operations
    inh_a = {
        0x40: (alu_neg8, 'NEGA'), 0x43: (alu_com8, 'COMA'),
        0x44: (alu_lsr8, 'LSRA'), 0x46: (alu_ror8, 'RORA'),
        0x47: (alu_asr8, 'ASRA'), 0x48: (alu_asl8, 'ASLA'),
        0x49: (alu_rol8, 'ROLA'), 0x4A: (alu_dec8, 'DECA'),
        0x4C: (alu_inc8, 'INCA'),
    }
    for opc, (fn, nm) in inh_a.items():
        gens[opc] = (lambda o, f, n: lambda: _gen_inherent_a(o, f, n, count))(opc, fn, nm)

    # TSTA (0x4D) — special: doesn't store result
    def gen_tsta(cnt):
        tests = []
        for i in range(cnt):
            s = random_state()
            f = s.copy()
            f.pc = (s.pc + 1) & 0xFFFF
            f.cc = alu_tst8(s.a, s.cc)
            ram = [[s.pc, 0x4D]]
            tests.append(make_test(f'TSTA_{i}', s, ram, f, ram))
        return tests
    gens[0x4D] = lambda: gen_tsta(count)

    # CLRA (0x4F)
    def gen_clra(cnt):
        tests = []
        for i in range(cnt):
            s = random_state()
            f = s.copy()
            f.pc = (s.pc + 1) & 0xFFFF
            _, f.cc = alu_clr8(s.cc)
            f.a = 0
            ram = [[s.pc, 0x4F]]
            tests.append(make_test(f'CLRA_{i}', s, ram, f, ram))
        return tests
    gens[0x4F] = lambda: gen_clra(count)

    # 0x50-0x5F: Inherent B operations
    inh_b = {
        0x50: (alu_neg8, 'NEGB'), 0x53: (alu_com8, 'COMB'),
        0x54: (alu_lsr8, 'LSRB'), 0x56: (alu_ror8, 'RORB'),
        0x57: (alu_asr8, 'ASRB'), 0x58: (alu_asl8, 'ASLB'),
        0x59: (alu_rol8, 'ROLB'), 0x5A: (alu_dec8, 'DECB'),
        0x5C: (alu_inc8, 'INCB'),
    }
    for opc, (fn, nm) in inh_b.items():
        gens[opc] = (lambda o, f, n: lambda: _gen_inherent_b(o, f, n, count))(opc, fn, nm)

    # TSTB (0x5D)
    def gen_tstb(cnt):
        tests = []
        for i in range(cnt):
            s = random_state()
            f = s.copy()
            f.pc = (s.pc + 1) & 0xFFFF
            f.cc = alu_tst8(s.b, s.cc)
            ram = [[s.pc, 0x5D]]
            tests.append(make_test(f'TSTB_{i}', s, ram, f, ram))
        return tests
    gens[0x5D] = lambda: gen_tstb(count)

    # CLRB (0x5F)
    def gen_clrb(cnt):
        tests = []
        for i in range(cnt):
            s = random_state()
            f = s.copy()
            f.pc = (s.pc + 1) & 0xFFFF
            _, f.cc = alu_clr8(s.cc)
            f.b = 0
            ram = [[s.pc, 0x5F]]
            tests.append(make_test(f'CLRB_{i}', s, ram, f, ram))
        return tests
    gens[0x5F] = lambda: gen_clrb(count)

    # 0x70-0x7F: Extended RMW
    gens[0x70] = lambda: _gen_extended_rmw(0x70, alu_neg8, 'NEG_extended', count)
    gens[0x73] = lambda: _gen_extended_rmw(0x73, alu_com8, 'COM_extended', count)
    gens[0x74] = lambda: _gen_extended_rmw(0x74, alu_lsr8, 'LSR_extended', count)
    gens[0x76] = lambda: _gen_extended_rmw(0x76, alu_ror8, 'ROR_extended', count)
    gens[0x77] = lambda: _gen_extended_rmw(0x77, alu_asr8, 'ASR_extended', count)
    gens[0x78] = lambda: _gen_extended_rmw(0x78, alu_asl8, 'ASL_extended', count)
    gens[0x79] = lambda: _gen_extended_rmw(0x79, alu_rol8, 'ROL_extended', count)
    gens[0x7A] = lambda: _gen_extended_rmw(0x7A, alu_dec8, 'DEC_extended', count)
    gens[0x7C] = lambda: _gen_extended_rmw(0x7C, alu_inc8, 'INC_extended', count)
    gens[0x7D] = lambda: _gen_extended_tst(0x7D, 'TST_extended', count)
    gens[0x7E] = lambda: gen_jmp_extended(count)
    gens[0x7F] = lambda: _gen_extended_clr(0x7F, 'CLR_extended', count)

    # 0x80-0x8F: Immediate A/D operations
    gens[0x80] = lambda: _gen_imm8_op(0x80, 'a', _wrap_sub8, 'SUBA_imm', count)
    gens[0x81] = lambda: _gen_imm8_op(0x81, 'a', _wrap_cmp8, 'CMPA_imm', count, store_result=False)
    gens[0x82] = lambda: _gen_imm8_op(0x82, 'a', _wrap_sbc8, 'SBCA_imm', count)
    gens[0x83] = lambda: gen_imm16_op(0x83, 'SUBD_imm', alu_sub16, count)
    gens[0x84] = lambda: _gen_imm8_op(0x84, 'a', lambda a,b,cc: alu_and8(a,b,cc), 'ANDA_imm', count)
    gens[0x85] = lambda: _gen_imm8_op(0x85, 'a', _wrap_bit8, 'BITA_imm', count, store_result=False)
    gens[0x86] = lambda: _gen_ld_imm8(0x86, 'a', 'LDA_imm', count)
    gens[0x88] = lambda: _gen_imm8_op(0x88, 'a', lambda a,b,cc: alu_eor8(a,b,cc), 'EORA_imm', count)
    gens[0x89] = lambda: _gen_imm8_op(0x89, 'a', _wrap_adc8, 'ADCA_imm', count)
    gens[0x8A] = lambda: _gen_imm8_op(0x8A, 'a', lambda a,b,cc: alu_or8(a,b,cc), 'ORA_imm', count)
    gens[0x8B] = lambda: _gen_imm8_op(0x8B, 'a', _wrap_add8, 'ADDA_imm', count)
    gens[0x8C] = lambda: gen_cmpx_imm(count)
    gens[0x8D] = lambda: gen_bsr(count)
    gens[0x8E] = lambda: gen_ld16_imm(0x8E, 'x', 'LDX_imm', count)

    # 0x90-0x9F: Direct A/D operations
    gens[0x90] = lambda: _gen_direct8_op(0x90, 'a', _wrap_sub8, 'SUBA_direct', count)
    gens[0x91] = lambda: _gen_direct8_op(0x91, 'a', _wrap_cmp8, 'CMPA_direct', count, store_result=False)
    gens[0x92] = lambda: _gen_direct8_op(0x92, 'a', _wrap_sbc8, 'SBCA_direct', count)
    gens[0x93] = lambda: gen_direct16_op(0x93, 'SUBD_direct', alu_sub16, count)
    gens[0x94] = lambda: _gen_direct8_op(0x94, 'a', lambda a,b,cc: alu_and8(a,b,cc), 'ANDA_direct', count)
    gens[0x95] = lambda: _gen_direct8_op(0x95, 'a', _wrap_bit8, 'BITA_direct', count, store_result=False)
    gens[0x96] = lambda: _gen_ld_direct8(0x96, 'a', 'LDA_direct', count)
    gens[0x97] = lambda: _gen_st_direct8(0x97, 'a', 'STA_direct', count)
    gens[0x98] = lambda: _gen_direct8_op(0x98, 'a', lambda a,b,cc: alu_eor8(a,b,cc), 'EORA_direct', count)
    gens[0x99] = lambda: _gen_direct8_op(0x99, 'a', _wrap_adc8, 'ADCA_direct', count)
    gens[0x9A] = lambda: _gen_direct8_op(0x9A, 'a', lambda a,b,cc: alu_or8(a,b,cc), 'ORA_direct', count)
    gens[0x9B] = lambda: _gen_direct8_op(0x9B, 'a', _wrap_add8, 'ADDA_direct', count)
    # 0x9C: CMPX direct
    # 0x9C: CMPX direct (compare X with memory, update flags only)
    def gen_cmpx_direct(cnt):
        tests = []
        for i in range(cnt):
            s = random_state_direct_safe()
            f = s.copy()
            pc = s.pc
            offset = random.randint(0, 255)
            ea = (s.dp << 8) | offset
            op_hi = random.randint(0, 255)
            op_lo = random.randint(0, 255)
            operand = (op_hi << 8) | op_lo
            _, f.cc = alu_sub16(s.x, operand, s.cc)
            f.pc = (pc + 2) & 0xFFFF
            ram = [[pc, 0x9C], [pc + 1, offset],
                   [ea, op_hi], [(ea + 1) & 0xFFFF, op_lo]]
            tests.append(make_test(f'CMPX_direct_{i}', s, ram, f, ram))
        return tests
    gens[0x9C] = lambda: gen_cmpx_direct(count)
    gens[0x9D] = lambda: gen_jsr_direct(count)
    gens[0x9E] = lambda: gen_ld16_direct(0x9E, 'x', 'LDX_direct', count)
    gens[0x9F] = lambda: gen_st16_direct(0x9F, 'x', 'STX_direct', count)

    # 0xB0-0xBF: Extended A/D operations
    gens[0xB0] = lambda: _gen_extended8_op(0xB0, 'a', _wrap_sub8, 'SUBA_extended', count)
    gens[0xB1] = lambda: _gen_extended8_op(0xB1, 'a', _wrap_cmp8, 'CMPA_extended', count, store_result=False)
    gens[0xB2] = lambda: _gen_extended8_op(0xB2, 'a', _wrap_sbc8, 'SBCA_extended', count)
    # 0xB3: SUBD extended — skip for now (16-bit extended gen needed)
    gens[0xB4] = lambda: _gen_extended8_op(0xB4, 'a', lambda a,b,cc: alu_and8(a,b,cc), 'ANDA_extended', count)
    gens[0xB5] = lambda: _gen_extended8_op(0xB5, 'a', _wrap_bit8, 'BITA_extended', count, store_result=False)
    gens[0xB6] = lambda: _gen_ld_extended8(0xB6, 'a', 'LDA_extended', count)
    gens[0xB7] = lambda: _gen_st_extended8(0xB7, 'a', 'STA_extended', count)
    gens[0xB8] = lambda: _gen_extended8_op(0xB8, 'a', lambda a,b,cc: alu_eor8(a,b,cc), 'EORA_extended', count)
    gens[0xB9] = lambda: _gen_extended8_op(0xB9, 'a', _wrap_adc8, 'ADCA_extended', count)
    gens[0xBA] = lambda: _gen_extended8_op(0xBA, 'a', lambda a,b,cc: alu_or8(a,b,cc), 'ORA_extended', count)
    gens[0xBB] = lambda: _gen_extended8_op(0xBB, 'a', _wrap_add8, 'ADDA_extended', count)
    gens[0xBD] = lambda: gen_jsr_extended(count)

    # 0xC0-0xCF: Immediate B operations
    gens[0xC0] = lambda: _gen_imm8_op(0xC0, 'b', _wrap_sub8, 'SUBB_imm', count)
    gens[0xC1] = lambda: _gen_imm8_op(0xC1, 'b', _wrap_cmp8, 'CMPB_imm', count, store_result=False)
    gens[0xC2] = lambda: _gen_imm8_op(0xC2, 'b', _wrap_sbc8, 'SBCB_imm', count)
    gens[0xC3] = lambda: gen_imm16_op(0xC3, 'ADDD_imm', alu_add16, count)
    gens[0xC4] = lambda: _gen_imm8_op(0xC4, 'b', lambda a,b,cc: alu_and8(a,b,cc), 'ANDB_imm', count)
    gens[0xC5] = lambda: _gen_imm8_op(0xC5, 'b', _wrap_bit8, 'BITB_imm', count, store_result=False)
    gens[0xC6] = lambda: _gen_ld_imm8(0xC6, 'b', 'LDB_imm', count)
    gens[0xC8] = lambda: _gen_imm8_op(0xC8, 'b', lambda a,b,cc: alu_eor8(a,b,cc), 'EORB_imm', count)
    gens[0xC9] = lambda: _gen_imm8_op(0xC9, 'b', _wrap_adc8, 'ADCB_imm', count)
    gens[0xCA] = lambda: _gen_imm8_op(0xCA, 'b', lambda a,b,cc: alu_or8(a,b,cc), 'ORB_imm', count)
    gens[0xCB] = lambda: _gen_imm8_op(0xCB, 'b', _wrap_add8, 'ADDB_imm', count)
    gens[0xCC] = lambda: gen_ldd_imm(count)
    gens[0xCE] = lambda: gen_ld16_imm(0xCE, 'u', 'LDU_imm', count)

    # 0xD0-0xDF: Direct B operations
    gens[0xD0] = lambda: _gen_direct8_op(0xD0, 'b', _wrap_sub8, 'SUBB_direct', count)
    gens[0xD1] = lambda: _gen_direct8_op(0xD1, 'b', _wrap_cmp8, 'CMPB_direct', count, store_result=False)
    gens[0xD2] = lambda: _gen_direct8_op(0xD2, 'b', _wrap_sbc8, 'SBCB_direct', count)
    gens[0xD3] = lambda: gen_direct16_op(0xD3, 'ADDD_direct', alu_add16, count)
    gens[0xD4] = lambda: _gen_direct8_op(0xD4, 'b', lambda a,b,cc: alu_and8(a,b,cc), 'ANDB_direct', count)
    gens[0xD5] = lambda: _gen_direct8_op(0xD5, 'b', _wrap_bit8, 'BITB_direct', count, store_result=False)
    gens[0xD6] = lambda: _gen_ld_direct8(0xD6, 'b', 'LDB_direct', count)
    gens[0xD7] = lambda: _gen_st_direct8(0xD7, 'b', 'STB_direct', count)
    gens[0xD8] = lambda: _gen_direct8_op(0xD8, 'b', lambda a,b,cc: alu_eor8(a,b,cc), 'EORB_direct', count)
    gens[0xD9] = lambda: _gen_direct8_op(0xD9, 'b', _wrap_adc8, 'ADCB_direct', count)
    gens[0xDA] = lambda: _gen_direct8_op(0xDA, 'b', lambda a,b,cc: alu_or8(a,b,cc), 'ORB_direct', count)
    gens[0xDB] = lambda: _gen_direct8_op(0xDB, 'b', _wrap_add8, 'ADDB_direct', count)
    gens[0xDC] = lambda: gen_ldd_direct(count)
    gens[0xDD] = lambda: gen_st16_direct(0xDD, 'd', 'STD_direct', count)
    gens[0xDE] = lambda: gen_ld16_direct(0xDE, 'u', 'LDU_direct', count)
    gens[0xDF] = lambda: gen_st16_direct(0xDF, 'u', 'STU_direct', count)

    # 0xF0-0xFF: Extended B operations
    gens[0xF0] = lambda: _gen_extended8_op(0xF0, 'b', _wrap_sub8, 'SUBB_extended', count)
    gens[0xF1] = lambda: _gen_extended8_op(0xF1, 'b', _wrap_cmp8, 'CMPB_extended', count, store_result=False)
    gens[0xF2] = lambda: _gen_extended8_op(0xF2, 'b', _wrap_sbc8, 'SBCB_extended', count)
    gens[0xF4] = lambda: _gen_extended8_op(0xF4, 'b', lambda a,b,cc: alu_and8(a,b,cc), 'ANDB_extended', count)
    gens[0xF5] = lambda: _gen_extended8_op(0xF5, 'b', _wrap_bit8, 'BITB_extended', count, store_result=False)
    gens[0xF6] = lambda: _gen_ld_extended8(0xF6, 'b', 'LDB_extended', count)
    gens[0xF7] = lambda: _gen_st_extended8(0xF7, 'b', 'STB_extended', count)
    gens[0xF8] = lambda: _gen_extended8_op(0xF8, 'b', lambda a,b,cc: alu_eor8(a,b,cc), 'EORB_extended', count)
    gens[0xF9] = lambda: _gen_extended8_op(0xF9, 'b', _wrap_adc8, 'ADCB_extended', count)
    gens[0xFA] = lambda: _gen_extended8_op(0xFA, 'b', lambda a,b,cc: alu_or8(a,b,cc), 'ORB_extended', count)
    gens[0xFB] = lambda: _gen_extended8_op(0xFB, 'b', _wrap_add8, 'ADDB_extended', count)

    return gens


# ============================================================================
# Main
# ============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Generate MC6809 test vectors')
    parser.add_argument('output_dir', nargs='?', default='mc6809_tests',
                        help='Output directory for JSON test files')
    parser.add_argument('--opcode', '-o', type=str, default=None,
                        help='Generate only for specific opcode (hex, e.g. 86)')
    parser.add_argument('--count', '-n', type=int, default=50,
                        help='Number of test cases per opcode (default: 50)')
    parser.add_argument('--seed', '-s', type=int, default=42,
                        help='Random seed (default: 42)')
    args = parser.parse_args()

    random.seed(args.seed)
    outdir = Path(args.output_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    gens = build_opcode_generators(args.count)

    if args.opcode:
        opc = int(args.opcode, 16)
        if opc not in gens:
            print(f"No generator for opcode 0x{opc:02X}")
            return 1
        opcodes = [opc]
    else:
        opcodes = sorted(gens.keys())

    total_tests = 0
    for opc in opcodes:
        tests = gens[opc]()
        if not tests:
            continue
        fname = outdir / f'{opc:02x}.json'
        with open(fname, 'w') as f:
            json.dump(tests, f, separators=(',', ':'))
        total_tests += len(tests)
        print(f'  0x{opc:02X}: {len(tests)} tests → {fname}')

    print(f'\nGenerated {total_tests} test cases for {len(opcodes)} opcodes in {outdir}/')
    return 0


if __name__ == '__main__':
    sys.exit(main())
