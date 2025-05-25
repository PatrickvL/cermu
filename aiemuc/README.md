# C64 Cycle-Accurate Emulator

A high-performance, cycle-accurate Commodore 64 emulator with optimized memory mapping and direct threading CPU dispatch.

## Architecture

The emulator is split into logical modules for maintainability while preserving optimal performance:

### Core Files

- **`c64.h`** - Main header with common definitions and chip select constants
- **`c64_main.c`** - Main emulator initialization and frame execution loop

### Bus System

- **`bus.h/c`** - Unified bus state and cycle coordination
  - 64-bit bus state union for optimal performance
  - Coordinated device cycle execution
  - Hardware-accurate BA/RDY line handling

### Memory Management

- **`memory.h/c`** - PLA emulation and memory mapping
  - 256-byte block granularity for optimal device compatibility
  - Pre-computed chip select maps (32 PLA modes × 256 blocks)
  - Direct mask storage in pointer LSB bits
  - Optimized read/write cycle functions

### Device Emulation

- **`vic.h/c`** - VIC-II video chip
  - Cycle-accurate raster timing
  - Hardware-accurate badline generation
  - BA/AEC signal control for CPU stalls

- **`cia.h/c`** - CIA timer and I/O chips
  - Timer A/B emulation with interrupts
  - Port A/B handling
  - IRQ/NMI generation

- **`sid.h/c`** - SID sound chip
  - Envelope generator emulation
  - Register-accurate sound synthesis

### CPU Emulation

- **`cpu.h/c`** - 6502 CPU with direct threading
  - Direct threading dispatch for maximum performance
  - Hardware-accurate BA/RDY stall handling
  - Cycle-accurate instruction execution

## Key Optimizations

1. **256-byte Memory Blocks** - Perfect granularity for CIA devices (16 registers)
2. **Direct Mask Storage** - Read masks stored in pointer LSB bits (no shifting)
3. **Direct Threading** - Computed goto dispatch eliminates function call overhead
4. **Pre-computed PLA Maps** - Instant address decoding for all 32 memory modes
5. **Unified Bus Cycles** - All devices run every cycle for perfect timing accuracy

## Building

```bash
make clean && make
```

## Performance Features

- **Zero-overhead bus state** - 64-bit union optimized for registers
- **Eliminated write handler dispatch** - Writes handled directly in device cycles
- **Hardware-accurate timing** - BA/RDY lines control CPU stalls precisely
- **Cache-optimized memory layout** - Aligned data structures for performance

## Usage

```c
c64_init();        // Initialize emulator state
c64_emulate_frame(); // Run one frame (infinite loop)
```

The emulator provides the foundation for cycle-accurate C64 emulation with modern optimization techniques while maintaining hardware compatibility.
