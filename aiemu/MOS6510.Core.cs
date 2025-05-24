using System;
using System.Runtime.CompilerServices;

namespace aiemu;

// COMPLETE MOS 6510 CPU EMULATOR (C64/C128 Processor)
//
// FEATURES:
// - All 256 opcodes implemented (official + unofficial/illegal)
// - Cycle-accurate bus timing simulation
// - Performance optimizations: Inline flag operations, pre-allocated delegates
// - Complete addressing mode support including illegal combinations
// - Proper Read-Modify-Write cycle emulation
// - Page boundary detection with extra cycle handling
//
// ARCHITECTURE: Split into partial classes for maintainability:
// - MOS6510.Core.cs: Core functionality, registers, and basic operations
// - MOS6510.AddressingModes.cs: All addressing mode implementations
// - MOS6510.Operations.cs: Instruction operation implementations
// - MOS6510.OpcodeTable.cs: Complete 256-opcode lookup table

public partial class MOS6510
{
    private readonly C64Bus bus;
    public int TotalCycles;
    // CPU registers
    public byte A, X, Y, S = 0xFF;
    public ushort PC = 0x0000;
    public byte P = 0x20;

    // Status flags
    private const byte C = 0x01, Z = 0x02, I = 0x04, D = 0x08, B = 0x10, U = 0x20, V = 0x40, N = 0x80;

    private delegate CycleAction CycleAction();
    private CycleAction nextCycle;

    // Shared state for addressing modes
    private byte lo, hi, val;
    private ushort addr;

    // Pre-allocated operations to avoid delegate creation
    private readonly CycleAction addWithCarryAction;
    private readonly CycleAction andAccumulatorAction;
    private readonly CycleAction fetchOpcodeAction;
    private readonly CycleAction loadAAction;
    private readonly CycleAction loadXAction;
    private readonly CycleAction loadYAction;
    private readonly CycleAction orAccumulatorAction;
    private readonly Func<byte, byte> sLO_OperationAction;
    private readonly CycleAction transferAXAction;
    private readonly CycleAction transferAYAction;
    private readonly CycleAction transferXAAction;
    private readonly CycleAction xorAccumulatorAction;
    
    private readonly Action<byte> plaAction;
    private readonly Action<byte> plpAction;

    public MOS6510(C64Bus bus)
    {
        this.bus = bus;

        // Pre-allocate frequently used operations
        // TODO : Complete pre-allocated operations for all delegates created in lambda expressions
        // which avoids runtime delegate allocations, so even a sole use is beneficial.
        addWithCarryAction = AddWithCarry;
        andAccumulatorAction = AndAccumulator;
        fetchOpcodeAction = FetchOpcode;
        loadAAction = LoadA;
        loadXAction = LoadX;
        loadYAction = LoadY;
        orAccumulatorAction = OrAccumulator;
        sLO_OperationAction = SLO_Operation;
        transferAXAction = TransferAX;
        transferAYAction = TransferAY;
        transferXAAction = TransferXA;
        xorAccumulatorAction = XorAccumulator;

        plaAction = val => { A = val; SetZN(A); };
        plpAction = val => P = val;

        InitializeOpcodeTable();
        // Initialize the CPU state
        nextCycle = fetchOpcodeAction;
    }

    // Inlined flag operations for better performance
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void SetZN(byte v) => P = (byte)((P & ~(Z | N)) | (v == 0 ? Z : 0) | (v & N));
    
    // Replace GetFlag calls with direct bit operations: (P & flag) != 0
    // Replace SetFlag calls with direct bit operations: P = condition ? (byte)(P | flag) : (byte)(P & ~flag)

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void BusRead(ushort address)
    {
        bus.AddressLines = address;
        bus.RW = true;
        bus.CS = true;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void BusWrite(ushort address, byte data)
    {
        bus.AddressLines = address;
        bus.DataLines = data;
        bus.RW = false;
        bus.CS = true;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private byte BusComplete()
    {
        bus.CS = false;
        return bus.DataLines;
    }

    // Combined bus operations for better performance
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private byte BusReadComplete(ushort address)
    {
        bus.AddressLines = address;
        bus.RW = true;
        bus.CS = true;
        bus.Cycle();
        bus.CS = false;
        return bus.DataLines;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void BusWriteComplete(ushort address, byte data)
    {
        bus.AddressLines = address;
        bus.DataLines = data;
        bus.RW = false;
        bus.CS = true;
        bus.Cycle();
        bus.CS = false;
    }

    public bool Step()
    {
        TotalCycles++;
        bus.Cycle();
        nextCycle = nextCycle();
        return nextCycle == fetchOpcodeAction;
    }

    private CycleAction FetchOpcode()
    {
        BusRead(PC++);
        return () => {
            byte opcode = BusComplete();
            return opcodeTable[opcode];
        };
    }
}
