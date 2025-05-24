using System;

namespace aiemu;

public class MOS6502
{
    private readonly C64Bus bus;
    public byte A, X, Y, S = 0xFF;
    public ushort PC = 0x0000;
    public byte P = 0x20;

    private delegate CycleAction CycleAction();
    private CycleAction nextCycle;
    private int totalCycles;

    // Status flags
    private const byte C = 0x01, Z = 0x02, I = 0x04, D = 0x08, B = 0x10, U = 0x20, V = 0x40, N = 0x80;

    // Shared state for addressing modes
    private byte lo, hi, val;
    private ushort addr;

    // Opcode table
    private readonly CycleAction[] opcodeTable = new CycleAction[256];

    public MOS6502(C64Bus bus)
    {
        this.bus = bus;
        nextCycle = FetchOpcode;
        InitializeOpcodeTable();
    }

    private void InitializeOpcodeTable()
    {
        // LOADS
        opcodeTable[0xA9] = () => ReadImmediate(() => LoadA);   // LDA #
        opcodeTable[0xAD] = () => ReadAbsolute(() => LoadA);    // LDA abs
        opcodeTable[0xA5] = () => ReadZeroPage(() => LoadA);    // LDA zp
        opcodeTable[0xA2] = () => ReadImmediate(() => LoadX);   // LDX #
        opcodeTable[0xAE] = () => ReadAbsolute(() => LoadX);    // LDX abs
        opcodeTable[0xA6] = () => ReadZeroPage(() => LoadX);    // LDX zp
        opcodeTable[0xA0] = () => ReadImmediate(() => LoadY);   // LDY #
        opcodeTable[0xAC] = () => ReadAbsolute(() => LoadY);    // LDY abs
        opcodeTable[0xA4] = () => ReadZeroPage(() => LoadY);    // LDY zp

        // STORES
        opcodeTable[0x8D] = () => WriteAbsolute(() => A);       // STA abs
        opcodeTable[0x85] = () => WriteZeroPage(() => A);       // STA zp
        opcodeTable[0x8E] = () => WriteAbsolute(() => X);       // STX abs
        opcodeTable[0x86] = () => WriteZeroPage(() => X);       // STX zp
        opcodeTable[0x8C] = () => WriteAbsolute(() => Y);       // STY abs
        opcodeTable[0x84] = () => WriteZeroPage(() => Y);       // STY zp

        // TRANSFERS
        opcodeTable[0xAA] = () => DummyRead(() => TransferAX);  // TAX
        opcodeTable[0x8A] = () => DummyRead(() => TransferXA);  // TXA
        opcodeTable[0xA8] = () => DummyRead(() => TransferAY);  // TAY
        opcodeTable[0x98] = () => DummyRead(() => TransferYA);  // TYA
        opcodeTable[0xBA] = () => DummyRead(() => TransferSX);  // TSX
        opcodeTable[0x9A] = () => DummyRead(() => TransferXS);  // TXS

        // ARITHMETIC
        opcodeTable[0x69] = () => ReadImmediate(() => AddWithCarry);     // ADC #
        opcodeTable[0x6D] = () => ReadAbsolute(() => AddWithCarry);      // ADC abs
        opcodeTable[0x65] = () => ReadZeroPage(() => AddWithCarry);      // ADC zp
        opcodeTable[0xE9] = () => ReadImmediate(() => SubtractWithCarry); // SBC #
        opcodeTable[0xED] = () => ReadAbsolute(() => SubtractWithCarry);  // SBC abs
        opcodeTable[0xE5] = () => ReadZeroPage(() => SubtractWithCarry);  // SBC zp

        // LOGICAL
        opcodeTable[0x29] = () => ReadImmediate(() => AndAccumulator);   // AND #
        opcodeTable[0x2D] = () => ReadAbsolute(() => AndAccumulator);    // AND abs
        opcodeTable[0x25] = () => ReadZeroPage(() => AndAccumulator);    // AND zp
        opcodeTable[0x09] = () => ReadImmediate(() => OrAccumulator);    // ORA #
        opcodeTable[0x0D] = () => ReadAbsolute(() => OrAccumulator);     // ORA abs
        opcodeTable[0x05] = () => ReadZeroPage(() => OrAccumulator);     // ORA zp
        opcodeTable[0x49] = () => ReadImmediate(() => XorAccumulator);   // EOR #
        opcodeTable[0x4D] = () => ReadAbsolute(() => XorAccumulator);    // EOR abs
        opcodeTable[0x45] = () => ReadZeroPage(() => XorAccumulator);    // EOR zp

        // COMPARES
        opcodeTable[0xC9] = () => ReadImmediate(() => CompareA);         // CMP #
        opcodeTable[0xCD] = () => ReadAbsolute(() => CompareA);          // CMP abs
        opcodeTable[0xC5] = () => ReadZeroPage(() => CompareA);          // CMP zp
        opcodeTable[0xE0] = () => ReadImmediate(() => CompareX);         // CPX #
        opcodeTable[0xEC] = () => ReadAbsolute(() => CompareX);          // CPX abs
        opcodeTable[0xE4] = () => ReadZeroPage(() => CompareX);          // CPX zp
        opcodeTable[0xC0] = () => ReadImmediate(() => CompareY);         // CPY #
        opcodeTable[0xCC] = () => ReadAbsolute(() => CompareY);          // CPY abs
        opcodeTable[0xC4] = () => ReadZeroPage(() => CompareY);          // CPY zp

        // BRANCHES
        opcodeTable[0xF0] = () => BranchIf(() => GetFlag(Z));           // BEQ
        opcodeTable[0xD0] = () => BranchIf(() => !GetFlag(Z));          // BNE
        opcodeTable[0xB0] = () => BranchIf(() => GetFlag(C));           // BCS
        opcodeTable[0x90] = () => BranchIf(() => !GetFlag(C));          // BCC
        opcodeTable[0x70] = () => BranchIf(() => GetFlag(V));           // BVS
        opcodeTable[0x50] = () => BranchIf(() => !GetFlag(V));          // BVC
        opcodeTable[0x30] = () => BranchIf(() => GetFlag(N));           // BMI
        opcodeTable[0x10] = () => BranchIf(() => !GetFlag(N));          // BPL

        // INCREMENTS/DECREMENTS
        opcodeTable[0xE8] = () => DummyRead(() => IncrementX);          // INX
        opcodeTable[0xC8] = () => DummyRead(() => IncrementY);          // INY
        opcodeTable[0xCA] = () => DummyRead(() => DecrementX);          // DEX
        opcodeTable[0x88] = () => DummyRead(() => DecrementY);          // DEY

        // STACK
        opcodeTable[0x48] = () => PushStack(() => A);                   // PHA
        opcodeTable[0x08] = () => PushStack(() => P);                   // PHP
        opcodeTable[0x68] = () => PullStack(() => val => { A = val; SetZN(A); }); // PLA
        opcodeTable[0x28] = () => PullStack(() => val => P = val);      // PLP

        // STATUS FLAGS
        opcodeTable[0x38] = () => DummyRead(() => SetCarry);            // SEC
        opcodeTable[0x18] = () => DummyRead(() => ClearCarry);          // CLC
        opcodeTable[0x78] = () => DummyRead(() => SetInterrupt);        // SEI
        opcodeTable[0x58] = () => DummyRead(() => ClearInterrupt);      // CLI
        opcodeTable[0xF8] = () => DummyRead(() => SetDecimal);          // SED
        opcodeTable[0xD8] = () => DummyRead(() => ClearDecimal);        // CLD
        opcodeTable[0xB8] = () => DummyRead(() => ClearOverflow);       // CLV

        // JUMPS
        opcodeTable[0x4C] = () => JumpAbsolute;                         // JMP abs
        opcodeTable[0x6C] = () => JumpIndirect;                         // JMP (abs)
        opcodeTable[0x20] = () => JumpSubroutine;                       // JSR
        opcodeTable[0x60] = () => ReturnSubroutine;                     // RTS
        opcodeTable[0x40] = () => ReturnInterrupt;                      // RTI

        // SYSTEM
        opcodeTable[0xEA] = () => DummyRead(() => FetchOpcode);         // NOP
        opcodeTable[0x00] = () => Break;                                // BRK

        // Fill unimplemented with NOP
        for (int i = 0; i < 256; i++)
            if (opcodeTable[i] == null)
                opcodeTable[i] = () => DummyRead(() => FetchOpcode);
    }

    private void SetZN(byte v) => P = (byte)((P & ~(Z | N)) | (v == 0 ? Z : 0) | (v & N));
    private bool GetFlag(byte flag) => (P & flag) != 0;
    private void SetFlag(byte flag, bool set) => P = set ? (byte)(P | flag) : (byte)(P & ~flag);

    private void BusRead(ushort address)
    {
        bus.AddressLines = address;
        bus.RW = true;
        bus.CS = true;
    }

    private void BusWrite(ushort address, byte data)
    {
        bus.AddressLines = address;
        bus.DataLines = data;
        bus.RW = false;
        bus.CS = true;
    }

    private byte BusComplete()
    {
        bus.CS = false;
        return bus.DataLines;
    }

    public bool Step()
    {
        totalCycles++;
        bus.Cycle();
        nextCycle = nextCycle();
        return nextCycle == FetchOpcode;
    }

    private CycleAction FetchOpcode()
    {
        BusRead(PC++);
        return () => {
            byte opcode = BusComplete();
            return opcodeTable[opcode];
        };
    }

    // SHARED ADDRESSING MODE HELPERS
    private CycleAction ReadImmediate(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            val = BusComplete();
            return operation();
        };
    }

    private CycleAction ReadZeroPage(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo);
            return () => {
                val = BusComplete();
                return operation();
            };
        };
    }

    private CycleAction ReadAbsolute(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                BusRead(addr);
                return () => {
                    val = BusComplete();
                    return operation();
                };
            };
        };
    }

    private CycleAction WriteZeroPage(Func<byte> getValue)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusWrite(lo, getValue());
            return () => {
                BusComplete();
                return FetchOpcode;
            };
        };
    }

    private CycleAction WriteAbsolute(Func<byte> getValue)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                BusWrite(addr, getValue());
                return () => {
                    BusComplete();
                    return FetchOpcode;
                };
            };
        };
    }

    private CycleAction DummyRead(Func<CycleAction> operation)
    {
        BusRead(PC);
        return () => {
            BusComplete();
            return operation();
        };
    }

    private CycleAction BranchIf(Func<bool> condition)
    {
        BusRead(PC++);
        return () => {
            sbyte offset = (sbyte)BusComplete();
            if (condition())
            {
                PC = (ushort)(PC + offset);
            }
            return FetchOpcode;
        };
    }

    private CycleAction PushStack(Func<byte> getValue)
    {
        BusRead(PC);
        return () => {
            BusComplete();
            BusWrite((ushort)(0x0100 + S--), getValue());
            return () => {
                BusComplete();
                return FetchOpcode;
            };
        };
    }

    private CycleAction PullStack(Func<Action<byte>> operation)
    {
        BusRead(PC);
        return () => {
            BusComplete();
            BusRead(PC);
            return () => {
                BusComplete();
                BusRead((ushort)(0x0100 + ++S));
                return () => {
                    val = BusComplete();
                    operation()(val);
                    return FetchOpcode;
                };
            };
        };
    }

    // OPERATION IMPLEMENTATIONS
    private CycleAction LoadA() { A = val; SetZN(A); return FetchOpcode; }
    private CycleAction LoadX() { X = val; SetZN(X); return FetchOpcode; }
    private CycleAction LoadY() { Y = val; SetZN(Y); return FetchOpcode; }

    private CycleAction TransferAX() { X = A; SetZN(X); return FetchOpcode; }
    private CycleAction TransferXA() { A = X; SetZN(A); return FetchOpcode; }
    private CycleAction TransferAY() { Y = A; SetZN(Y); return FetchOpcode; }
    private CycleAction TransferYA() { A = Y; SetZN(A); return FetchOpcode; }
    private CycleAction TransferSX() { X = S; SetZN(X); return FetchOpcode; }
    private CycleAction TransferXS() { S = X; return FetchOpcode; }

    private CycleAction AddWithCarry()
    {
        int result = A + val + (GetFlag(C) ? 1 : 0);
        SetFlag(V, ((A ^ result) & (val ^ result) & 0x80) != 0);
        SetFlag(C, result > 255);
        A = (byte)result;
        SetZN(A);
        return FetchOpcode;
    }

    private CycleAction SubtractWithCarry()
    {
        val = (byte)~val;
        int result = A + val + (GetFlag(C) ? 1 : 0);
        SetFlag(V, ((A ^ result) & (val ^ result) & 0x80) != 0);
        SetFlag(C, result > 255);
        A = (byte)result;
        SetZN(A);
        return FetchOpcode;
    }

    private CycleAction AndAccumulator() { A &= val; SetZN(A); return FetchOpcode; }
    private CycleAction OrAccumulator() { A |= val; SetZN(A); return FetchOpcode; }
    private CycleAction XorAccumulator() { A ^= val; SetZN(A); return FetchOpcode; }

    private CycleAction CompareA() { SetFlag(C, A >= val); SetZN((byte)(A - val)); return FetchOpcode; }
    private CycleAction CompareX() { SetFlag(C, X >= val); SetZN((byte)(X - val)); return FetchOpcode; }
    private CycleAction CompareY() { SetFlag(C, Y >= val); SetZN((byte)(Y - val)); return FetchOpcode; }

    private CycleAction IncrementX() { X++; SetZN(X); return FetchOpcode; }
    private CycleAction IncrementY() { Y++; SetZN(Y); return FetchOpcode; }
    private CycleAction DecrementX() { X--; SetZN(X); return FetchOpcode; }
    private CycleAction DecrementY() { Y--; SetZN(Y); return FetchOpcode; }

    private CycleAction SetCarry() { SetFlag(C, true); return FetchOpcode; }
    private CycleAction ClearCarry() { SetFlag(C, false); return FetchOpcode; }
    private CycleAction SetInterrupt() { SetFlag(I, true); return FetchOpcode; }
    private CycleAction ClearInterrupt() { SetFlag(I, false); return FetchOpcode; }
    private CycleAction SetDecimal() { SetFlag(D, true); return FetchOpcode; }
    private CycleAction ClearDecimal() { SetFlag(D, false); return FetchOpcode; }
    private CycleAction ClearOverflow() { SetFlag(V, false); return FetchOpcode; }

    private CycleAction JumpAbsolute()
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                PC = (ushort)(lo | (hi << 8));
                return FetchOpcode;
            };
        };
    }

    private CycleAction JumpIndirect()
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                BusRead(addr);
                return () => {
                    lo = BusComplete();
                    BusRead((ushort)(addr + 1));
                    return () => {
                        hi = BusComplete();
                        PC = (ushort)(lo | (hi << 8));
                        return FetchOpcode;
                    };
                };
            };
        };
    }

    private CycleAction JumpSubroutine()
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead((ushort)(0x0100 + S));
            return () => {
                BusComplete();
                BusWrite((ushort)(0x0100 + S--), (byte)(PC >> 8));
                return () => {
                    BusComplete();
                    BusWrite((ushort)(0x0100 + S--), (byte)PC);
                    return () => {
                        BusComplete();
                        BusRead(PC++);
                        return () => {
                            hi = BusComplete();
                            PC = (ushort)(lo | (hi << 8));
                            return FetchOpcode;
                        };
                    };
                };
            };
        };
    }

    private CycleAction ReturnSubroutine()
    {
        BusRead(PC);
        return () => {
            BusComplete();
            BusRead((ushort)(0x0100 + S));
            return () => {
                BusComplete();
                BusRead((ushort)(0x0100 + ++S));
                return () => {
                    lo = BusComplete();
                    BusRead((ushort)(0x0100 + ++S));
                    return () => {
                        hi = BusComplete();
                        PC = (ushort)(lo | (hi << 8));
                        BusRead(PC++);
                        return () => {
                            BusComplete();
                            return FetchOpcode;
                        };
                    };
                };
            };
        };
    }

    private CycleAction ReturnInterrupt()
    {
        BusRead(PC);
        return () => {
            BusComplete();
            BusRead((ushort)(0x0100 + S));
            return () => {
                BusComplete();
                BusRead((ushort)(0x0100 + ++S));
                return () => {
                    P = BusComplete();
                    BusRead((ushort)(0x0100 + ++S));
                    return () => {
                        lo = BusComplete();
                        BusRead((ushort)(0x0100 + ++S));
                        return () => {
                            hi = BusComplete();
                            PC = (ushort)(lo | (hi << 8));
                            return FetchOpcode;
                        };
                    };
                };
            };
        };
    }

    private CycleAction Break()
    {
        BusRead(PC++);
        return () => {
            BusComplete();
            BusWrite((ushort)(0x0100 + S--), (byte)(PC >> 8));
            return () => {
                BusComplete();
                BusWrite((ushort)(0x0100 + S--), (byte)PC);
                return () => {
                    BusComplete();
                    BusWrite((ushort)(0x0100 + S--), (byte)(P | B));
                    return () => {
                        BusComplete();
                        BusRead(0xFFFE);
                        return () => {
                            lo = BusComplete();
                            BusRead(0xFFFF);
                            return () => {
                                hi = BusComplete();
                                PC = (ushort)(lo | (hi << 8));
                                SetFlag(I, true);
                                return FetchOpcode;
                            };
                        };
                    };
                };
            };
        };
    }
}
