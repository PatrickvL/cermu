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
    // OPERATION IMPLEMENTATIONS
    private CycleAction LoadA() { A = val; SetZN(A); return fetchOpcodeAction; }
    private CycleAction LoadX() { X = val; SetZN(X); return fetchOpcodeAction; }
    private CycleAction LoadY() { Y = val; SetZN(Y); return fetchOpcodeAction; }

    private CycleAction TransferAX() { X = A; SetZN(X); return fetchOpcodeAction; }
    private CycleAction TransferXA() { A = X; SetZN(A); return fetchOpcodeAction; }
    private CycleAction TransferAY() { Y = A; SetZN(Y); return fetchOpcodeAction; }
    private CycleAction TransferYA() { A = Y; SetZN(A); return fetchOpcodeAction; }
    private CycleAction TransferSX() { X = S; SetZN(X); return fetchOpcodeAction; }
    private CycleAction TransferXS() { S = X; return fetchOpcodeAction; }

    private CycleAction AddWithCarry()
    {
        int result = A + val + ((P & C) != 0 ? 1 : 0);
        P = ((A ^ result) & (val ^ result) & 0x80) != 0 ? (byte)(P | V) : (byte)(P & ~V);
        P = result > 255 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)result;
        SetZN(A);
        return fetchOpcodeAction;
    }

    private CycleAction SubtractWithCarry()
    {
        val = (byte)~val;
        int result = A + val + ((P & C) != 0 ? 1 : 0);
        P = ((A ^ result) & (val ^ result) & 0x80) != 0 ? (byte)(P | V) : (byte)(P & ~V);
        P = result > 255 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)result;
        SetZN(A);
        return fetchOpcodeAction;
    }

    private CycleAction AndAccumulator() { A &= val; SetZN(A); return fetchOpcodeAction; }
    private CycleAction OrAccumulator() { A |= val; SetZN(A); return fetchOpcodeAction; }
    private CycleAction XorAccumulator() { A ^= val; SetZN(A); return fetchOpcodeAction; }

    private CycleAction CompareA() { P = A >= val ? (byte)(P | C) : (byte)(P & ~C); SetZN((byte)(A - val)); return fetchOpcodeAction; }
    private CycleAction CompareX() { P = X >= val ? (byte)(P | C) : (byte)(P & ~C); SetZN((byte)(X - val)); return fetchOpcodeAction; }
    private CycleAction CompareY() { P = Y >= val ? (byte)(P | C) : (byte)(P & ~C); SetZN((byte)(Y - val)); return fetchOpcodeAction; }

    private CycleAction IncrementX() { X++; SetZN(X); return fetchOpcodeAction; }
    private CycleAction IncrementY() { Y++; SetZN(Y); return fetchOpcodeAction; }
    private CycleAction DecrementX() { X--; SetZN(X); return fetchOpcodeAction; }
    private CycleAction DecrementY() { Y--; SetZN(Y); return fetchOpcodeAction; }

    private CycleAction SetCarry() { P = (byte)(P | C); return fetchOpcodeAction; }
    private CycleAction ClearCarry() { P = (byte)(P & ~C); return fetchOpcodeAction; }
    private CycleAction SetInterrupt() { P = (byte)(P | I); return fetchOpcodeAction; }
    private CycleAction ClearInterrupt() { P = (byte)(P & ~I); return fetchOpcodeAction; }
    private CycleAction SetDecimal() { P = (byte)(P | D); return fetchOpcodeAction; }
    private CycleAction ClearDecimal() { P = (byte)(P & ~D); return fetchOpcodeAction; }
    private CycleAction ClearOverflow() { P = (byte)(P & ~V); return fetchOpcodeAction; }

    private CycleAction JumpAbsolute()
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                PC = (ushort)(lo | (hi << 8));
                return fetchOpcodeAction;
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
                        return fetchOpcodeAction;
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
                            return fetchOpcodeAction;
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
                            return fetchOpcodeAction;
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
                            return fetchOpcodeAction;
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
                                P = (byte)(P | I);
                                return fetchOpcodeAction;
                            };
                        };
                    };
                };
            };
        };
    }

    // SHIFT/ROTATE OPERATIONS
    private CycleAction ArithmeticShiftLeftA()
    {
        P = (A & 0x80) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)(A << 1);
        SetZN(A);
        return fetchOpcodeAction;
    }

    private byte ArithmeticShiftLeft(byte value)
    {
        P = (value & 0x80) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        byte result = (byte)(value << 1);
        SetZN(result);
        return result;
    }

    private CycleAction LogicalShiftRightA()
    {
        P = (A & 0x01) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)(A >> 1);
        SetZN(A);
        return fetchOpcodeAction;
    }

    private byte LogicalShiftRight(byte value)
    {
        P = (value & 0x01) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        byte result = (byte)(value >> 1);
        SetZN(result);
        return result;
    }

    private CycleAction RotateLeftA()
    {
        bool carry = (A & 0x80) != 0;
        A = (byte)((A << 1) | ((P & C) != 0 ? 1 : 0));
        P = carry ? (byte)(P | C) : (byte)(P & ~C);
        SetZN(A);
        return fetchOpcodeAction;
    }

    private byte RotateLeft(byte value)
    {
        bool carry = (value & 0x80) != 0;
        byte result = (byte)((value << 1) | ((P & C) != 0 ? 1 : 0));
        P = carry ? (byte)(P | C) : (byte)(P & ~C);
        SetZN(result);
        return result;
    }

    private CycleAction RotateRightA()
    {
        bool carry = (A & 0x01) != 0;
        A = (byte)((A >> 1) | ((P & C) != 0 ? 0x80 : 0));
        P = carry ? (byte)(P | C) : (byte)(P & ~C);
        SetZN(A);
        return fetchOpcodeAction;
    }

    private byte RotateRight(byte value)
    {
        bool carry = (value & 0x01) != 0;
        byte result = (byte)((value >> 1) | ((P & C) != 0 ? 0x80 : 0));
        P = carry ? (byte)(P | C) : (byte)(P & ~C);
        SetZN(result);
        return result;
    }

    // INCREMENT/DECREMENT MEMORY
    private byte IncrementMemory(byte value)
    {
        byte result = (byte)(value + 1);
        SetZN(result);
        return result;
    }

    private byte DecrementMemory(byte value)
    {
        byte result = (byte)(value - 1);
        SetZN(result);
        return result;
    }

    // BIT TEST
    private CycleAction BitTest()
    {
        P = (val & 0x80) != 0 ? (byte)(P | N) : (byte)(P & ~N);
        P = (val & 0x40) != 0 ? (byte)(P | V) : (byte)(P & ~V);
        P = (A & val) == 0 ? (byte)(P | Z) : (byte)(P & ~Z);
        return fetchOpcodeAction;
    }

    // MODIFY ADDRESSING MODES (Read-Modify-Write)
    private CycleAction ModifyZeroPage(Func<byte, byte> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo);
            return () => {
                val = BusComplete();
                BusWrite(lo, val); // Dummy write
                return () => {
                    BusComplete();
                    val = operation(val);
                    BusWrite(lo, val);
                    return () => {
                        BusComplete();
                        return fetchOpcodeAction;
                    };
                };
            };
        };
    }

    private CycleAction ModifyZeroPageX(Func<byte, byte> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo); // Dummy read
            return () => {
                BusComplete();
                byte addr = (byte)(lo + X);
                BusRead(addr);
                return () => {
                    val = BusComplete();
                    BusWrite(addr, val); // Dummy write
                    return () => {
                        BusComplete();
                        val = operation(val);
                        BusWrite(addr, val);
                        return () => {
                            BusComplete();
                            return fetchOpcodeAction;
                        };
                    };
                };
            };
        };
    }

    private CycleAction ModifyAbsolute(Func<byte, byte> operation)
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
                    BusWrite(addr, val); // Dummy write
                    return () => {
                        BusComplete();
                        val = operation(val);
                        BusWrite(addr, val);
                        return () => {
                            BusComplete();
                            return fetchOpcodeAction;
                        };
                    };
                };
            };
        };
    }

    private CycleAction ModifyAbsoluteX(Func<byte, byte> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                ushort indexedAddr = (ushort)(addr + X);
                BusRead(addr); // Always dummy read for RMW
                return () => {
                    BusComplete();
                    BusRead(indexedAddr);
                    return () => {
                        val = BusComplete();
                        BusWrite(indexedAddr, val); // Dummy write
                        return () => {
                            BusComplete();
                            val = operation(val);
                            BusWrite(indexedAddr, val);
                            return () => {
                                BusComplete();
                                return fetchOpcodeAction;
                            };
                        };
                    };
                };
            };
        };
    }

    // ADDITIONAL ADDRESSING MODES FOR ILLEGAL OPCODES
    private CycleAction ModifyZeroPageIndexedIndirect(Func<byte, byte> operation) // (zp,X)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo); // Dummy read
            return () => {
                BusComplete();
                byte zpAddr = (byte)(lo + X);
                BusRead(zpAddr);
                return () => {
                    byte addrLo = BusComplete();
                    BusRead((byte)(zpAddr + 1));
                    return () => {
                        byte addrHi = BusComplete();
                        addr = (ushort)(addrLo | (addrHi << 8));
                        BusRead(addr);
                        return () => {
                            val = BusComplete();
                            BusWrite(addr, val); // Dummy write
                            return () => {
                                BusComplete();
                                val = operation(val);
                                BusWrite(addr, val);
                                return () => {
                                    BusComplete();
                                    return fetchOpcodeAction;
                                };
                            };
                        };
                    };
                };
            };
        };
    }

    private CycleAction ModifyZeroPageIndirectIndexed(Func<byte, byte> operation) // (zp),Y
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo);
            return () => {
                byte addrLo = BusComplete();
                BusRead((byte)(lo + 1));
                return () => {
                    byte addrHi = BusComplete();
                    addr = (ushort)(addrLo | (addrHi << 8));
                    ushort indexedAddr = (ushort)(addr + Y);
                    BusRead(addr); // Always dummy read for RMW
                    return () => {
                        BusComplete();
                        BusRead(indexedAddr);
                        return () => {
                            val = BusComplete();
                            BusWrite(indexedAddr, val); // Dummy write
                            return () => {
                                BusComplete();
                                val = operation(val);
                                BusWrite(indexedAddr, val);
                                return () => {
                                    BusComplete();
                                    return fetchOpcodeAction;
                                };
                            };
                        };
                    };
                };
            };
        };
    }

    private CycleAction ModifyAbsoluteY(Func<byte, byte> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                ushort indexedAddr = (ushort)(addr + Y);
                BusRead(addr); // Always dummy read for RMW
                return () => {
                    BusComplete();
                    BusRead(indexedAddr);
                    return () => {
                        val = BusComplete();
                        BusWrite(indexedAddr, val); // Dummy write
                        return () => {
                            BusComplete();
                            val = operation(val);
                            BusWrite(indexedAddr, val);
                            return () => {
                                BusComplete();
                                return fetchOpcodeAction;
                            };
                        };
                    };
                };
            };
        };
    }

    // ILLEGAL OPCODE OPERATIONS - MOS 6510
    private byte SLO_Operation(byte value) // Shift Left and OR
    {
        P = (value & 0x80) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        value = (byte)(value << 1);
        A |= value;
        SetZN(A);
        return value;
    }

    private byte RLA_Operation(byte value) // Rotate Left and AND
    {
        bool carry = (value & 0x80) != 0;
        value = (byte)((value << 1) | ((P & C) != 0 ? 1 : 0));
        P = carry ? (byte)(P | C) : (byte)(P & ~C);
        A &= value;
        SetZN(A);
        return value;
    }

    private byte SRE_Operation(byte value) // Shift Right and EOR
    {
        P = (value & 0x01) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        value = (byte)(value >> 1);
        A ^= value;
        SetZN(A);
        return value;
    }

    private byte RRA_Operation(byte value) // Rotate Right and Add
    {
        bool carry = (value & 0x01) != 0;
        value = (byte)((value >> 1) | ((P & C) != 0 ? 0x80 : 0));
        P = carry ? (byte)(P | C) : (byte)(P & ~C);
        int result = A + value + ((P & C) != 0 ? 1 : 0);
        P = ((A ^ result) & (value ^ result) & 0x80) != 0 ? (byte)(P | V) : (byte)(P & ~V);
        P = result > 255 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)result;
        SetZN(A);
        return value;
    }

    private byte DCP_Operation(byte value) // Decrement and Compare
    {
        value--;
        P = A >= value ? (byte)(P | C) : (byte)(P & ~C);
        SetZN((byte)(A - value));
        return value;
    }

    private byte ISC_Operation(byte value) // Increment and Subtract with Carry
    {
        value++;
        val = (byte)~value;
        int result = A + val + ((P & C) != 0 ? 1 : 0);
        P = ((A ^ result) & (val ^ result) & 0x80) != 0 ? (byte)(P | V) : (byte)(P & ~V);
        P = result > 255 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)result;
        SetZN(A);
        return value;
    }

    private CycleAction LAX_Operation() // Load A and X
    {
        A = val;
        X = val;
        SetZN(A);
        return fetchOpcodeAction;
    }

    private CycleAction ANC_Operation() // AND with Carry
    {
        A &= val;
        SetZN(A);
        P = (A & 0x80) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        return fetchOpcodeAction;
    }

    private CycleAction ALR_Operation() // AND and LSR
    {
        A &= val;
        P = (A & 0x01) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        A = (byte)(A >> 1);
        SetZN(A);
        return fetchOpcodeAction;
    }

    private CycleAction ARR_Operation() // AND and ROR
    {
        A &= val;
        A = (byte)((A >> 1) | ((P & C) != 0 ? 0x80 : 0));
        SetZN(A);
        P = (A & 0x40) != 0 ? (byte)(P | C) : (byte)(P & ~C);
        P = ((A ^ (A >> 1)) & 0x40) != 0 ? (byte)(P | V) : (byte)(P & ~V);
        return fetchOpcodeAction;
    }

    private CycleAction XAA_Operation() // X AND A (unstable)
    {
        A = (byte)(X & val);
        SetZN(A);
        return fetchOpcodeAction;
    }

    private CycleAction AXS_Operation() // A AND X minus value
    {
        int result = (A & X) - val;
        P = result >= 0 ? (byte)(P | C) : (byte)(P & ~C);
        X = (byte)result;
        SetZN(X);
        return fetchOpcodeAction;
    }

    private CycleAction LAS_Operation() // Load A, X, S
    {
        A = (byte)(val & S);
        X = A;
        S = A;
        SetZN(A);
        return fetchOpcodeAction;
    }
}
