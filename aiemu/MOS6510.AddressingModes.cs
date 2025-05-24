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
                return fetchOpcodeAction;
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
                    return fetchOpcodeAction;
                };
            };
        };
    }

    // WRITE INDEXED ADDRESSING MODES
    private CycleAction WriteAbsoluteX(Func<byte> getValue)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                ushort indexedAddr = (ushort)(addr + X);
                BusRead(addr); // Always dummy read for writes
                return () => {
                    BusComplete();
                    BusWrite(indexedAddr, getValue());
                    return () => {
                        BusComplete();
                        return fetchOpcodeAction;
                    };
                };
            };
        };
    }

    private CycleAction WriteAbsoluteY(Func<byte> getValue)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                ushort indexedAddr = (ushort)(addr + Y);
                BusRead(addr); // Always dummy read for writes
                return () => {
                    BusComplete();
                    BusWrite(indexedAddr, getValue());
                    return () => {
                        BusComplete();
                        return fetchOpcodeAction;
                    };
                };
            };
        };
    }

    private CycleAction WriteZeroPageX(Func<byte> getValue)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo); // Dummy read
            return () => {
                BusComplete();
                BusWrite((byte)(lo + X), getValue());
                return () => {
                    BusComplete();
                    return fetchOpcodeAction;
                };
            };
        };
    }

    private CycleAction WriteZeroPageY(Func<byte> getValue)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo); // Dummy read
            return () => {
                BusComplete();
                BusWrite((byte)(lo + Y), getValue());
                return () => {
                    BusComplete();
                    return fetchOpcodeAction;
                };
            };
        };
    }

    private CycleAction WriteZeroPageIndexedIndirect(Func<byte> getValue) // (zp,X)
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
                        BusWrite(addr, getValue());
                        return () => {
                            BusComplete();
                            return fetchOpcodeAction;
                        };
                    };
                };
            };
        };
    }

    private CycleAction WriteZeroPageIndirectIndexed(Func<byte> getValue) // (zp),Y
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
                    BusRead(addr); // Always dummy read for writes
                    return () => {
                        BusComplete();
                        BusWrite(indexedAddr, getValue());
                        return () => {
                            BusComplete();
                            return fetchOpcodeAction;
                        };
                    };
                };
            };
        };
    }

    // INDEXED ADDRESSING MODES
    private CycleAction ReadAbsoluteX(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                ushort indexedAddr = (ushort)(addr + X);
                if ((addr & 0xFF00) != (indexedAddr & 0xFF00)) {
                    // Page crossed - dummy read from addr before indexedAddr
                    BusRead(addr);
                    return () => {
                        BusComplete();
                        BusRead(indexedAddr);
                        return () => {
                            val = BusComplete();
                            return operation();
                        };
                    };
                } else {
                    BusRead(indexedAddr);
                    return () => {
                        val = BusComplete();
                        return operation();
                    };
                }
            };
        };
    }

    private CycleAction ReadAbsoluteY(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(PC++);
            return () => {
                hi = BusComplete();
                addr = (ushort)(lo | (hi << 8));
                ushort indexedAddr = (ushort)(addr + Y);
                if ((addr & 0xFF00) != (indexedAddr & 0xFF00)) {
                    // Page crossed - dummy read from addr before indexedAddr
                    BusRead(addr);
                    return () => {
                        BusComplete();
                        BusRead(indexedAddr);
                        return () => {
                            val = BusComplete();
                            return operation();
                        };
                    };
                } else {
                    BusRead(indexedAddr);
                    return () => {
                        val = BusComplete();
                        return operation();
                    };
                }
            };
        };
    }

    private CycleAction ReadZeroPageX(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo); // Dummy read
            return () => {
                BusComplete();
                BusRead((byte)(lo + X));
                return () => {
                    val = BusComplete();
                    return operation();
                };
            };
        };
    }

    private CycleAction ReadZeroPageY(Func<CycleAction> operation)
    {
        BusRead(PC++);
        return () => {
            lo = BusComplete();
            BusRead(lo); // Dummy read
            return () => {
                BusComplete();
                BusRead((byte)(lo + Y));
                return () => {
                    val = BusComplete();
                    return operation();
                };
            };
        };
    }

    private CycleAction ReadZeroPageIndexedIndirect(Func<CycleAction> operation) // (zp,X)
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
                            return operation();
                        };
                    };
                };
            };
        };
    }

    private CycleAction ReadZeroPageIndirectIndexed(Func<CycleAction> operation) // (zp),Y
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
                    if ((addr & 0xFF00) != (indexedAddr & 0xFF00)) {
                        // Page crossed - dummy read from addr before indexedAddr
                        BusRead(addr);
                        return () => {
                            BusComplete();
                            BusRead(indexedAddr);
                            return () => {
                                val = BusComplete();
                                return operation();
                            };
                        };
                    } else {
                        BusRead(indexedAddr);
                        return () => {
                            val = BusComplete();
                            return operation();
                        };
                    }
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
            return fetchOpcodeAction;
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
                return fetchOpcodeAction;
            };
        };
    }

    private CycleAction PullStack(Action<byte> operation)
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
                    operation(val);
                    return fetchOpcodeAction;
                };
            };
        };
    }
}
