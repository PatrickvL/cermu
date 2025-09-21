# MOS 6510 Pin Specification

This document provides a systematic description of all pins on the MOS 6510 microprocessor, grouped by logical function.

## Power Supply Pins

### VCC (Pin 25) and VDD (Pin 8)
- **VCC**: +5V supply for logic circuits
- **VDD**: +12V supply for internal clock generation and some I/O drivers
- Both must be stable during operation

### VSS (Pin 21) and VBB (Pin 1)
- **VSS**: Ground (0V) reference for logic circuits
- **VBB**: -5V supply for substrate bias (some variants may not require this)

## Clock Pins

### φ0 (Pin 3) - Clock Input
- External clock input, typically 1-2 MHz
- CMOS levels required for proper operation
- All internal timing derives from this signal

### φ1 (Pin 37) and φ2 (Pin 39) - Clock Outputs
- Two-phase non-overlapping clock outputs
- φ1 leads φ2 in timing
- Used to synchronize external circuitry with CPU operations
- Memory accesses typically occur during φ2 high periods

## Address Bus (16-bit)

### A0-A15 (Pins 9-20, 22-25)
- 16-bit address bus providing 64K addressing range
- Addresses are valid during φ2 high for memory operations
- Three-state outputs that can be disabled via bus control signals
- Address setup occurs during φ1, becomes valid during φ2

## Data Bus (8-bit)

### D0-D7 (Pins 26-33)
- Bidirectional 8-bit data bus
- During reads: CPU inputs data during φ2 high
- During writes: CPU outputs data during φ2 high
- Three-state capability for bus sharing
- Data setup/hold times are critical relative to φ2

## Memory Control Signals

### R/W (Pin 34) - Read/Write
- HIGH = Read cycle, LOW = Write cycle
- Changes state during φ1, valid during φ2
- Directly controls memory operation type

### SYNC (Pin 7) - Synchronization
- HIGH during T1 (opcode fetch) cycles only
- LOW during all other cycles (T0, T2-T7)
- Used by external hardware to identify instruction boundaries
- Critical for single-stepping and instruction monitoring

## Bus Control and Reset

### RDY (Pin 2) - Ready
- Input signal to halt processor during current cycle
- When LOW, extends current φ2 cycle indefinitely
- CPU samples RDY during φ2 high
- Used for slow memory interfacing and DMA operations
- Only affects cycles where CPU would read/write memory

### RES (Pin 40) - Reset
- Active LOW reset input
- Must be held LOW for minimum 2 cycles after power-up
- When released, initiates reset sequence:
  - T0-T1: Internal reset operations
  - T2-T7: Stack pointer decremented 3 times (but no writes occur)
  - T6-T7: Vector fetch from FFFC/FFFD
  - Sets I flag, clears D flag, loads PC from reset vector

## Interrupt Pins

### IRQ (Pin 4) - Interrupt Request
- Active LOW maskable interrupt
- Sampled during φ2 of last cycle of each instruction
- If I flag is clear and IRQ is LOW:
  - T0-T1: Finish current instruction
  - T2: Push PCH to stack
  - T3: Push PCL to stack
  - T4: Push status register to stack (with B flag clear)
  - T5: Set I flag
  - T6-T7: Load PC from FFFE/FFFF
- RTI instruction reverses this process

### NMI (Pin 6) - Non-Maskable Interrupt
- Active LOW non-maskable interrupt
- Edge-sensitive (HIGH-to-LOW transition)
- Cannot be disabled by I flag
- Similar sequence to IRQ but vectors through FFFA/FFFB
- Higher priority than IRQ

## 6510-Specific I/O Port

### P0-P7 (Pins 35, 36, 38, and others) - Port Pins
- 6510 includes built-in 8-bit I/O port
- Controlled by addresses $0000 (data direction) and $0001 (data)
- Each bit can be configured as input or output
- Used in C64 for bank switching and cassette/serial control
- P0-P5 typically available on package pins
- P6-P7 may be internal or package-specific

## Special Behavioral Notes

### Bus Timing
- Address valid during entire φ2 high period
- Data setup required before φ2 falls (reads) or valid during φ2 high (writes)
- All control signals (R/W, SYNC) change during φ1, stable during φ2

### Three-State Conditions
- Address and data buses enter high-impedance when RDY is held LOW during certain cycles
- Allows DMA controllers to take over buses

### Critical Opcodes for Pin Behavior
- **BRK ($00)**: Similar to IRQ sequence but sets B flag in pushed status
- **RTI ($40)**: Restores status and PC from stack, affects interrupt handling
- **CLI ($58)/SEI ($78)**: Control IRQ mask, affecting IRQ pin sensitivity
- **All Read-Modify-Write instructions**: Generate extra cycles with specific R/W patterns

## Implementation Notes

The 6510's pin behavior is tightly coupled to its internal state machine, with most control signals following predictable patterns relative to the two-phase clock system.
