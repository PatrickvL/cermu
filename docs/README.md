# AI Emulator (aiemu)

A cycle-accurate MOS 6502 CPU emulator written in C# using Avalonia UI framework. This project implements a detailed simulation of the famous 8-bit processor used in systems like the Commodore 64, Apple II, and NES.

## Features

- **Cycle-accurate MOS 6502 CPU emulation**: Every instruction executes with precise timing matching the original hardware
- **Complete instruction set**: Supports all documented 6502 opcodes including loads, stores, arithmetic, logical operations, branches, and stack operations
- **Bus-accurate simulation**: Implements proper read/write cycles with address and data bus simulation
- **Cross-platform GUI**: Built with Avalonia UI for Windows, macOS, and Linux support
- **Real-time execution monitoring**: Debug output shows cycle-by-cycle execution details

## Architecture

### Core Components

- **[`MOS6502`](aiemu/MOS6502.cs)**: The main CPU emulator implementing cycle-accurate instruction execution
- **[`C64Bus`](aiemu/C64Bus.cs)**: Simple bus simulation with 64KB RAM addressing
- **[`MainWindow`](aiemu/MainWindow.axaml)**: Avalonia-based user interface
- **[`Program`](aiemu/Program.cs)**: Entry point with example emulation code

### Key Implementation Details

The emulator uses a state machine approach where each CPU cycle is explicitly modeled:

```csharp
private delegate CycleAction CycleAction();
private CycleAction nextCycle;
```

Instructions are broken down into individual bus cycles, accurately representing:
- Opcode fetch cycles
- Address calculation cycles  
- Memory read/write cycles
- Internal processing cycles

## Building and Running

### Prerequisites

- .NET 9.0 SDK
- Compatible with Windows, macOS, and Linux

### Build

```bash
cd aiemu
dotnet build
```

### Run

```bash
dotnet run
```

## Example Usage

The program includes a demonstration that:

1. Loads a simple 6502 assembly program into memory
2. Executes it cycle by cycle
3. Displays detailed execution trace including:
   - Bus activity (address/data lines, read/write signals)
   - CPU register states (A, X, Y, PC, P, S)
   - Instruction completion status

```
Cycle:01 Addr:$8000 R CS:1 Data:$A9 PC:$8001 A:$00 X:$00 Y:$00 P:$20 S:$FF 
Cycle:02 Addr:$8001 R CS:1 Data:$42 PC:$8002 A:$42 X:$00 Y:$00 P:$20 S:$FF COMPLETE
```

## Supported Instructions

### Load/Store Operations
- LDA, LDX, LDY (Load accumulator/X/Y register)
- STA, STX, STY (Store accumulator/X/Y register)

### Register Transfers  
- TAX, TXA, TAY, TYA, TSX, TXS

### Arithmetic
- ADC (Add with carry)
- SBC (Subtract with carry)

### Logical Operations
- AND, ORA, EOR (Bitwise operations)

### Comparisons
- CMP, CPX, CPY (Compare registers)

### Branches
- BEQ, BNE, BCS, BCC, BVS, BVC, BMI, BPL

### Stack Operations
- PHA, PLA, PHP, PLP (Push/pull accumulator and processor status)

### Jumps and Subroutines
- JMP (Jump absolute and indirect)
- JSR, RTS (Jump to subroutine and return)
- RTI (Return from interrupt)

### Increment/Decrement
- INX, INY, DEX, DEY

### Status Flag Control
- SEC, CLC, SEI, CLI, SED, CLD, CLV

### System
- BRK (Break/interrupt)
- NOP (No operation)

## Addressing Modes

- **Immediate**: `#$42`
- **Zero Page**: `$80` 
- **Absolute**: `$8000`
- **Indirect**: `($8000)` (JMP only)

## Project Structure

```
aiemu/
├── aiemu.csproj          # Project configuration
├── Program.cs            # Application entry point  
├── App.axaml(.cs)        # Avalonia application setup
├── MainWindow.axaml(.cs) # Main UI window
├── MOS6502.cs           # CPU emulator implementation
├── C64Bus.cs            # Bus/memory simulation
└── app.manifest         # Windows application manifest
```

## Dependencies

- **Avalonia 11.2.1**: Cross-platform UI framework
- **Avalonia.Desktop**: Desktop platform support
- **Avalonia.Themes.Fluent**: Modern UI theme
- **Avalonia.Fonts.Inter**: Font support
- **Avalonia.Diagnostics**: Development tools (Debug builds only)

## Technical Notes

- Implements the official MOS 6502 instruction timings
- Properly handles the 6502's unique behaviors (like dummy reads)
- Status register flags are accurately maintained
- Stack operations use the correct 6502 stack behavior ($0100-$01FF)
- Interrupt vector handling matches original hardware

## Future Enhancements

- Additional addressing modes (indexed, indirect indexed)
- Interrupt handling (IRQ, NMI)
- Memory-mapped I/O simulation
- Full Commodore 64 system emulation
- Interactive debugger interface
- Save/load state functionality

## License

This project demonstrates educational CPU emulation techniques and is suitable for learning about computer architecture and emulation development.
