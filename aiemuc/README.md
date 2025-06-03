# C64 Cycle-Accurate Emulator

A high-performance, cycle-accurate Commodore 64 emulator with optimized memory mapping and direct threading CPU dispatch.

## Architecture

The emulator is split into logical modules for maintainability while preserving optimal performance:

### Core Files

- **`c64.h`** - Main header with common definitions and chip select constants
- **`c64_main.c`** - Main emulator initialization and frame execution loop

### Bus System

- **`bus.h/c`** - Unified bus state and cycle coordination with callback table optimization
  - Pre-computed device callback tables for direct dispatch
  - Coordinated device cycle execution
  - Hardware-accurate BA/RDY line handling

### Memory Management

- **`bus.c`** - PLA emulation and optimized I/O dispatch
  - 256-byte block granularity for optimal device compatibility
  - Pre-computed chip select maps (32 PLA modes × 256 blocks)
  - Direct callback dispatch eliminating conditional overhead
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

- **`mos6581.h/c`** - SID sound chip
  - Envelope generator emulation
  - Register-accurate sound synthesis

### CPU Emulation

- **`mos6510.h/c`** - MOS 6510 CPU with function pointer dispatch
  - Function pointer table dispatch for optimal performance
  - Hardware-accurate BA/RDY stall handling
  - Cycle-accurate instruction execution
  - Comprehensive illegal opcode support

## Key Optimizations

1. **256-byte Memory Blocks** - Perfect granularity for CIA devices (16 registers)
2. **Callback Table Dispatch** - Pre-computed function pointers eliminate conditional overhead
3. **Function Pointer Dispatch** - Direct function calls eliminate branch prediction overhead
4. **Pre-computed PLA Maps** - Instant address decoding for all 32 memory modes
5. **Unified Bus Cycles** - All devices run every cycle for perfect timing accuracy

## Building

```bash
make clean && make
```

## Performance Features

- **Zero-overhead I/O dispatch** - Direct callback function calls eliminate conditionals
- **Eliminated chip select checks** - Pre-computed callback tables handle device selection
- **Hardware-accurate timing** - BA/RDY lines control CPU stalls precisely
- **Cache-optimized memory layout** - Aligned data structures for performance

## Usage

```c
c64_init();        // Initialize emulator state
c64_emulate_frame(); // Run one frame (infinite loop)
```

The emulator provides the foundation for cycle-accurate C64 emulation with modern optimization techniques while maintaining hardware compatibility.
