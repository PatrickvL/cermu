#!/usr/bin/env python3
"""
Generate a 4KB Atari 2600 test ROM — scrolling color gradient with playfield.

The ROM exercises TIA background color, playfield color, and playfield
registers to produce a visually obvious animated display.  Useful for
verifying basic CPU execution, TIA rendering, and frame timing.

Frame structure (NTSC, 262 lines):
    3 lines VSYNC
   37 lines VBLANK
  192 lines visible  (kernel: scrolling gradient)
   30 lines overscan
  --- total 262

Branch offsets are computed explicitly (target - next_pc) to avoid
typical off-by-one displacement errors.
"""

import struct
import sys
import os

ROM_SIZE = 4096
ORG      = 0xF000          # CPU-internal origin (6507 sees A0-A12 only)

# TIA write register addresses
VSYNC  = 0x00
VBLANK = 0x01
WSYNC  = 0x02
COLUP0 = 0x06
COLUPF = 0x08
COLUBK = 0x09
PF0    = 0x0D
PF1    = 0x0E
PF2    = 0x0F

# RIOT RAM scratch location for frame counter
FRAME_COUNTER = 0x80

code = bytearray()
labels = {}


def emit(*args):
    """Append raw bytes to code."""
    for b in args:
        code.append(b & 0xFF)


def pc():
    """Return current program counter."""
    return ORG + len(code)


def label(name):
    """Record current PC as a named label."""
    labels[name] = pc()


def branch_offset(target_name):
    """Compute signed byte for Bxx instruction to a forward/backward label.

    The branch displacement is: target - (pc + 2), where pc is the address
    of the branch opcode and +2 accounts for the 2-byte instruction.
    """
    target = labels[target_name]
    next_pc = pc() + 2      # pc of byte AFTER the branch instruction
    offset = target - next_pc
    assert -128 <= offset <= 127, f"Branch to {target_name} out of range: {offset}"
    return offset & 0xFF


# ============================================================================
# RESET entry point ($F000)
# ============================================================================
label("reset")
emit(0x78)                  # SEI
emit(0xD8)                  # CLD
emit(0xA2, 0xFF)            # LDX #$FF
emit(0x9A)                  # TXS          ; S = $FF

# Clear TIA + zero-page (writes 0 to $FF down through $00)
emit(0xA9, 0x00)            # LDA #$00
emit(0xA8)                  # TAY          ; Y = 0 (used later)
emit(0xAA)                  # TAX          ; X = 0

label("clear_loop")
emit(0x95, 0x00)            # STA $00,X
emit(0xE8)                  # INX
emit(0xD0, branch_offset("clear_loop"))  # BNE clear_loop (X wraps 1→255→0)

# ============================================================================
# Main frame loop
# ============================================================================
label("frame_start")

# --- 3 lines VSYNC ---
emit(0xA9, 0x02)            # LDA #$02
emit(0x85, VSYNC)           # STA VSYNC     ; turn on VSYNC
emit(0x85, WSYNC)           # STA WSYNC     ; line 1
emit(0x85, WSYNC)           # STA WSYNC     ; line 2
emit(0x85, WSYNC)           # STA WSYNC     ; line 3
emit(0xA9, 0x00)            # LDA #$00
emit(0x85, VSYNC)           # STA VSYNC     ; turn off VSYNC

# --- 37 lines VBLANK ---
# VBLANK was left ON from overscan of previous frame (or is 0 on cold start)
emit(0xA2, 37)              # LDX #37
label("vblank_loop")
emit(0x85, WSYNC)           # STA WSYNC     ; (A is still 0, but WSYNC ignores data)
emit(0xCA)                  # DEX
emit(0xD0, branch_offset("vblank_loop"))  # BNE vblank_loop

# --- Turn off VBLANK, set up visible area ---
emit(0x85, VBLANK)          # STA VBLANK    ; VBLANK = 0 (A is 0)

# Set playfield pattern
emit(0xA9, 0xF0)            # LDA #$F0
emit(0x85, PF0)             # STA PF0       ; PF0 = $F0 (left 4 bits)
emit(0xA9, 0xAA)            # LDA #$AA
emit(0x85, PF1)             # STA PF1       ; PF1 = $AA (alternating)
emit(0xA9, 0x55)            # LDA #$55
emit(0x85, PF2)             # STA PF2       ; PF2 = $55 (alternating)

# Increment frame counter for scrolling
emit(0xE6, FRAME_COUNTER)   # INC $80
emit(0xA4, FRAME_COUNTER)   # LDY $80       ; Y = frame counter

# --- 192 visible scanlines ---
emit(0xA2, 192)             # LDX #192
label("kernel_loop")
emit(0x84, COLUBK)          # STY COLUBK    ; background = gradient value
emit(0x98)                  # TYA
emit(0x49, 0x80)            # EOR #$80
emit(0x85, COLUPF)          # STA COLUPF    ; playfield = complement color
emit(0xC8)                  # INY           ; next gradient hue
emit(0xC8)                  # INY           ; skip luminance
emit(0x85, WSYNC)           # STA WSYNC     ; wait for end of scanline
emit(0xCA)                  # DEX
emit(0xD0, branch_offset("kernel_loop"))  # BNE kernel_loop

# --- 30 lines overscan ---
emit(0xA9, 0x02)            # LDA #$02
emit(0x85, VBLANK)          # STA VBLANK    ; turn on VBLANK (blanks output)
emit(0xA9, 0x00)            # LDA #$00
emit(0x85, COLUBK)          # STA COLUBK    ; black background for overscan
emit(0xA2, 30)              # LDX #30
label("overscan_loop")
emit(0x85, WSYNC)           # STA WSYNC
emit(0xCA)                  # DEX
emit(0xD0, branch_offset("overscan_loop"))  # BNE overscan_loop

emit(0x4C)                  # JMP frame_start
emit(labels["frame_start"] & 0xFF)
emit((labels["frame_start"] >> 8) & 0xFF)

# ============================================================================
# Fill ROM, set vectors
# ============================================================================
assert len(code) <= ROM_SIZE - 6, f"Code too large: {len(code)} bytes (max {ROM_SIZE - 6})"

# Pad to $FFFA (NMI vector)
code.extend(b'\x00' * (ROM_SIZE - 6 - len(code)))

# Vectors: NMI, RESET, IRQ/BRK — all point to reset
reset_addr = labels["reset"]
code.extend(struct.pack('<HHH', reset_addr, reset_addr, reset_addr))

assert len(code) == ROM_SIZE

# Write output
out_path = os.path.join(os.path.dirname(__file__),
                        '..', 'data', 'atari2600', 'roms', 'test_gradient.a26')
out_path = os.path.normpath(out_path)
with open(out_path, 'wb') as f:
    f.write(code)

print(f"Generated {out_path} ({len(code)} bytes)")
print(f"Labels: {', '.join(f'{k}=${v:04X}' for k, v in labels.items())}")
print(f"Code size: {labels.get('overscan_loop', pc()) + 5 - ORG} bytes before padding")
