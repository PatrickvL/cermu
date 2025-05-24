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
    // Opcode table - optimized with function pointers
    private readonly CycleAction[] opcodeTable = new CycleAction[256];
    
    private void InitializeOpcodeTable()
    {
        // Complete 256-entry opcode table in ascending order
        // MOS 6502/6510 compatible with version annotations
        
        opcodeTable[0x00] = () => Break;                                                   // BRK - 6502
        opcodeTable[0x01] = () => ReadZeroPageIndexedIndirect(() => orAccumulatorAction);  // ORA (zp,X) - 6502
        opcodeTable[0x02] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x03] = () => ModifyZeroPageIndexedIndirect(sLO_OperationAction);      // *SLO (zp,X) - 6510 (illegal)
        opcodeTable[0x04] = () => ReadZeroPage(() => fetchOpcodeAction);                   // *NOP zp - 6510 (illegal)
        opcodeTable[0x05] = () => ReadZeroPage(() => orAccumulatorAction);                 // ORA zp - 6502
        opcodeTable[0x06] = () => ModifyZeroPage(ArithmeticShiftLeft);                     // ASL zp - 6502
        opcodeTable[0x07] = () => ModifyZeroPage(sLO_OperationAction);                     // *SLO zp - 6510 (illegal)
        opcodeTable[0x08] = () => PushStack(() => P);                                      // PHP - 6502
        opcodeTable[0x09] = () => ReadImmediate(() => orAccumulatorAction);                // ORA # - 6502
        opcodeTable[0x0A] = () => DummyRead(() => ArithmeticShiftLeftA);                   // ASL A - 6502
        opcodeTable[0x0B] = () => ReadImmediate(() => ANC_Operation);                      // *ANC # - 6510 (illegal)
        opcodeTable[0x0C] = () => ReadAbsolute(() => fetchOpcodeAction);                   // *NOP abs - 6510 (illegal)
        opcodeTable[0x0D] = () => ReadAbsolute(() => orAccumulatorAction);                 // ORA abs - 6502
        opcodeTable[0x0E] = () => ModifyAbsolute(ArithmeticShiftLeft);                     // ASL abs - 6502
        opcodeTable[0x0F] = () => ModifyAbsolute(sLO_OperationAction);                     // *SLO abs - 6510 (illegal)
 
        opcodeTable[0x10] = () => BranchIf(() => (P & N) == 0);                            // BPL - 6502
        opcodeTable[0x11] = () => ReadZeroPageIndirectIndexed(() => orAccumulatorAction);  // ORA (zp),Y - 6502
        opcodeTable[0x12] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x13] = () => ModifyZeroPageIndirectIndexed(sLO_OperationAction);      // *SLO (zp),Y - 6510 (illegal)
        opcodeTable[0x14] = () => ReadZeroPageX(() => fetchOpcodeAction);                  // *NOP zp,X - 6510 (illegal)
        opcodeTable[0x15] = () => ReadZeroPageX(() => orAccumulatorAction);                // ORA zp,X - 6502
        opcodeTable[0x16] = () => ModifyZeroPageX(ArithmeticShiftLeft);                    // ASL zp,X - 6502
        opcodeTable[0x17] = () => ModifyZeroPageX(sLO_OperationAction);                    // *SLO zp,X - 6510 (illegal)
        opcodeTable[0x18] = () => DummyRead(() => ClearCarry);                             // CLC - 6502
        opcodeTable[0x19] = () => ReadAbsoluteY(() => orAccumulatorAction);                // ORA abs,Y - 6502
        opcodeTable[0x1A] = () => DummyRead(() => fetchOpcodeAction);                      // *NOP - 6510 (illegal)
        opcodeTable[0x1B] = () => ModifyAbsoluteY(sLO_OperationAction);                    // *SLO abs,Y - 6510 (illegal)
        opcodeTable[0x1C] = () => ReadAbsoluteX(() => fetchOpcodeAction);                  // *NOP abs,X - 6510 (illegal)
        opcodeTable[0x1D] = () => ReadAbsoluteX(() => orAccumulatorAction);                // ORA abs,X - 6502
        opcodeTable[0x1E] = () => ModifyAbsoluteX(ArithmeticShiftLeft);                    // ASL abs,X - 6502
        opcodeTable[0x1F] = () => ModifyAbsoluteX(sLO_OperationAction);                    // *SLO abs,X - 6510 (illegal)

        opcodeTable[0x20] = () => JumpSubroutine;                                          // JSR - 6502
        opcodeTable[0x21] = () => ReadZeroPageIndexedIndirect(() => andAccumulatorAction); // AND (zp,X) - 6502
        opcodeTable[0x22] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x23] = () => ModifyZeroPageIndexedIndirect(RLA_Operation);            // *RLA (zp,X) - 6510 (illegal)
        opcodeTable[0x24] = () => ReadZeroPage(() => BitTest);                             // BIT zp - 6502
        opcodeTable[0x25] = () => ReadZeroPage(() => andAccumulatorAction);                // AND zp - 6502
        opcodeTable[0x26] = () => ModifyZeroPage(RotateLeft);                              // ROL zp - 6502
        opcodeTable[0x27] = () => ModifyZeroPage(RLA_Operation);                           // *RLA zp - 6510 (illegal)
        opcodeTable[0x28] = () => PullStack(plpAction);                                    // PLP - 6502
        opcodeTable[0x29] = () => ReadImmediate(() => andAccumulatorAction);               // AND # - 6502
        opcodeTable[0x2A] = () => DummyRead(() => RotateLeftA);                            // ROL A - 6502
        opcodeTable[0x2B] = () => ReadImmediate(() => ANC_Operation);                      // *ANC # - 6510 (illegal)
        opcodeTable[0x2C] = () => ReadAbsolute(() => BitTest);                             // BIT abs - 6502
        opcodeTable[0x2D] = () => ReadAbsolute(() => andAccumulatorAction);                // AND abs - 6502
        opcodeTable[0x2E] = () => ModifyAbsolute(RotateLeft);                              // ROL abs - 6502
        opcodeTable[0x2F] = () => ModifyAbsolute(RLA_Operation);                           // *RLA abs - 6510 (illegal)

        opcodeTable[0x30] = () => BranchIf(() => (P & N) != 0);                            // BMI - 6502
        opcodeTable[0x31] = () => ReadZeroPageIndirectIndexed(() => andAccumulatorAction); // AND (zp),Y - 6502
        opcodeTable[0x32] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x33] = () => ModifyZeroPageIndirectIndexed(RLA_Operation);            // *RLA (zp),Y - 6510 (illegal)
        opcodeTable[0x34] = () => ReadZeroPageX(() => fetchOpcodeAction);                  // *NOP zp,X - 6510 (illegal)
        opcodeTable[0x35] = () => ReadZeroPageX(() => andAccumulatorAction);               // AND zp,X - 6502
        opcodeTable[0x36] = () => ModifyZeroPageX(RotateLeft);                             // ROL zp,X - 6502
        opcodeTable[0x37] = () => ModifyZeroPageX(RLA_Operation);                          // *RLA zp,X - 6510 (illegal)
        opcodeTable[0x38] = () => DummyRead(() => SetCarry);                               // SEC - 6502
        opcodeTable[0x39] = () => ReadAbsoluteY(() => andAccumulatorAction);               // AND abs,Y - 6502
        opcodeTable[0x3A] = () => DummyRead(() => fetchOpcodeAction);                      // *NOP - 6510 (illegal)
        opcodeTable[0x3B] = () => ModifyAbsoluteY(RLA_Operation);                          // *RLA abs,Y - 6510 (illegal)
        opcodeTable[0x3C] = () => ReadAbsoluteX(() => fetchOpcodeAction);                  // *NOP abs,X - 6510 (illegal)
        opcodeTable[0x3D] = () => ReadAbsoluteX(() => andAccumulatorAction);               // AND abs,X - 6502
        opcodeTable[0x3E] = () => ModifyAbsoluteX(RotateLeft);                             // ROL abs,X - 6502
        opcodeTable[0x3F] = () => ModifyAbsoluteX(RLA_Operation);                          // *RLA abs,X - 6510 (illegal)

        opcodeTable[0x40] = () => ReturnInterrupt;                                         // RTI - 6502
        opcodeTable[0x41] = () => ReadZeroPageIndexedIndirect(() => xorAccumulatorAction); // EOR (zp,X) - 6502
        opcodeTable[0x42] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x43] = () => ModifyZeroPageIndexedIndirect(SRE_Operation);            // *SRE (zp,X) - 6510 (illegal)
        opcodeTable[0x44] = () => ReadZeroPage(() => fetchOpcodeAction);                   // *NOP zp - 6510 (illegal)
        opcodeTable[0x45] = () => ReadZeroPage(() => xorAccumulatorAction);                // EOR zp - 6502
        opcodeTable[0x46] = () => ModifyZeroPage(LogicalShiftRight);                       // LSR zp - 6502
        opcodeTable[0x47] = () => ModifyZeroPage(SRE_Operation);                           // *SRE zp - 6510 (illegal)
        opcodeTable[0x48] = () => PushStack(() => A);                                      // PHA - 6502
        opcodeTable[0x49] = () => ReadImmediate(() => xorAccumulatorAction);               // EOR # - 6502
        opcodeTable[0x4A] = () => DummyRead(() => LogicalShiftRightA);                     // LSR A - 6502
        opcodeTable[0x4B] = () => ReadImmediate(() => ALR_Operation);                      // *ALR # - 6510 (illegal)
        opcodeTable[0x4C] = () => JumpAbsolute;                                            // JMP abs - 6502
        opcodeTable[0x4D] = () => ReadAbsolute(() => xorAccumulatorAction);                // EOR abs - 6502
        opcodeTable[0x4E] = () => ModifyAbsolute(LogicalShiftRight);                       // LSR abs - 6502
        opcodeTable[0x4F] = () => ModifyAbsolute(SRE_Operation);                           // *SRE abs - 6510 (illegal)
      
        opcodeTable[0x50] = () => BranchIf(() => (P & V) == 0);                            // BVC - 6502
        opcodeTable[0x51] = () => ReadZeroPageIndirectIndexed(() => xorAccumulatorAction); // EOR (zp),Y - 6502
        opcodeTable[0x52] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x53] = () => ModifyZeroPageIndirectIndexed(SRE_Operation);            // *SRE (zp),Y - 6510 (illegal)
        opcodeTable[0x54] = () => ReadZeroPageX(() => fetchOpcodeAction);                  // *NOP zp,X - 6510 (illegal)
        opcodeTable[0x55] = () => ReadZeroPageX(() => xorAccumulatorAction);               // EOR zp,X - 6502
        opcodeTable[0x56] = () => ModifyZeroPageX(LogicalShiftRight);                      // LSR zp,X - 6502
        opcodeTable[0x57] = () => ModifyZeroPageX(SRE_Operation);                          // *SRE zp,X - 6510 (illegal)
        opcodeTable[0x58] = () => DummyRead(() => ClearInterrupt);                         // CLI - 6502
        opcodeTable[0x59] = () => ReadAbsoluteY(() => xorAccumulatorAction);               // EOR abs,Y - 6502
        opcodeTable[0x5A] = () => DummyRead(() => fetchOpcodeAction);                      // *NOP - 6510 (illegal)
        opcodeTable[0x5B] = () => ModifyAbsoluteY(SRE_Operation);                          // *SRE abs,Y - 6510 (illegal)
        opcodeTable[0x5C] = () => ReadAbsoluteX(() => fetchOpcodeAction);                  // *NOP abs,X - 6510 (illegal)
        opcodeTable[0x5D] = () => ReadAbsoluteX(() => xorAccumulatorAction);               // EOR abs,X - 6502
        opcodeTable[0x5E] = () => ModifyAbsoluteX(LogicalShiftRight);                      // LSR abs,X - 6502
        opcodeTable[0x5F] = () => ModifyAbsoluteX(SRE_Operation);                          // *SRE abs,X - 6510 (illegal)

        opcodeTable[0x60] = () => ReturnSubroutine;                                        // RTS - 6502
        opcodeTable[0x61] = () => ReadZeroPageIndexedIndirect(() => addWithCarryAction);   // ADC (zp,X) - 6502
        opcodeTable[0x62] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x63] = () => ModifyZeroPageIndexedIndirect(RRA_Operation);            // *RRA (zp,X) - 6510 (illegal)
        opcodeTable[0x64] = () => ReadZeroPage(() => fetchOpcodeAction);                   // *NOP zp - 6510 (illegal)
        opcodeTable[0x65] = () => ReadZeroPage(() => addWithCarryAction);                  // ADC zp - 6502
        opcodeTable[0x66] = () => ModifyZeroPage(RotateRight);                             // ROR zp - 6502
        opcodeTable[0x67] = () => ModifyZeroPage(RRA_Operation);                           // *RRA zp - 6510 (illegal)
        opcodeTable[0x68] = () => PullStack(plaAction);                                    // PLA - 6502
        opcodeTable[0x69] = () => ReadImmediate(() => addWithCarryAction);                 // ADC # - 6502
        opcodeTable[0x6A] = () => DummyRead(() => RotateRightA);                           // ROR A - 6502
        opcodeTable[0x6B] = () => ReadImmediate(() => ARR_Operation);                      // *ARR # - 6510 (illegal)
        opcodeTable[0x6C] = () => JumpIndirect;                                            // JMP (abs) - 6502
        opcodeTable[0x6D] = () => ReadAbsolute(() => addWithCarryAction);                  // ADC abs - 6502
        opcodeTable[0x6E] = () => ModifyAbsolute(RotateRight);                             // ROR abs - 6502
        opcodeTable[0x6F] = () => ModifyAbsolute(RRA_Operation);                           // *RRA abs - 6510 (illegal)

        opcodeTable[0x70] = () => BranchIf(() => (P & V) != 0);                            // BVS - 6502
        opcodeTable[0x71] = () => ReadZeroPageIndirectIndexed(() => addWithCarryAction);   // ADC (zp),Y - 6502
        opcodeTable[0x72] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x73] = () => ModifyZeroPageIndirectIndexed(RRA_Operation);            // *RRA (zp),Y - 6510 (illegal)
        opcodeTable[0x74] = () => ReadZeroPageX(() => fetchOpcodeAction);                  // *NOP zp,X - 6510 (illegal)
        opcodeTable[0x75] = () => ReadZeroPageX(() => addWithCarryAction);                 // ADC zp,X - 6502
        opcodeTable[0x76] = () => ModifyZeroPageX(RotateRight);                            // ROR zp,X - 6502
        opcodeTable[0x77] = () => ModifyZeroPageX(RRA_Operation);                          // *RRA zp,X - 6510 (illegal)
        opcodeTable[0x78] = () => DummyRead(() => SetInterrupt);                           // SEI - 6502
        opcodeTable[0x79] = () => ReadAbsoluteY(() => addWithCarryAction);                 // ADC abs,Y - 6502
        opcodeTable[0x7A] = () => DummyRead(() => fetchOpcodeAction);                      // *NOP - 6510 (illegal)
        opcodeTable[0x7B] = () => ModifyAbsoluteY(RRA_Operation);                          // *RRA abs,Y - 6510 (illegal)
        opcodeTable[0x7C] = () => ReadAbsoluteX(() => fetchOpcodeAction);                  // *NOP abs,X - 6510 (illegal)
        opcodeTable[0x7D] = () => ReadAbsoluteX(() => addWithCarryAction);                 // ADC abs,X - 6502
        opcodeTable[0x7E] = () => ModifyAbsoluteX(RotateRight);                            // ROR abs,X - 6502
        opcodeTable[0x7F] = () => ModifyAbsoluteX(RRA_Operation);                          // *RRA abs,X - 6510 (illegal)

        opcodeTable[0x80] = () => ReadImmediate(() => fetchOpcodeAction);                  // *NOP # - 6510 (illegal)
        opcodeTable[0x81] = () => WriteZeroPageIndexedIndirect(() => A);                   // STA (zp,X) - 6502
        opcodeTable[0x82] = () => ReadImmediate(() => fetchOpcodeAction);                  // *NOP # - 6510 (illegal)
        opcodeTable[0x83] = () => WriteZeroPageIndexedIndirect(() => (byte)(A & X));       // *SAX (zp,X) - 6510 (illegal)
        opcodeTable[0x84] = () => WriteZeroPage(() => Y);                                  // STY zp - 6502
        opcodeTable[0x85] = () => WriteZeroPage(() => A);                                  // STA zp - 6502
        opcodeTable[0x86] = () => WriteZeroPage(() => X);                                  // STX zp - 6502
        opcodeTable[0x87] = () => WriteZeroPage(() => (byte)(A & X));                      // *SAX zp - 6510 (illegal)
        opcodeTable[0x88] = () => DummyRead(() => DecrementY);                             // DEY - 6502
        opcodeTable[0x89] = () => ReadImmediate(() => fetchOpcodeAction);                  // *NOP # - 6510 (illegal)
        opcodeTable[0x8A] = () => DummyRead(() => transferXAAction);                       // TXA - 6502
        opcodeTable[0x8B] = () => ReadImmediate(() => XAA_Operation);                      // *XAA # - 6510 (illegal)
        opcodeTable[0x8C] = () => WriteAbsolute(() => Y);                                  // STY abs - 6502
        opcodeTable[0x8D] = () => WriteAbsolute(() => A);                                  // STA abs - 6502
        opcodeTable[0x8E] = () => WriteAbsolute(() => X);                                  // STX abs - 6502
        opcodeTable[0x8F] = () => WriteAbsolute(() => (byte)(A & X));                      // *SAX abs - 6510 (illegal)

        opcodeTable[0x90] = () => BranchIf(() => (P & C) == 0);                            // BCC - 6502
        opcodeTable[0x91] = () => WriteZeroPageIndirectIndexed(() => A);                   // STA (zp),Y - 6502
        opcodeTable[0x92] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0x93] = () => WriteZeroPageIndirectIndexed(() => (byte)(A & X & (hi + 1))); // *AHX (zp),Y - 6510 (illegal, unstable)
        opcodeTable[0x94] = () => WriteZeroPageX(() => Y);                                 // STY zp,X - 6502
        opcodeTable[0x95] = () => WriteZeroPageX(() => A);                                 // STA zp,X - 6502
        opcodeTable[0x96] = () => WriteZeroPageY(() => X);                                 // STX zp,Y - 6502
        opcodeTable[0x97] = () => WriteZeroPageY(() => (byte)(A & X));                     // *SAX zp,Y - 6510 (illegal)
        opcodeTable[0x98] = () => DummyRead(() => TransferYA);                             // TYA - 6502
        opcodeTable[0x99] = () => WriteAbsoluteY(() => A);                                 // STA abs,Y - 6502
        opcodeTable[0x9A] = () => DummyRead(() => TransferXS);                             // TXS - 6502
        opcodeTable[0x9B] = () => WriteAbsoluteY(() => (byte)(A & X & S));                 // *TAS abs,Y - 6510 (illegal, unstable)
        opcodeTable[0x9C] = () => WriteAbsoluteX(() => (byte)(Y & (hi + 1)));              // *SHY abs,X - 6510 (illegal, unstable)
        opcodeTable[0x9D] = () => WriteAbsoluteX(() => A);                                 // STA abs,X - 6502
        opcodeTable[0x9E] = () => WriteAbsoluteY(() => (byte)(X & (hi + 1)));              // *SHX abs,Y - 6510 (illegal, unstable)
        opcodeTable[0x9F] = () => WriteAbsoluteY(() => (byte)(A & X & (hi + 1)));          // *AHX abs,Y - 6510 (illegal, unstable)

        opcodeTable[0xA0] = () => ReadImmediate(() => loadYAction);                        // LDY # - 6502
        opcodeTable[0xA1] = () => ReadZeroPageIndexedIndirect(() => loadAAction);          // LDA (zp,X) - 6502
        opcodeTable[0xA2] = () => ReadImmediate(() => loadXAction);                        // LDX # - 6502
        opcodeTable[0xA3] = () => ReadZeroPageIndexedIndirect(() => LAX_Operation);        // *LAX (zp,X) - 6510 (illegal)
        opcodeTable[0xA4] = () => ReadZeroPage(() => loadYAction);                         // LDY zp - 6502
        opcodeTable[0xA5] = () => ReadZeroPage(() => loadAAction);                         // LDA zp - 6502
        opcodeTable[0xA6] = () => ReadZeroPage(() => loadXAction);                         // LDX zp - 6502
        opcodeTable[0xA7] = () => ReadZeroPage(() => LAX_Operation);                       // *LAX zp - 6510 (illegal)
        opcodeTable[0xA8] = () => DummyRead(() => transferAYAction);                       // TAY - 6502
        opcodeTable[0xA9] = () => ReadImmediate(() => loadAAction);                        // LDA # - 6502
        opcodeTable[0xAA] = () => DummyRead(() => transferAXAction);                       // TAX - 6502
        opcodeTable[0xAB] = () => ReadImmediate(() => LAX_Operation);                      // *LAX # - 6510 (illegal)
        opcodeTable[0xAC] = () => ReadAbsolute(() => loadYAction);                         // LDY abs - 6502
        opcodeTable[0xAD] = () => ReadAbsolute(() => loadAAction);                         // LDA abs - 6502
        opcodeTable[0xAE] = () => ReadAbsolute(() => loadXAction);                         // LDX abs - 6502
        opcodeTable[0xAF] = () => ReadAbsolute(() => LAX_Operation);                       // *LAX abs - 6510 (illegal)

        opcodeTable[0xB0] = () => BranchIf(() => (P & C) != 0);                            // BCS - 6502
        opcodeTable[0xB1] = () => ReadZeroPageIndirectIndexed(() => loadAAction);          // LDA (zp),Y - 6502
        opcodeTable[0xB2] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0xB3] = () => ReadZeroPageIndirectIndexed(() => LAX_Operation);        // *LAX (zp),Y - 6510 (illegal)
        opcodeTable[0xB4] = () => ReadZeroPageX(() => loadYAction);                        // LDY zp,X - 6502
        opcodeTable[0xB5] = () => ReadZeroPageX(() => loadAAction);                        // LDA zp,X - 6502
        opcodeTable[0xB6] = () => ReadZeroPageY(() => loadXAction);                        // LDX zp,Y - 6502
        opcodeTable[0xB7] = () => ReadZeroPageY(() => LAX_Operation);                      // *LAX zp,Y - 6510 (illegal)
        opcodeTable[0xB8] = () => DummyRead(() => ClearOverflow);                          // CLV - 6502
        opcodeTable[0xB9] = () => ReadAbsoluteY(() => loadAAction);                        // LDA abs,Y - 6502
        opcodeTable[0xBA] = () => DummyRead(() => TransferSX);                             // TSX - 6502
        opcodeTable[0xBB] = () => ReadAbsoluteY(() => LAS_Operation);                      // *LAS abs,Y - 6510 (illegal)
        opcodeTable[0xBC] = () => ReadAbsoluteX(() => loadYAction);                        // LDY abs,X - 6502
        opcodeTable[0xBD] = () => ReadAbsoluteX(() => loadAAction);                        // LDA abs,X - 6502
        opcodeTable[0xBE] = () => ReadAbsoluteY(() => loadXAction);                        // LDX abs,Y - 6502
        opcodeTable[0xBF] = () => ReadAbsoluteY(() => LAX_Operation);                      // *LAX abs,Y - 6510 (illegal)

        opcodeTable[0xC0] = () => ReadImmediate(() => CompareY);                           // CPY # - 6502
        opcodeTable[0xC1] = () => ReadZeroPageIndexedIndirect(() => CompareA);             // CMP (zp,X) - 6502
        opcodeTable[0xC2] = () => ReadImmediate(() => fetchOpcodeAction);                  // *NOP # - 6510 (illegal)
        opcodeTable[0xC3] = () => ModifyZeroPageIndexedIndirect(DCP_Operation);            // *DCP (zp,X) - 6510 (illegal)
        opcodeTable[0xC4] = () => ReadZeroPage(() => CompareY);                            // CPY zp - 6502
        opcodeTable[0xC5] = () => ReadZeroPage(() => CompareA);                            // CMP zp - 6502
        opcodeTable[0xC6] = () => ModifyZeroPage(DecrementMemory);                         // DEC zp - 6502
        opcodeTable[0xC7] = () => ModifyZeroPage(DCP_Operation);                           // *DCP zp - 6510 (illegal)
        opcodeTable[0xC8] = () => DummyRead(() => IncrementY);                             // INY - 6502
        opcodeTable[0xC9] = () => ReadImmediate(() => CompareA);                           // CMP # - 6502
        opcodeTable[0xCA] = () => DummyRead(() => DecrementX);                             // DEX - 6502
        opcodeTable[0xCB] = () => ReadImmediate(() => AXS_Operation);                      // *AXS # - 6510 (illegal)
        opcodeTable[0xCC] = () => ReadAbsolute(() => CompareY);                            // CPY abs - 6502
        opcodeTable[0xCD] = () => ReadAbsolute(() => CompareA);                            // CMP abs - 6502
        opcodeTable[0xCE] = () => ModifyAbsolute(DecrementMemory);                         // DEC abs - 6502
        opcodeTable[0xCF] = () => ModifyAbsolute(DCP_Operation);                           // *DCP abs - 6510 (illegal)

        opcodeTable[0xD0] = () => BranchIf(() => (P & Z) == 0);                            // BNE - 6502
        opcodeTable[0xD1] = () => ReadZeroPageIndirectIndexed(() => CompareA);             // CMP (zp),Y - 6502
        opcodeTable[0xD2] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0xD3] = () => ModifyZeroPageIndirectIndexed(DCP_Operation);            // *DCP (zp),Y - 6510 (illegal)
        opcodeTable[0xD4] = () => ReadZeroPageX(() => fetchOpcodeAction);                  // *NOP zp,X - 6510 (illegal)
        opcodeTable[0xD5] = () => ReadZeroPageX(() => CompareA);                           // CMP zp,X - 6502
        opcodeTable[0xD6] = () => ModifyZeroPageX(DecrementMemory);                        // DEC zp,X - 6502
        opcodeTable[0xD7] = () => ModifyZeroPageX(DCP_Operation);                          // *DCP zp,X - 6510 (illegal)
        opcodeTable[0xD8] = () => DummyRead(() => ClearDecimal);                           // CLD - 6502
        opcodeTable[0xD9] = () => ReadAbsoluteY(() => CompareA);                           // CMP abs,Y - 6502
        opcodeTable[0xDA] = () => DummyRead(() => fetchOpcodeAction);                      // *NOP - 6510 (illegal)
        opcodeTable[0xDB] = () => ModifyAbsoluteY(DCP_Operation);                          // *DCP abs,Y - 6510 (illegal)
        opcodeTable[0xDC] = () => ReadAbsoluteX(() => fetchOpcodeAction);                  // *NOP abs,X - 6510 (illegal)
        opcodeTable[0xDD] = () => ReadAbsoluteX(() => CompareA);                           // CMP abs,X - 6502
        opcodeTable[0xDE] = () => ModifyAbsoluteX(DecrementMemory);                        // DEC abs,X - 6502
        opcodeTable[0xDF] = () => ModifyAbsoluteX(DCP_Operation);                          // *DCP abs,X - 6510 (illegal)

        opcodeTable[0xE0] = () => ReadImmediate(() => CompareX);                           // CPX # - 6502
        opcodeTable[0xE1] = () => ReadZeroPageIndexedIndirect(() => SubtractWithCarry);    // SBC (zp,X) - 6502
        opcodeTable[0xE2] = () => ReadImmediate(() => fetchOpcodeAction);                  // *NOP # - 6510 (illegal)
        opcodeTable[0xE3] = () => ModifyZeroPageIndexedIndirect(ISC_Operation);            // *ISC (zp,X) - 6510 (illegal)
        opcodeTable[0xE4] = () => ReadZeroPage(() => CompareX);                            // CPX zp - 6502
        opcodeTable[0xE5] = () => ReadZeroPage(() => SubtractWithCarry);                   // SBC zp - 6502
        opcodeTable[0xE6] = () => ModifyZeroPage(IncrementMemory);                         // INC zp - 6502
        opcodeTable[0xE7] = () => ModifyZeroPage(ISC_Operation);                           // *ISC zp - 6510 (illegal)
        opcodeTable[0xE8] = () => DummyRead(() => IncrementX);                             // INX - 6502
        opcodeTable[0xE9] = () => ReadImmediate(() => SubtractWithCarry);                  // SBC # - 6502
        opcodeTable[0xEA] = () => DummyRead(() => fetchOpcodeAction);                      // NOP - 6502
        opcodeTable[0xEB] = () => ReadImmediate(() => SubtractWithCarry);                  // *SBC # - 6510 (illegal, same as official)
        opcodeTable[0xEC] = () => ReadAbsolute(() => CompareX);                            // CPX abs - 6502
        opcodeTable[0xED] = () => ReadAbsolute(() => SubtractWithCarry);                   // SBC abs - 6502
        opcodeTable[0xEE] = () => ModifyAbsolute(IncrementMemory);                         // INC abs - 6502
        opcodeTable[0xEF] = () => ModifyAbsolute(ISC_Operation);                           // *ISC abs - 6510 (illegal)

        opcodeTable[0xF0] = () => BranchIf(() => (P & Z) != 0);                            // BEQ - 6502
        opcodeTable[0xF1] = () => ReadZeroPageIndirectIndexed(() => SubtractWithCarry);    // SBC (zp),Y - 6502
        opcodeTable[0xF2] = () => DummyRead(() => fetchOpcodeAction);                      // *JAM - 6510 (illegal)
        opcodeTable[0xF3] = () => ModifyZeroPageIndirectIndexed(ISC_Operation);            // *ISC (zp),Y - 6510 (illegal)
        opcodeTable[0xF4] = () => ReadZeroPageX(() => fetchOpcodeAction);                  // *NOP zp,X - 6510 (illegal)
        opcodeTable[0xF5] = () => ReadZeroPageX(() => SubtractWithCarry);                  // SBC zp,X - 6502
        opcodeTable[0xF6] = () => ModifyZeroPageX(IncrementMemory);                        // INC zp,X - 6502
        opcodeTable[0xF7] = () => ModifyZeroPageX(ISC_Operation);                          // *ISC zp,X - 6510 (illegal)
        opcodeTable[0xF8] = () => DummyRead(() => SetDecimal);                             // SED - 6502
        opcodeTable[0xF9] = () => ReadAbsoluteY(() => SubtractWithCarry);                  // SBC abs,Y - 6502
        opcodeTable[0xFA] = () => DummyRead(() => fetchOpcodeAction);                      // *NOP - 6510 (illegal)
        opcodeTable[0xFB] = () => ModifyAbsoluteY(ISC_Operation);                          // *ISC abs,Y - 6510 (illegal)
        opcodeTable[0xFC] = () => ReadAbsoluteX(() => fetchOpcodeAction);                  // *NOP abs,X - 6510 (illegal)
        opcodeTable[0xFD] = () => ReadAbsoluteX(() => SubtractWithCarry);                  // SBC abs,X - 6502
        opcodeTable[0xFE] = () => ModifyAbsoluteX(IncrementMemory);                        // INC abs,X - 6502
        opcodeTable[0xFF] = () => ModifyAbsoluteX(ISC_Operation);                          // *ISC abs,X - 6510 (illegal)
    }
}
