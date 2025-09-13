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
    private CycleAction nextStepAction = null!;

    // Pre-allocated operations to avoid delegate creation
    private readonly CycleAction addWithCarryAction;
    private readonly CycleAction andAccumulatorAction;
    private readonly CycleAction fetchOpcodeAction;
    private readonly CycleAction fetchOpcodeCompleteAction;
    private readonly CycleAction loadAAction;
    private readonly CycleAction loadXAction;
    private readonly CycleAction loadYAction;
    private readonly CycleAction orAccumulatorAction;
    private readonly CycleAction xorAccumulatorAction;
    private readonly CycleAction transferAXAction;
    private readonly CycleAction transferAYAction;
    private readonly CycleAction transferXAAction;
    private readonly CycleAction subtractWithCarryAction;
    private readonly CycleAction compareAAction;
    private readonly CycleAction compareXAction;
    private readonly CycleAction compareYAction;
    private readonly CycleAction bitTestAction;
    private readonly CycleAction incrementXAction;
    private readonly CycleAction incrementYAction;
    private readonly CycleAction decrementXAction;
    private readonly CycleAction decrementYAction;
    private readonly CycleAction setCarryAction;
    private readonly CycleAction clearCarryAction;
    private readonly CycleAction setInterruptAction;
    private readonly CycleAction clearInterruptAction;
    private readonly CycleAction setDecimalAction;
    private readonly CycleAction clearDecimalAction;
    private readonly CycleAction clearOverflowAction;
    private readonly CycleAction transferYAAction;
    private readonly CycleAction transferSXAction;
    private readonly CycleAction transferXSAction;
    private readonly CycleAction arithmeticShiftLeftAAction;
    private readonly CycleAction logicalShiftRightAAction;
    private readonly CycleAction rotateLeftAAction;
    private readonly CycleAction rotateRightAAction;
    private readonly CycleAction laxOperationAction;
    private readonly CycleAction ancOperationAction;
    private readonly CycleAction alrOperationAction;
    private readonly CycleAction arrOperationAction;
    private readonly CycleAction xaaOperationAction;
    private readonly CycleAction axsOperationAction;
    private readonly CycleAction lasOperationAction;

    // Pre-allocated function delegates for modify operations
    private readonly Func<byte, byte> sLO_OperationAction;
    private readonly Func<byte, byte> rLA_OperationAction;
    private readonly Func<byte, byte> sRE_OperationAction;
    private readonly Func<byte, byte> rRA_OperationAction;
    private readonly Func<byte, byte> dCP_OperationAction;
    private readonly Func<byte, byte> iSC_OperationAction;
    private readonly Func<byte, byte> arithmeticShiftLeftAction;
    private readonly Func<byte, byte> logicalShiftRightAction;
    private readonly Func<byte, byte> rotateLeftAction;
    private readonly Func<byte, byte> rotateRightAction;
    private readonly Func<byte, byte> incrementMemoryAction;
    private readonly Func<byte, byte> decrementMemoryAction;

    // Pre-allocated function delegates for getValue operations
    private readonly Func<byte> getAAction;
    private readonly Func<byte> getXAction;
    private readonly Func<byte> getYAction;
    private readonly Func<byte> getPAction;
    private readonly Func<byte> getAXAction;
    private readonly Func<byte> getYHiAction;
    private readonly Func<byte> getXHiAction;
    private readonly Func<byte> getAXHiAction;
    private readonly Func<byte> getAXSAction;

    // Pre-allocated Action delegates for stack operations
    private readonly Action<byte> plaAction;
    private readonly Action<byte> plpAction;

    // Pre-allocated Func delegates for branch conditions
    private readonly Func<bool> branchCarryClearAction;
    private readonly Func<bool> branchCarrySetAction;
    private readonly Func<bool> branchZeroClearAction;
    private readonly Func<bool> branchZeroSetAction;
    private readonly Func<bool> branchNegativeClearAction;
    private readonly Func<bool> branchNegativeSetAction;
    private readonly Func<bool> branchOverflowClearAction;
    private readonly Func<bool> branchOverflowSetAction;

    // Pre-allocated continuation delegates for addressing modes (to eliminate lambda expressions)
    private readonly CycleAction readZeroPageStep1Complete;
    
    private readonly CycleAction readImmediateLoadAComplete;
    private readonly CycleAction readImmediateLoadXComplete;
    private readonly CycleAction readImmediateLoadYComplete;
    private readonly CycleAction readImmediateOrAccumulatorComplete;
    private readonly CycleAction readImmediateAndAccumulatorComplete;
    private readonly CycleAction readImmediateXorAccumulatorComplete;
    private readonly CycleAction readImmediateAddWithCarryComplete;
    private readonly CycleAction readImmediateSubtractWithCarryComplete;
    private readonly CycleAction readImmediateCompareAComplete;
    private readonly CycleAction readImmediateCompareXComplete;
    private readonly CycleAction readImmediateCompareYComplete;
    private readonly CycleAction readImmediateNopComplete;
    private readonly CycleAction readImmediateLAXComplete;
    private readonly CycleAction readImmediateANCComplete;
    private readonly CycleAction readImmediateALRComplete;
    private readonly CycleAction readImmediateARRComplete;
    private readonly CycleAction readImmediateXAAComplete;
    private readonly CycleAction readImmediateAXSComplete;

    public MOS6510(C64Bus bus)
    {
        this.bus = bus;

        // Pre-allocate all operations to avoid runtime delegate creation
        addWithCarryAction = AddWithCarry;
        andAccumulatorAction = AndAccumulator;
        fetchOpcodeAction = FetchOpcode;
        fetchOpcodeCompleteAction = FetchOpcodeComplete;
        
        // Pre-allocate zero page addressing mode delegates
        readZeroPageStep1Complete = ReadZeroPageStep1Complete;
        loadAAction = LoadA;
        loadXAction = LoadX;
        loadYAction = LoadY;
        orAccumulatorAction = OrAccumulator;
        xorAccumulatorAction = XorAccumulator;
        transferAXAction = TransferAX;
        transferAYAction = TransferAY;
        transferXAAction = TransferXA;
        subtractWithCarryAction = SubtractWithCarry;
        compareAAction = CompareA;
        compareXAction = CompareX;
        compareYAction = CompareY;
        bitTestAction = BitTest;
        incrementXAction = IncrementX;
        incrementYAction = IncrementY;
        decrementXAction = DecrementX;
        decrementYAction = DecrementY;
        setCarryAction = SetCarry;
        clearCarryAction = ClearCarry;
        setInterruptAction = SetInterrupt;
        clearInterruptAction = ClearInterrupt;
        setDecimalAction = SetDecimal;
        clearDecimalAction = ClearDecimal;
        clearOverflowAction = ClearOverflow;
        transferYAAction = TransferYA;
        transferSXAction = TransferSX;
        transferXSAction = TransferXS;
        arithmeticShiftLeftAAction = ArithmeticShiftLeftA;
        logicalShiftRightAAction = LogicalShiftRightA;
        rotateLeftAAction = RotateLeftA;
        rotateRightAAction = RotateRightA;
        laxOperationAction = LAX_Operation;
        ancOperationAction = ANC_Operation;
        alrOperationAction = ALR_Operation;
        arrOperationAction = ARR_Operation;
        xaaOperationAction = XAA_Operation;
        axsOperationAction = AXS_Operation;
        lasOperationAction = LAS_Operation;

        // Function delegates for modify operations
        sLO_OperationAction = SLO_Operation;
        rLA_OperationAction = RLA_Operation;
        sRE_OperationAction = SRE_Operation;
        rRA_OperationAction = RRA_Operation;
        dCP_OperationAction = DCP_Operation;
        iSC_OperationAction = ISC_Operation;
        arithmeticShiftLeftAction = ArithmeticShiftLeft;
        logicalShiftRightAction = LogicalShiftRight;
        rotateLeftAction = RotateLeft;
        rotateRightAction = RotateRight;
        incrementMemoryAction = IncrementMemory;
        decrementMemoryAction = DecrementMemory;

        // Function delegates for getValue operations
        getAAction = () => A;
        getXAction = () => X;
        getYAction = () => Y;
        getPAction = () => P;
        getAXAction = () => (byte)(A & X);
        getYHiAction = () => (byte)(Y & (hi + 1));
        getXHiAction = () => (byte)(X & (hi + 1));
        getAXHiAction = () => (byte)(A & X & (hi + 1));
        getAXSAction = () => (byte)(A & X & S);

        // Action delegates for stack operations
        plaAction = val => { A = val; SetZN(A); };
        plpAction = val => P = val;

        // Function delegates for branch conditions
        branchCarryClearAction = () => (P & C) == 0;
        branchCarrySetAction = () => (P & C) != 0;
        branchZeroClearAction = () => (P & Z) == 0;
        branchZeroSetAction = () => (P & Z) != 0;
        branchNegativeClearAction = () => (P & N) == 0;
        branchNegativeSetAction = () => (P & N) != 0;
        branchOverflowClearAction = () => (P & V) == 0;
        branchOverflowSetAction = () => (P & V) != 0;

        // Initialize continuation delegates for addressing modes
        readImmediateLoadAComplete = () => { val = BusComplete(); return LoadA; };
        readImmediateLoadXComplete = () => { val = BusComplete(); return LoadX; };
        readImmediateLoadYComplete = () => { val = BusComplete(); return LoadY; };
        readImmediateOrAccumulatorComplete = () => { val = BusComplete(); return OrAccumulator; };
        readImmediateAndAccumulatorComplete = () => { val = BusComplete(); return AndAccumulator; };
        readImmediateXorAccumulatorComplete = () => { val = BusComplete(); return XorAccumulator; };
        readImmediateAddWithCarryComplete = () => { val = BusComplete(); return AddWithCarry; };
        readImmediateSubtractWithCarryComplete = () => { val = BusComplete(); return SubtractWithCarry; };
        readImmediateCompareAComplete = () => { val = BusComplete(); return CompareA; };
        readImmediateCompareXComplete = () => { val = BusComplete(); return CompareX; };
        readImmediateCompareYComplete = () => { val = BusComplete(); return CompareY; };
        readImmediateNopComplete = () => { val = BusComplete(); return fetchOpcodeAction; };
        readImmediateLAXComplete = () => { val = BusComplete(); return LAX_Operation; };
        readImmediateANCComplete = () => { val = BusComplete(); return ANC_Operation; };
        readImmediateALRComplete = () => { val = BusComplete(); return ALR_Operation; };
        readImmediateARRComplete = () => { val = BusComplete(); return ARR_Operation; };
        readImmediateXAAComplete = () => { val = BusComplete(); return XAA_Operation; };
        readImmediateAXSComplete = () => { val = BusComplete(); return AXS_Operation; };

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
        return fetchOpcodeCompleteAction;
    }

    private CycleAction FetchOpcodeComplete()
    {
        byte opcode = BusComplete();
        return opcodeTable[opcode];
    }

    // Fully deduplicated zero page addressing mode implementation
    private CycleAction ReadZeroPageStep1Complete()
    {
        lo = BusComplete();
        BusRead(lo);
        return nextStepAction;
    }
}
