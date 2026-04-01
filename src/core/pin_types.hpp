/*
 * pin_types.hpp — Pin type definitions and enumerations
 *
 * Core pin type definitions used across all IC packages and chip types.
 * Both PinType and PinLabel enums are generated from the PIN_LABELS()
 * X-macro, keeping label strings, category membership, and documentation
 * in a single authoritative source.
 */

#pragma once

#include <cstdint>
#include <string>
#include "core/system_lines.hpp"

// ============================================================================
// PIN_LABELS — master X-macro
// ============================================================================
//
// Visitors:
//   INV(id, str, comment)  — active-low pin label (before ACTIVE_LOW_END)
//   CAT(pin_type)          — start of a PinType category (active-high)
//   PIN(id, str, comment)  — active-high pin label
//
// Rules:
//   • All INV entries come first (no interleaved CAT).
//   • CAT entries mark PinType boundaries; every PIN after a CAT belongs
//     to that category until the next CAT.
//   • Sequential numbered groups (A0-A23, D0-D15, etc.) MUST remain
//     contiguous — get_bit_index_from_label() depends on arithmetic.
//   • Every INV(_FOO) must have a corresponding PIN(FOO) somewhere so
//     that pin_canonical(_FOO) → FOO resolves to a valid label.
//

#define PIN_LABELS(PLBL_INV, PLBL_CAT, PLBL_PIN) \
    /* ════════════════════════════════════════════════════════════════ */ \
    /* Active-low (inverted) pins — sorted alphabetically.            */ \
    /* String representation omits the underscore prefix; the display */ \
    /* layer prepends "/" or overbar based on notation style.          */ \
    /* ════════════════════════════════════════════════════════════════ */ \
    PLBL_INV(_ABORT,      "ABORT",      "abort (65C816)")                       \
    PLBL_INV(_AEC,        "AEC",        "address enable control")               \
    PLBL_INV(_AS_M68K,    "AS",         "address strobe (M68K)")                \
    PLBL_INV(_BASIC,      "BASIC",      "BASIC ROM select")                     \
    PLBL_INV(_BERR,       "BERR",       "bus error (M68K)")                     \
    PLBL_INV(_BG,         "BG",         "bus grant (M68K)")                     \
    PLBL_INV(_BGACK,      "BGACK",      "bus grant acknowledge (M68K)")         \
    PLBL_INV(_BR,         "BR",         "bus request (M68K)")                   \
    PLBL_INV(_BUSAK,      "BUSAK",      "bus acknowledge (Z80)")                \
    PLBL_INV(_BUSRQ,      "BUSRQ",      "bus request (Z80)")                    \
    PLBL_INV(_CAS,        "CAS",        "column address strobe")                \
    PLBL_INV(_CASRAM_PLA, "CASRAM",     "CAS RAM (PLA output F0)")              \
    PLBL_INV(_CE,         "CE",         "chip enable")                          \
    PLBL_INV(_CHAREN,     "CHAREN",     "character ROM enable")                 \
    PLBL_INV(_CHAROM,     "CHAROM",     "character ROM select")                 \
    PLBL_INV(_CLR,        "CLR",        "master clear (74LS259)")               \
    PLBL_INV(_CS,         "CS",         "chip select")                          \
    PLBL_INV(_CS0,        "CS0",        "chip select 0")                        \
    PLBL_INV(_CS1,        "CS1",        "chip select 1")                        \
    PLBL_INV(_CS2,        "CS2",        "chip select 2")                        \
    PLBL_INV(_CSR,        "CSR",        "chip select read (TMS9918)")           \
    PLBL_INV(_CSW,        "CSW",        "chip select write (TMS9918)")          \
    PLBL_INV(_DTACK,      "DTACK",      "data transfer acknowledge (M68K)")     \
    PLBL_INV(_E1,         "E1",         "enable 1 (74LS138)")                   \
    PLBL_INV(_E2,         "E2",         "enable 2 (74LS138)")                   \
    PLBL_INV(_EXROM,      "EXROM",      "external ROM")                         \
    PLBL_INV(_FIRQ,       "FIRQ",       "fast interrupt request (MC6809)")      \
    PLBL_INV(_G,          "G",          "gate/enable (74LS249)")                \
    PLBL_INV(_GAME,       "GAME",       "game line")                            \
    PLBL_INV(_HALT,       "HALT",       "halt (Z80)")                           \
    PLBL_INV(_HIRAM,      "HIRAM",      "high RAM")                             \
    PLBL_INV(_IC,         "IC",         "initial clear (Yamaha FM)")            \
    PLBL_INV(_INT,        "INT",        "interrupt (Z80)")                      \
    PLBL_INV(_INTR,       "INTR",       "interrupt (MOS 8563 VDC)")             \
    PLBL_INV(_IO,         "I/O",        "I/O area select")                      \
    PLBL_INV(_IORQ,       "IORQ",       "I/O request (Z80)")                    \
    PLBL_INV(_IPL0,       "IPL0",       "interrupt priority level 0 (M68K)")    \
    PLBL_INV(_IPL1,       "IPL1",       "interrupt priority level 1 (M68K)")    \
    PLBL_INV(_IPL2,       "IPL2",       "interrupt priority level 2 (M68K)")    \
    PLBL_INV(_IRQ,        "IRQ",        "interrupt request")                    \
    PLBL_INV(_IRQA,       "IRQA",       "interrupt request A (PIA)")            \
    PLBL_INV(_IRQB,       "IRQB",       "interrupt request B (PIA)")            \
    PLBL_INV(_KERNAL,     "KERNAL",     "KERNAL ROM select")                    \
    PLBL_INV(_LDS,        "LDS",        "lower data strobe (M68K)")             \
    PLBL_INV(_LORAM,      "LORAM",      "low RAM")                              \
    PLBL_INV(_M1,         "M1",         "machine cycle 1 (Z80)")                \
    PLBL_INV(_ML,         "ML",         "memory lock (65C02/65C816)")           \
    PLBL_INV(_MREQ,       "MREQ",       "memory request (Z80)")                 \
    PLBL_INV(_NMI,        "NMI",        "non-maskable interrupt")               \
    PLBL_INV(_OE,         "OE",         "output enable")                        \
    PLBL_INV(_PARD,       "PARD",       "peripheral address read (5A22)")       \
    PLBL_INV(_PAWR,       "PAWR",       "peripheral address write (5A22)")      \
    PLBL_INV(_Q7,         "Q7",         "complement output (shift register)")   \
    PLBL_INV(_RAS,        "RAS",        "row address strobe")                   \
    PLBL_INV(_RD,         "RD",         "read strobe")                          \
    PLBL_INV(_RES,        "RES",        "reset")                                \
    PLBL_INV(_RFSH,       "RFSH",       "refresh (Z80)")                        \
    PLBL_INV(_ROMH,       "ROMH",       "ROM high")                             \
    PLBL_INV(_ROML,       "ROML",       "ROM low")                              \
    PLBL_INV(_ROMSEL,     "ROMSEL",     "ROM select (5A22 cartridge)")          \
    PLBL_INV(_SO,         "SO",         "set overflow")                         \
    PLBL_INV(_TEST,       "TEST",       "test pin")                             \
    PLBL_INV(_UDS,        "UDS",        "upper data strobe (M68K)")             \
    PLBL_INV(_VA14,       "VA14",       "video address 14")                     \
    PLBL_INV(_VMA,        "VMA",        "valid memory address (M68K)")          \
    PLBL_INV(_VP,         "VP",         "vector pull")                          \
    PLBL_INV(_VPA_M68K,   "VPA",        "valid peripheral address (M68K)")      \
    PLBL_INV(_VPB,        "VPB",        "vector pull bar")                      \
    PLBL_INV(_WAIT,       "WAIT",       "wait (Z80)")                           \
    PLBL_INV(_WE,         "WE",         "write enable")                         \
    PLBL_INV(_WR,         "WR",         "write strobe (5A22)")                  \
    PLBL_INV(_WRAM,       "WRAM",       "work RAM chip select (5A22)")          \
    PLBL_INV(_Y0,         "Y0",         "decoder output 0 (74LS138)")           \
    PLBL_INV(_Y1,         "Y1",         "decoder output 1 (74LS138)")           \
    PLBL_INV(_Y2,         "Y2",         "decoder output 2 (74LS138)")           \
    PLBL_INV(_Y3,         "Y3",         "decoder output 3 (74LS138)")           \
    PLBL_INV(_Y4,         "Y4",         "decoder output 4 (74LS138)")           \
    PLBL_INV(_Y5,         "Y5",         "decoder output 5 (74LS138)")           \
    PLBL_INV(_Y6,         "Y6",         "decoder output 6 (74LS138)")           \
    PLBL_INV(_Y7,         "Y7",         "decoder output 7 (74LS138)")           \
    \
    /* ════════════════════════════════════════════════════════════════ */ \
    /* Active-high pins — grouped by PinType category.                */ \
    /* ════════════════════════════════════════════════════════════════ */ \
    \
    /* ── Power ───────────────────────────────────────────────────── */ \
    PLBL_CAT(POWER)                                                             \
    PLBL_PIN(AGND,        "AGND",       "analog ground")                        \
    PLBL_PIN(AVCC,        "AVCC",       "analog supply voltage")                \
    PLBL_PIN(GND,         "GND",        "ground")                               \
    PLBL_PIN(VCC,         "VCC",        "+5V power")                            \
    PLBL_PIN(VCC_DRAM,    "VCC DRAM",   "DRAM +5V supply (MOS 8563 VDC)")       \
    PLBL_PIN(VDD,         "VDD",        "+5V power supply")                     \
    PLBL_PIN(VSS,         "VSS",        "ground (0V)")                          \
    PLBL_PIN(VSS_DRAM,    "VSS DRAM",   "DRAM ground (MOS 8563 VDC)")           \
    PLBL_PIN(VTIA,        "Vtia",       "TIA analog supply voltage")            \
    \
    /* ── Clock / oscillator ──────────────────────────────────────── */ \
    PLBL_PIN(CK,          "CK",         "clock (EAROM/general)")                \
    PLBL_CAT(CLOCK)                                                             \
    PLBL_PIN(CLK,         "CLK",        "generic clock input")                  \
    PLBL_PIN(COLOR_CLK,   "COLOR CLK",  "color clock")                          \
    PLBL_PIN(CPUCLK,      "CPUCLK",     "CPU clock output (5A22)")              \
    PLBL_PIN(DOT_CLK,     "DOT CLK",    "dot clock")                            \
    PLBL_PIN(ENABLE,      "E",          "enable clock input (6800 bus)")        \
    PLBL_PIN(EXTAL,       "EXTAL",      "external crystal input (MC6809)")      \
    PLBL_PIN(E_CLK,       "E",          "enable clock output (M68K 6800)")      \
    PLBL_PIN(GROMCLK,     "GROMCLK",    "GROM clock output (TMS9918)")          \
    PLBL_PIN(M2,          "M2",         "derived clock output (2A03)")          \
    PLBL_PIN(OSC_IN,      "OSC IN",     "oscillator input")                     \
    PLBL_PIN(OSC_OUT,     "OSC OUT",    "oscillator output")                    \
    PLBL_PIN(PHI0,        "Φ0",         "clock input")                          \
    PLBL_PIN(PHI1,        "Φ1",         "inverted clock output")                \
    PLBL_PIN(PHI2,        "Φ2",         "primary clock output")                 \
    PLBL_PIN(PHI_M,       "ΦM",         "master clock input (Yamaha FM)")       \
    PLBL_PIN(PHI_S,       "ΦS",         "SSG/secondary clock (Yamaha OPN)")     \
    PLBL_PIN(Q_CLK,       "Q",          "Q clock output (MC6809)")              \
    PLBL_PIN(SYSCLK,      "SYSCLK",     "system master clock (5A22)")           \
    PLBL_PIN(XTAL,        "XTAL",       "crystal (MC6809)")                     \
    PLBL_PIN(XTAL1,       "XTAL1",      "crystal 1")                            \
    PLBL_PIN(XTAL2,       "XTAL2",      "crystal 2")                            \
    \
    /* ── CPU address bus (A0-A23 must remain sequential) ─────────── */ \
    PLBL_CAT(ADDRESS)                                                           \
    PLBL_PIN(A0,          "A0",         "address bit 0")                        \
    PLBL_PIN(A1,          "A1",         "address bit 1")                        \
    PLBL_PIN(A2,          "A2",         "address bit 2")                        \
    PLBL_PIN(A3,          "A3",         "address bit 3")                        \
    PLBL_PIN(A4,          "A4",         "address bit 4")                        \
    PLBL_PIN(A5,          "A5",         "address bit 5")                        \
    PLBL_PIN(A6,          "A6",         "address bit 6")                        \
    PLBL_PIN(A7,          "A7",         "address bit 7")                        \
    PLBL_PIN(A8,          "A8",         "address bit 8")                        \
    PLBL_PIN(A9,          "A9",         "address bit 9")                        \
    PLBL_PIN(A10,         "A10",        "address bit 10")                       \
    PLBL_PIN(A11,         "A11",        "address bit 11")                       \
    PLBL_PIN(A12,         "A12",        "address bit 12")                       \
    PLBL_PIN(A13,         "A13",        "address bit 13")                       \
    PLBL_PIN(A14,         "A14",        "address bit 14")                       \
    PLBL_PIN(A15,         "A15",        "address bit 15")                       \
    PLBL_PIN(A16,         "A16",        "address bit 16")                       \
    PLBL_PIN(A17,         "A17",        "address bit 17")                       \
    PLBL_PIN(A18,         "A18",        "address bit 18")                       \
    PLBL_PIN(A19,         "A19",        "address bit 19")                       \
    PLBL_PIN(A20,         "A20",        "address bit 20")                       \
    PLBL_PIN(A21,         "A21",        "address bit 21")                       \
    PLBL_PIN(A22,         "A22",        "address bit 22")                       \
    PLBL_PIN(A23,         "A23",        "address bit 23")                       \
    \
    /* ── CPU data bus (D0-D15 must remain sequential) ────────────── */ \
    PLBL_CAT(DATA)                                                              \
    PLBL_PIN(D0,          "D0",         "data bit 0")                           \
    PLBL_PIN(D1,          "D1",         "data bit 1")                           \
    PLBL_PIN(D2,          "D2",         "data bit 2")                           \
    PLBL_PIN(D3,          "D3",         "data bit 3")                           \
    PLBL_PIN(D4,          "D4",         "data bit 4")                           \
    PLBL_PIN(D5,          "D5",         "data bit 5")                           \
    PLBL_PIN(D6,          "D6",         "data bit 6")                           \
    PLBL_PIN(D7,          "D7",         "data bit 7")                           \
    PLBL_PIN(D8,          "D8",         "data bit 8")                           \
    PLBL_PIN(D9,          "D9",         "data bit 9")                           \
    PLBL_PIN(D10,         "D10",        "data bit 10")                          \
    PLBL_PIN(D11,         "D11",        "data bit 11")                          \
    PLBL_PIN(D12,         "D12",        "data bit 12")                          \
    PLBL_PIN(D13,         "D13",        "data bit 13")                          \
    PLBL_PIN(D14,         "D14",        "data bit 14")                          \
    PLBL_PIN(D15,         "D15",        "data bit 15")                          \
    /* ── Secondary data buses (DB=CPU, DD=DRAM — VDC, sequential) ── */ \
    PLBL_PIN(DB0,         "DB0",        "CPU data 0 (MOS 8563 VDC)")            \
    PLBL_PIN(DB1,         "DB1",        "CPU data 1 (MOS 8563 VDC)")            \
    PLBL_PIN(DB2,         "DB2",        "CPU data 2 (MOS 8563 VDC)")            \
    PLBL_PIN(DB3,         "DB3",        "CPU data 3 (MOS 8563 VDC)")            \
    PLBL_PIN(DB4,         "DB4",        "CPU data 4 (MOS 8563 VDC)")            \
    PLBL_PIN(DB5,         "DB5",        "CPU data 5 (MOS 8563 VDC)")            \
    PLBL_PIN(DB6,         "DB6",        "CPU data 6 (MOS 8563 VDC)")            \
    PLBL_PIN(DB7,         "DB7",        "CPU data 7 (MOS 8563 VDC)")            \
    PLBL_PIN(DD0,         "DD0",        "DRAM data 0 (MOS 8563 VDC)")           \
    PLBL_PIN(DD1,         "DD1",        "DRAM data 1 (MOS 8563 VDC)")           \
    PLBL_PIN(DD2,         "DD2",        "DRAM data 2 (MOS 8563 VDC)")           \
    PLBL_PIN(DD3,         "DD3",        "DRAM data 3 (MOS 8563 VDC)")           \
    PLBL_PIN(DD4,         "DD4",        "DRAM data 4 (MOS 8563 VDC)")           \
    PLBL_PIN(DD5,         "DD5",        "DRAM data 5 (MOS 8563 VDC)")           \
    PLBL_PIN(DD6,         "DD6",        "DRAM data 6 (MOS 8563 VDC)")           \
    PLBL_PIN(DD7,         "DD7",        "DRAM data 7 (MOS 8563 VDC)")           \
    \
    /* ── Control signals ─────────────────────────────────────────── */ \
    PLBL_CAT(CONTROL)                                                           \
    PLBL_PIN(AEC,         "AEC",        "address enable control (6510)")        \
    PLBL_PIN(ALE,         "ALE",        "address latch enable")                 \
    PLBL_PIN(AS_M68K,     "AS",         "address strobe (M68K, active-high)")   \
    PLBL_PIN(AVMA,        "AVMA",       "advanced valid memory address (6809)") \
    PLBL_PIN(BA,          "BA",         "bus available")                        \
    PLBL_PIN(BE,          "BE",         "bus enable (65C02/65C816)")            \
    PLBL_PIN(BERR,        "BERR",       "bus error (M68K, active-high)")        \
    PLBL_PIN(BG,          "BG",         "bus grant (M68K, active-high)")        \
    PLBL_PIN(BGACK,       "BGACK",      "bus grant ack (M68K, active-high)")    \
    PLBL_PIN(BR,          "BR",         "bus request (M68K, active-high)")      \
    PLBL_PIN(BS,          "BS",         "bus status (MC6809)")                  \
    PLBL_PIN(BUSAK,       "BUSAK",      "bus acknowledge (Z80, active-high)")   \
    PLBL_PIN(BUSRQ,       "BUSRQ",      "bus request (Z80, active-high)")       \
    PLBL_PIN(BUSY,        "BUSY",       "busy (MC6809, 16-bit operations)")     \
    PLBL_PIN(C1,          "C1",         "control pin 1")                        \
    PLBL_PIN(C2,          "C2",         "control pin 2")                        \
    PLBL_PIN(CAS,         "CAS",        "column address strobe (active-high)")  \
    PLBL_PIN(CASRAM,      "CASRAM",     "CAS for RAM")                          \
    PLBL_PIN(CE,          "CE",         "chip enable (active-high)")            \
    PLBL_PIN(CLR,         "CLR",        "master clear (active-high)")           \
    PLBL_PIN(CS,          "CS",         "chip select (active-high)")            \
    PLBL_PIN(CS0,         "CS0",        "chip select 0 (active-high)")          \
    PLBL_PIN(CS1,         "CS1",        "chip select 1 (active-high)")          \
    PLBL_PIN(CS2,         "CS2",        "chip select 2 (active-high)")          \
    PLBL_PIN(CS3,         "CS3",        "chip select 3 (TIA)")                  \
    PLBL_PIN(CSR,         "CSR",        "chip select read (TMS9918, act-h)")    \
    PLBL_PIN(CSW,         "CSW",        "chip select write (TMS9918, act-h)")   \
    PLBL_PIN(DTACK,       "DTACK",      "data transfer ack (M68K, active-h)")   \
    PLBL_PIN(HALT,        "HALT",       "halt (Z80, active-high)")              \
    PLBL_PIN(INT,         "INT",        "interrupt (Z80, active-high)")         \
    PLBL_PIN(IORQ,        "IORQ",       "I/O request (Z80, active-high)")       \
    PLBL_PIN(LDS,         "LDS",        "lower data strobe (M68K, active-h)")   \
    PLBL_PIN(M1,          "M1",         "machine cycle 1 (Z80, active-high)")   \
    PLBL_PIN(MDEN,        "MDEN",       "memory data enable (Yamaha OPNA)")     \
    PLBL_PIN(MREQ,        "MREQ",       "memory request (Z80, active-high)")    \
    PLBL_PIN(MODE,        "MODE",       "mode select (TMS9918)")                \
    PLBL_PIN(MUX,         "MUX",        "address multiplexer")                  \
    PLBL_PIN(OE,          "OE",         "output enable (active-high)")          \
    PLBL_PIN(PARD,        "PARD",       "peripheral addr read (5A22, act-h)")   \
    PLBL_PIN(PAWR,        "PAWR",       "peripheral addr write (5A22, act-h)")  \
    PLBL_PIN(P_S,         "P/S",        "parallel/serial (shift register)")     \
    PLBL_PIN(RAS,         "RAS",        "row address strobe (active-high)")     \
    PLBL_PIN(RD,          "RD",         "read strobe (active-high)")            \
    PLBL_PIN(RDY,         "RDY",        "ready input/output")                   \
    PLBL_PIN(RFSH,        "RFSH",       "refresh (Z80, active-high)")           \
    PLBL_PIN(ROMCS,       "ROMCS",      "ROM chip select (Yamaha OPNA)")        \
    PLBL_PIN(ROMSEL,      "ROMSEL",     "ROM select (5A22 cart, active-high)")  \
    PLBL_PIN(RS,          "RS",         "register select")                      \
    PLBL_PIN(RS0,         "RS0",        "register select 0 (PIA)")              \
    PLBL_PIN(RS1,         "RS1",        "register select 1 (PIA)")              \
    PLBL_PIN(RW,          "R/W",        "read/write control")                   \
    PLBL_PIN(SPOFF,       "SPOFF",      "speaker off (Yamaha OPNA)")            \
    PLBL_PIN(SYNC,        "SYNC",       "synchronization output")               \
    PLBL_PIN(UDS,         "UDS",        "upper data strobe (M68K, active-h)")   \
    PLBL_PIN(VMA,         "VMA",        "valid mem address (M68K, active-h)")   \
    PLBL_PIN(WAIT,        "WAIT",       "wait (Z80, active-high)")              \
    PLBL_PIN(WE,          "WE",         "write enable (active-high)")           \
    PLBL_PIN(WR,          "WR",         "write strobe (5A22, active-high)")     \
    PLBL_PIN(WRAM,        "WRAM",       "work RAM chip sel (5A22, active-h)")   \
    \
    /* ── Bus control (Z80 PIO/CTC, AY-3-8910 bus protocol) ──────── */ \
    PLBL_CAT(BUS_CONTROL)                                                       \
    PLBL_PIN(ARDY,        "ARDY",       "port A ready output (Z80 PIO)")        \
    PLBL_PIN(ASTB,        "ASTB",       "port A strobe input (Z80 PIO)")        \
    PLBL_PIN(BC1,         "BC1",        "bus control 1 (AY-3-8910)")            \
    PLBL_PIN(BC2,         "BC2",        "bus control 2 (AY-3-8910)")            \
    PLBL_PIN(BDIR,        "BDIR",       "bus direction (AY-3-8910)")            \
    PLBL_PIN(BRDY,        "BRDY",       "port B ready output (Z80 PIO)")        \
    PLBL_PIN(BSTB,        "BSTB",       "port B strobe input (Z80 PIO)")        \
    PLBL_PIN(B_ASEL,      "B/A\u0305",  "port select (Z80 PIO)")                \
    PLBL_PIN(C_DSEL,      "C/D\u0305",  "control/data (Z80 PIO/CTC)")           \
    \
    /* ── Interrupt ───────────────────────────────────────────────── */ \
    PLBL_CAT(INTERRUPT)                                                         \
    PLBL_PIN(ABORT,       "ABORT",      "abort (65C816, active-high)")          \
    PLBL_PIN(FIRQ,        "FIRQ",       "fast IRQ (MC6809, active-high)")       \
    PLBL_PIN(IC,          "IC",         "initial clear (Yamaha, active-high)")  \
    PLBL_PIN(IEI,         "IEI",        "interrupt enable in (Z80 daisy)")      \
    PLBL_PIN(IEO,         "IEO",        "interrupt enable out (Z80 daisy)")     \
    PLBL_PIN(IPL0,        "IPL0",       "interrupt priority 0 (M68K, act-h)")   \
    PLBL_PIN(IPL1,        "IPL1",       "interrupt priority 1 (M68K, act-h)")   \
    PLBL_PIN(IPL2,        "IPL2",       "interrupt priority 2 (M68K, act-h)")   \
    PLBL_PIN(IRQ,         "IRQ",        "interrupt request (active-high)")      \
    PLBL_PIN(IRQA,        "IRQA",       "interrupt request A (PIA, act-h)")     \
    PLBL_PIN(IRQB,        "IRQB",       "interrupt request B (PIA, act-h)")     \
    PLBL_PIN(INTR,        "INTR",       "interrupt (MOS 8563 VDC, act-h)")     \
    PLBL_PIN(LIC,         "LIC",        "last instruction cycle (MC6809)")      \
    PLBL_PIN(NMI,         "NMI",        "non-maskable interrupt (active-high)") \
    PLBL_PIN(RES,         "RES",        "reset (active-high)")                  \
    \
    /* ── Function code (M68K) ────────────────────────────────────── */ \
    PLBL_CAT(FUNCTION_CODE)                                                     \
    PLBL_PIN(FC0,         "FC0",        "function code 0 (M68K)")               \
    PLBL_PIN(FC1,         "FC1",        "function code 1 (M68K)")               \
    PLBL_PIN(FC2,         "FC2",        "function code 2 (M68K)")               \
    \
    /* ── Special / status ────────────────────────────────────────── */ \
    PLBL_CAT(SPECIAL)                                                           \
    PLBL_PIN(E,           "E",          "emulation mode (65C816)")              \
    PLBL_PIN(ML,          "ML",         "memory lock (active-high)")            \
    PLBL_PIN(MX,          "MX",         "memory/index size status (65C816)")    \
    PLBL_PIN(SO,          "SO",         "set overflow (active-high)")           \
    PLBL_PIN(TEST,        "TEST",       "test mode pin")                        \
    PLBL_PIN(TESTA,       "TEST",       "test pin (active-high)")               \
    PLBL_PIN(VDA,         "VDA",        "valid data address (65C816)")          \
    PLBL_PIN(VP,          "VP",         "vector pull (active-high)")            \
    PLBL_PIN(VPA,         "VPA",        "valid program address (65C816)")       \
    PLBL_PIN(VPA_M68K,    "VPA",        "valid periph addr (M68K, act-h)")      \
    PLBL_PIN(VPB,         "VPB",        "vector pull bar (active-high)")        \
    \
    /* ── I/O port (6510-specific P0-P7, must remain sequential) ──── */ \
    PLBL_CAT(IO_PORT)                                                           \
    PLBL_PIN(P0,          "P0",         "I/O port 0")                           \
    PLBL_PIN(P1,          "P1",         "I/O port 1")                           \
    PLBL_PIN(P2,          "P2",         "I/O port 2")                           \
    PLBL_PIN(P3,          "P3",         "I/O port 3")                           \
    PLBL_PIN(P4,          "P4",         "I/O port 4")                           \
    PLBL_PIN(P5,          "P5",         "I/O port 5")                           \
    PLBL_PIN(P6,          "P6",         "I/O port 6")                           \
    PLBL_PIN(P7,          "P7",         "I/O port 7")                           \
    \
    /* ── GPIO Port A (PA0-PA7 must remain sequential) ────────────── */ \
    PLBL_CAT(PORT_A)                                                            \
    PLBL_PIN(IOA0,        "IOA0",       "SSG I/O port A 0 (Yamaha FM)")         \
    PLBL_PIN(IOA1,        "IOA1",       "SSG I/O port A 1 (Yamaha FM)")         \
    PLBL_PIN(IOA2,        "IOA2",       "SSG I/O port A 2 (Yamaha FM)")         \
    PLBL_PIN(IOA3,        "IOA3",       "SSG I/O port A 3 (Yamaha FM)")         \
    PLBL_PIN(IOA4,        "IOA4",       "SSG I/O port A 4 (Yamaha FM)")         \
    PLBL_PIN(IOA5,        "IOA5",       "SSG I/O port A 5 (Yamaha FM)")         \
    PLBL_PIN(IOA6,        "IOA6",       "SSG I/O port A 6 (Yamaha FM)")         \
    PLBL_PIN(IOA7,        "IOA7",       "SSG I/O port A 7 (Yamaha FM)")         \
    PLBL_PIN(PA0,         "PA0",        "port A bit 0")                         \
    PLBL_PIN(PA1,         "PA1",        "port A bit 1")                         \
    PLBL_PIN(PA2,         "PA2",        "port A bit 2")                         \
    PLBL_PIN(PA3,         "PA3",        "port A bit 3")                         \
    PLBL_PIN(PA4,         "PA4",        "port A bit 4")                         \
    PLBL_PIN(PA5,         "PA5",        "port A bit 5")                         \
    PLBL_PIN(PA6,         "PA6",        "port A bit 6")                         \
    PLBL_PIN(PA7,         "PA7",        "port A bit 7")                         \
    \
    /* ── GPIO Port B (PB0-PB7 must remain sequential) ────────────── */ \
    PLBL_CAT(PORT_B)                                                            \
    PLBL_PIN(IOB0,        "IOB0",       "SSG I/O port B 0 (Yamaha FM)")         \
    PLBL_PIN(IOB1,        "IOB1",       "SSG I/O port B 1 (Yamaha FM)")         \
    PLBL_PIN(IOB2,        "IOB2",       "SSG I/O port B 2 (Yamaha FM)")         \
    PLBL_PIN(IOB3,        "IOB3",       "SSG I/O port B 3 (Yamaha FM)")         \
    PLBL_PIN(IOB4,        "IOB4",       "SSG I/O port B 4 (Yamaha FM)")         \
    PLBL_PIN(IOB5,        "IOB5",       "SSG I/O port B 5 (Yamaha FM)")         \
    PLBL_PIN(IOB6,        "IOB6",       "SSG I/O port B 6 (Yamaha FM)")         \
    PLBL_PIN(IOB7,        "IOB7",       "SSG I/O port B 7 (Yamaha FM)")         \
    PLBL_PIN(PB0,         "PB0",        "port B bit 0")                         \
    PLBL_PIN(PB1,         "PB1",        "port B bit 1")                         \
    PLBL_PIN(PB2,         "PB2",        "port B bit 2")                         \
    PLBL_PIN(PB3,         "PB3",        "port B bit 3")                         \
    PLBL_PIN(PB4,         "PB4",        "port B bit 4")                         \
    PLBL_PIN(PB5,         "PB5",        "port B bit 5")                         \
    PLBL_PIN(PB6,         "PB6",        "port B bit 6")                         \
    PLBL_PIN(PB7,         "PB7",        "port B bit 7")                         \
    \
    /* ── GPIO Port C (PC0-PC7 must remain sequential) ────────────── */ \
    PLBL_CAT(PORT_C)                                                            \
    PLBL_PIN(PC0,         "PC0",        "port C bit 0")                         \
    PLBL_PIN(PC1,         "PC1",        "port C bit 1")                         \
    PLBL_PIN(PC2,         "PC2",        "port C bit 2")                         \
    PLBL_PIN(PC3,         "PC3",        "port C bit 3")                         \
    PLBL_PIN(PC4,         "PC4",        "port C bit 4")                         \
    PLBL_PIN(PC5,         "PC5",        "port C bit 5")                         \
    PLBL_PIN(PC6,         "PC6",        "port C bit 6")                         \
    PLBL_PIN(PC7,         "PC7",        "port C bit 7")                         \
    \
    /* ── GPIO Port D (PD0-PD7 must remain sequential) ────────────── */ \
    PLBL_CAT(PORT_D)                                                            \
    PLBL_PIN(PD0,         "PD0",        "port D bit 0")                         \
    PLBL_PIN(PD1,         "PD1",        "port D bit 1")                         \
    PLBL_PIN(PD2,         "PD2",        "port D bit 2")                         \
    PLBL_PIN(PD3,         "PD3",        "port D bit 3")                         \
    PLBL_PIN(PD4,         "PD4",        "port D bit 4")                         \
    PLBL_PIN(PD5,         "PD5",        "port D bit 5")                         \
    PLBL_PIN(PD6,         "PD6",        "port D bit 6")                         \
    PLBL_PIN(PD7,         "PD7",        "port D bit 7")                         \
    \
    /* ── Serial communication ────────────────────────────────────── */ \
    PLBL_CAT(SERIAL)                                                            \
    PLBL_PIN(SPI_CLK,     "SPI CLK",    "SPI clock")                            \
    PLBL_PIN(SPI_CS,      "SPI CS",     "SPI chip select")                      \
    PLBL_PIN(SPI_MISO,    "SPI MISO",   "SPI data in")                          \
    PLBL_PIN(SPI_MOSI,    "SPI MOSI",   "SPI data out")                         \
    PLBL_PIN(UART_RX,     "UART RX",    "UART receive")                         \
    PLBL_PIN(UART_TX,     "UART TX",    "UART transmit")                        \
    \
    /* ── PWM outputs ─────────────────────────────────────────────── */ \
    PLBL_CAT(PWM)                                                               \
    PLBL_PIN(PWM0,        "PWM0",       "PWM output 0")                         \
    PLBL_PIN(PWM1,        "PWM1",       "PWM output 1")                         \
    PLBL_PIN(PWM2,        "PWM2",       "PWM output 2")                         \
    PLBL_PIN(PWM3,        "PWM3",       "PWM output 3")                         \
    \
    /* ── Video output ────────────────────────────────────────────── */ \
    PLBL_CAT(VIDEO)                                                             \
    PLBL_PIN(CHROMA,      "CHROMA",     "chrominance output")                   \
    PLBL_PIN(COLOR,       "COLOR",      "color signal output (VIC-II)")         \
    PLBL_PIN(COLU,        "COLU",       "color/luminance output (TIA)")         \
    PLBL_PIN(COMVID,      "COMVID",     "composite video output (TMS9918)")     \
    PLBL_PIN(COMP_BLK,    "BLK",        "composite blank (TIA)")                \
    PLBL_PIN(CSYNC,       "CSYNC",      "composite sync")                       \
    PLBL_PIN(HSYNC,       "HSYNC",      "horizontal sync")                      \
    PLBL_PIN(LUMA,        "LUMA",       "luminance output")                     \
    PLBL_PIN(RGBI,        "RGBI",       "RGBI digital video (MOS 8563 VDC)")    \
    PLBL_PIN(VOUT,        "VOUT",       "composite video output")               \
    PLBL_PIN(VSYNC,       "VSYNC",      "vertical sync")                        \
    \
    /* ── Video control ───────────────────────────────────────────── */ \
    PLBL_CAT(VIDEO_CONTROL)                                                     \
    PLBL_PIN(CURSOR,      "CURSOR",     "cursor output (MC6845 CRTC)")          \
    PLBL_PIN(DRDY,        "DRDY",       "DRAM ready output (MOS 8563 VDC)")     \
    PLBL_PIN(DE,          "DE",         "display enable (MC6845 CRTC)")         \
    PLBL_PIN(EXTVDP,      "EXTVDP",     "external VDP input (TMS9918)")         \
    PLBL_PIN(DUMP,        "DUMP",       "paddle dump/discharge (TIA)")          \
    PLBL_PIN(HBLANK,      "HBLANK",     "horizontal blank output (5A22)")       \
    PLBL_PIN(LIGHT_PEN,   "LP",         "light pen input")                      \
    PLBL_PIN(LPEN,        "LP",         "light pen input (MOS 8563 VDC)")       \
    PLBL_PIN(LPSTB,       "LPSTB",      "light pen strobe (MC6845 CRTC)")       \
    PLBL_PIN(REFRESH,     "REFRESH",    "WRAM refresh output (5A22)")           \
    PLBL_PIN(VBLANK,      "VBLANK",     "vertical blank output (5A22)")         \
    \
    /* ── Raster address (RA0-RA4, MC6845, must remain sequential) ── */ \
    PLBL_CAT(RASTER_ADDRESS)                                                    \
    PLBL_PIN(RA0,         "RA0",        "raster addr 0")                        \
    PLBL_PIN(RA1,         "RA1",        "raster addr 1")                        \
    PLBL_PIN(RA2,         "RA2",        "raster addr 2")                        \
    PLBL_PIN(RA3,         "RA3",        "raster addr 3")                        \
    PLBL_PIN(RA4,         "RA4",        "raster addr 4")                        \
    \
    /* ── Video address (PLA / VIC-II address lines) ──────────────── */ \
    PLBL_CAT(VIDEO_ADDRESS)                                                     \
    PLBL_PIN(VA12,        "VA12",       "video address 12")                     \
    PLBL_PIN(VA13,        "VA13",       "video address 13")                     \
    PLBL_PIN(VA14,        "VA14",       "video address 14 (active-high)")       \
    \
    /* ── Audio signals ───────────────────────────────────────────── */ \
    PLBL_CAT(AUDIO)                                                             \
    PLBL_PIN(ANALOG_OUT,  "ANALOG OUT", "analog output (Yamaha OPNA)")          \
    PLBL_PIN(AUD,         "AUD",        "audio output (Atari POKEY)")           \
    PLBL_PIN(AUD0,        "AUD0",       "audio output 0 (TIA)")                 \
    PLBL_PIN(AUD1,        "AUD1",       "audio output 1 (TIA)")                 \
    PLBL_PIN(AUDIO_IN,    "AUDIO IN",   "audio input")                          \
    PLBL_PIN(AUDIO_OUT,   "AUDIO OUT",  "audio output")                         \
    PLBL_PIN(CH3_OUT,     "CH3",        "FM channel 3 output (Yamaha OPN)")     \
    PLBL_PIN(CHANNEL_A,   "CH A",       "analog channel A (AY-3-8910)")         \
    PLBL_PIN(CHANNEL_B,   "CH B",       "analog channel B (AY-3-8910)")         \
    PLBL_PIN(CHANNEL_C,   "CH C",       "analog channel C (AY-3-8910)")         \
    PLBL_PIN(DA,          "DA",         "D/A converter output (Yamaha FM)")     \
    PLBL_PIN(EAR,         "EAR",        "tape EAR input")                       \
    PLBL_PIN(MIC,         "MIC",        "tape MIC output")                      \
    PLBL_PIN(MO,          "MO",         "mixed output (Yamaha FM)")             \
    PLBL_PIN(NOISE,       "NOISE",      "noise output")                         \
    PLBL_PIN(OPO,         "OPO",        "operator output (Yamaha FM)")          \
    PLBL_PIN(OSC1,        "OSC1",       "oscillator output 1")                  \
    PLBL_PIN(OSC2,        "OSC2",       "oscillator output 2")                  \
    PLBL_PIN(OSC3,        "OSC3",       "oscillator output 3")                  \
    PLBL_PIN(RO,          "RO",         "rhythm output (Yamaha OPLL)")          \
    PLBL_PIN(SH1,         "SH1",        "sample-and-hold 1 (Yamaha FM)")        \
    PLBL_PIN(SH2,         "SH2",        "sample-and-hold 2 (Yamaha FM)")        \
    PLBL_PIN(SND1,        "SND1",       "sound output 1 (Ricoh 2A03)")          \
    PLBL_PIN(SND2,        "SND2",       "sound output 2 (Ricoh 2A03)")          \
    PLBL_PIN(SOUND,       "SOUND",      "sound output (VIC-I/II)")              \
    PLBL_PIN(SPEAKER,     "SPKR",       "speaker output")                       \
    PLBL_PIN(SSG_OUT,     "SSG",        "SSG section output (Yamaha OPN)")      \
    \
    /* ── Analog ──────────────────────────────────────────────────── */ \
    PLBL_CAT(ANALOG)                                                            \
    PLBL_PIN(AD_FM,       "AD",         "A/D converter input (Yamaha FM)")      \
    PLBL_PIN(AIN0,        "AIN0",       "analog in 0")                          \
    PLBL_PIN(AIN1,        "AIN1",       "analog in 1")                          \
    PLBL_PIN(AIN2,        "AIN2",       "analog in 2")                          \
    PLBL_PIN(AIN3,        "AIN3",       "analog in 3")                          \
    PLBL_PIN(AIN4,        "AIN4",       "analog in 4")                          \
    PLBL_PIN(AIN5,        "AIN5",       "analog in 5")                          \
    PLBL_PIN(AIN6,        "AIN6",       "analog in 6")                          \
    PLBL_PIN(AIN7,        "AIN7",       "analog in 7")                          \
    PLBL_PIN(AOUT0,       "AOUT0",      "analog output 0")                      \
    PLBL_PIN(AOUT1,       "AOUT1",      "analog output 1")                      \
    PLBL_PIN(C_DAC,       "C",          "DAC capacitor (Yamaha FM)")            \
    PLBL_PIN(VREF,        "VREF",       "voltage reference")                    \
    \
    /* ── SID analog (filter caps, pots, ext audio) ───────────────── */ \
    PLBL_CAT(SID_ANALOG)                                                        \
    PLBL_PIN(CAP1A,       "CAP1A",      "filter capacitor 1A")                  \
    PLBL_PIN(CAP1B,       "CAP1B",      "filter capacitor 1B")                  \
    PLBL_PIN(CAP2A,       "CAP2A",      "filter capacitor 2A")                  \
    PLBL_PIN(CAP2B,       "CAP2B",      "filter capacitor 2B")                  \
    PLBL_PIN(EXT_IN,      "EXT IN",     "external audio input")                 \
    PLBL_PIN(FILTER_IN,   "FILT IN",    "filter input")                         \
    PLBL_PIN(FILTER_OUT,  "FILT OUT",   "filter output")                        \
    PLBL_PIN(POTX,        "POTX",       "paddle X input")                       \
    PLBL_PIN(POTY,        "POTY",       "paddle Y input")                       \
    \
    /* ── Timer / CIA (CNT, SP, TOD, FLAG, SDR, PC) ───────────────── */ \
    PLBL_CAT(TIMER)                                                             \
    PLBL_PIN(CNT,         "CNT",        "counter input")                        \
    PLBL_PIN(FLAG,        "FLAG",       "flag input")                           \
    PLBL_PIN(PC_CIA,      "PC",         "peripheral control output (CIA)")      \
    PLBL_PIN(SDR,         "SDR",        "serial data register")                 \
    PLBL_PIN(SP,          "SP",         "serial port")                          \
    PLBL_PIN(TOD,         "TOD",        "time of day clock")                    \
    \
    /* ── VIA handshake (MOS 6522: CA1/2, CB1/2) ─────────────────── */ \
    PLBL_CAT(HANDSHAKE)                                                         \
    PLBL_PIN(CA1,         "CA1",        "port A control line 1")                \
    PLBL_PIN(CA2,         "CA2",        "port A control line 2")                \
    PLBL_PIN(CB1,         "CB1",        "port B control line 1")                \
    PLBL_PIN(CB2,         "CB2",        "port B control line 2")                \
    \
    /* ── Z80 CTC (CLK_TRG / ZC_TO, must remain sequential) ──────── */ \
    PLBL_CAT(TIMER_CTC)                                                         \
    PLBL_PIN(CLK_TRG0,    "CLK/TRG0",   "clock/trigger 0 (Z80 CTC)")            \
    PLBL_PIN(CLK_TRG1,    "CLK/TRG1",   "clock/trigger 1 (Z80 CTC)")            \
    PLBL_PIN(CLK_TRG2,    "CLK/TRG2",   "clock/trigger 2 (Z80 CTC)")            \
    PLBL_PIN(CLK_TRG3,    "CLK/TRG3",   "clock/trigger 3 (Z80 CTC)")            \
    PLBL_PIN(ZC_TO0,      "ZC/TO0",     "zero count/timer 0 (Z80 CTC)")         \
    PLBL_PIN(ZC_TO1,      "ZC/TO1",     "zero count/timer 1 (Z80 CTC)")         \
    PLBL_PIN(ZC_TO2,      "ZC/TO2",     "zero count/timer 2 (Z80 CTC)")         \
    PLBL_PIN(ZC_TO3,      "ZC/TO3",     "zero count/timer 3 (Z80 CTC)")         \
    \
    /* ── Memory data I/O (DQ0-DQ7, DM0-DM7, must remain sequential) ── */ \
    PLBL_CAT(MEMORY)                                                            \
    PLBL_PIN(DM0,         "DM0",        "DRAM data 0 (Yamaha OPNA)")            \
    PLBL_PIN(DM1,         "DM1",        "DRAM data 1 (Yamaha OPNA)")            \
    PLBL_PIN(DM2,         "DM2",        "DRAM data 2 (Yamaha OPNA)")            \
    PLBL_PIN(DM3,         "DM3",        "DRAM data 3 (Yamaha OPNA)")            \
    PLBL_PIN(DM4,         "DM4",        "DRAM data 4 (Yamaha OPNA)")            \
    PLBL_PIN(DM5,         "DM5",        "DRAM data 5 (Yamaha OPNA)")            \
    PLBL_PIN(DM6,         "DM6",        "DRAM data 6 (Yamaha OPNA)")            \
    PLBL_PIN(DM7,         "DM7",        "DRAM data 7 (Yamaha OPNA)")            \
    PLBL_PIN(DQ0,         "DQ0",        "memory data I/O 0")                    \
    PLBL_PIN(DQ1,         "DQ1",        "memory data I/O 1")                    \
    PLBL_PIN(DQ2,         "DQ2",        "memory data I/O 2")                    \
    PLBL_PIN(DQ3,         "DQ3",        "memory data I/O 3")                    \
    PLBL_PIN(DQ4,         "DQ4",        "memory data I/O 4")                    \
    PLBL_PIN(DQ5,         "DQ5",        "memory data I/O 5")                    \
    PLBL_PIN(DQ6,         "DQ6",        "memory data I/O 6")                    \
    PLBL_PIN(DQ7,         "DQ7",        "memory data I/O 7")                    \
    PLBL_PIN(DTO,         "DTO",        "data output (Yamaha OPNA ADPCM)")      \
    PLBL_PIN(I_O1,        "I/O1",       "memory I/O 1 (MOS 2114)")              \
    PLBL_PIN(I_O2,        "I/O2",       "memory I/O 2 (MOS 2114)")              \
    PLBL_PIN(I_O3,        "I/O3",       "memory I/O 3 (MOS 2114)")              \
    PLBL_PIN(I_O4,        "I/O4",       "memory I/O 4 (MOS 2114)")              \
    \
    /* ── Memory address (MA0-MA15, must remain sequential) ────────── */ \
    PLBL_CAT(MEMORY_ADDRESS)                                                    \
    PLBL_PIN(MA0,         "MA0",        "memory addr 0")                        \
    PLBL_PIN(MA1,         "MA1",        "memory addr 1")                        \
    PLBL_PIN(MA2,         "MA2",        "memory addr 2")                        \
    PLBL_PIN(MA3,         "MA3",        "memory addr 3")                        \
    PLBL_PIN(MA4,         "MA4",        "memory addr 4")                        \
    PLBL_PIN(MA5,         "MA5",        "memory addr 5")                        \
    PLBL_PIN(MA6,         "MA6",        "memory addr 6")                        \
    PLBL_PIN(MA7,         "MA7",        "memory addr 7")                        \
    PLBL_PIN(MA8,         "MA8",        "memory addr 8")                        \
    PLBL_PIN(MA9,         "MA9",        "memory addr 9")                        \
    PLBL_PIN(MA10,        "MA10",       "memory addr 10")                       \
    PLBL_PIN(MA11,        "MA11",       "memory addr 11")                       \
    PLBL_PIN(MA12,        "MA12",       "memory addr 12")                       \
    PLBL_PIN(MA13,        "MA13",       "memory addr 13")                       \
    PLBL_PIN(MA14,        "MA14",       "memory addr 14")                       \
    PLBL_PIN(MA15,        "MA15",       "memory addr 15")                       \
    \
    /* ── PLA / ROM-RAM select outputs ────────────────────────────── */ \
    PLBL_CAT(PLA)                                                               \
    PLBL_PIN(BASIC,       "BASIC",      "BASIC ROM select (active-high)")       \
    PLBL_PIN(CASRAM_PLA,  "CASRAM",     "CAS RAM — PLA specific (active-high)") \
    PLBL_PIN(CHAREN,      "CHAREN",     "character ROM enable (active-high)")   \
    PLBL_PIN(CHAROM,      "CHAROM",     "character ROM select (active-high)")   \
    PLBL_PIN(EXROM,       "EXROM",      "external ROM (active-high)")           \
    PLBL_PIN(GAME,        "GAME",       "game line (active-high)")              \
    PLBL_PIN(GRW,         "GRW",        "graphics read/write")                  \
    PLBL_PIN(HIRAM,       "HIRAM",      "high RAM (active-high)")               \
    PLBL_PIN(IO,          "I/O",        "I/O select (active-high)")             \
    PLBL_PIN(KERNAL,      "KERNAL",     "KERNAL ROM select (active-high)")      \
    PLBL_PIN(LORAM,       "LORAM",      "low RAM (active-high)")                \
    PLBL_PIN(ROMH,        "ROMH",       "ROM high (active-high)")               \
    PLBL_PIN(ROML,        "ROML",       "ROM low (active-high)")                \
    \
    /* ── Keyboard matrix (K0-K7, must remain sequential) ─────────── */ \
    PLBL_CAT(KEYBOARD)                                                          \
    PLBL_PIN(K0,          "K0",         "keyboard 0")                           \
    PLBL_PIN(K1,          "K1",         "keyboard 1")                           \
    PLBL_PIN(K2,          "K2",         "keyboard 2")                           \
    PLBL_PIN(K3,          "K3",         "keyboard 3")                           \
    PLBL_PIN(K4,          "K4",         "keyboard 4")                           \
    PLBL_PIN(K5,          "K5",         "keyboard 5")                           \
    PLBL_PIN(K6,          "K6",         "keyboard 6")                           \
    PLBL_PIN(K7,          "K7",         "keyboard 7")                           \
    \
    /* ── Multiplexed addr/data & chip I/O (2A03 / 2C02 / TMS9918) ── */ \
    PLBL_CAT(NES_IO)                                                            \
    PLBL_PIN(AD0,         "AD0",        "multiplexed addr/data 0 (TMS9918)")    \
    PLBL_PIN(AD1,         "AD1",        "multiplexed addr/data 1 (2A03)")       \
    PLBL_PIN(AD2,         "AD2",        "multiplexed addr/data 2 (2A03)")       \
    PLBL_PIN(AD3,         "AD3",        "multiplexed addr/data 3 (TMS9918)")    \
    PLBL_PIN(AD4,         "AD4",        "multiplexed addr/data 4 (TMS9918)")    \
    PLBL_PIN(AD5,         "AD5",        "multiplexed addr/data 5 (TMS9918)")    \
    PLBL_PIN(AD6,         "AD6",        "multiplexed addr/data 6 (TMS9918)")    \
    PLBL_PIN(AD7,         "AD7",        "multiplexed addr/data 7 (TMS9918)")    \
    PLBL_PIN(EXT0,        "EXT0",       "PPU ext 0")                            \
    PLBL_PIN(EXT1,        "EXT1",       "PPU ext 1")                            \
    PLBL_PIN(EXT2,        "EXT2",       "PPU ext 2")                            \
    PLBL_PIN(EXT3,        "EXT3",       "PPU ext 3")                            \
    PLBL_PIN(IN0,         "IN0",        "controller data input 0 (2A03)")       \
    PLBL_PIN(IN1,         "IN1",        "controller data input 1 (2A03)")       \
    PLBL_PIN(OUT0,        "OUT0",       "controller strobe 0 (2A03)")           \
    PLBL_PIN(OUT1,        "OUT1",       "controller strobe 1 (2A03)")           \
    PLBL_PIN(OUT2,        "OUT2",       "controller strobe 2 (2A03)")           \
    \
    /* ── SNES-specific I/O (Ricoh 5A22) ──────────────────────────── */ \
    PLBL_CAT(SNES_IO)                                                           \
    PLBL_PIN(JOY1,        "JOY1",       "joypad 1 serial data (5A22)")          \
    PLBL_PIN(JOY2,        "JOY2",       "joypad 2 serial data (5A22)")          \
    PLBL_PIN(JOYCLK,      "JOYCLK",     "joypad clock output (5A22)")           \
    PLBL_PIN(JOYLAT,      "JOYLAT",     "joypad latch output (5A22)")           \
    PLBL_PIN(JOYRD,       "JOYRD",      "joypad auto-read strobe (5A22)")       \
    \
    /* ── TIA input (INPT0-5, must remain sequential) ─────────────── */ \
    PLBL_CAT(TIA_INPUT)                                                         \
    PLBL_PIN(INPT0,       "INPT0",      "TIA input 0")                          \
    PLBL_PIN(INPT1,       "INPT1",      "TIA input 1")                          \
    PLBL_PIN(INPT2,       "INPT2",      "TIA input 2")                          \
    PLBL_PIN(INPT3,       "INPT3",      "TIA input 3")                          \
    PLBL_PIN(INPT4,       "INPT4",      "TIA input 4")                          \
    PLBL_PIN(INPT5,       "INPT5",      "TIA input 5")                          \
    \
    /* ── MC6847 VDG mode pins ────────────────────────────────────── */ \
    PLBL_CAT(VDG)                                                               \
    PLBL_PIN(AG,          "AG",         "alpha/graphics mode (MC6847)")         \
    PLBL_PIN(AS,          "A/S",        "alpha/semigraphics (MC6847)")          \
    PLBL_PIN(CSS,         "CSS",        "color set select (MC6847)")            \
    PLBL_PIN(FS,          "FS",         "field sync output (MC6847)")           \
    PLBL_PIN(GM0,         "GM0",        "graphics mode 0 (MC6847)")             \
    PLBL_PIN(GM1,         "GM1",        "graphics mode 1 (MC6847)")             \
    PLBL_PIN(GM2,         "GM2",        "graphics mode 2 (MC6847)")             \
    PLBL_PIN(INTEXT,      "INT/EXT",    "internal/external chargen (MC6847)")   \
    PLBL_PIN(INV,         "INV",        "invert (MC6847)")                      \
    \
    /* ── POKEY I/O (Atari) ───────────────────────────────────────── */ \
    PLBL_CAT(POKEY_IO)                                                          \
    PLBL_PIN(BCLK_IN,     "BCLK IN",    "bidirectional clock input")            \
    PLBL_PIN(KR1,         "KR1",        "keyboard return 1")                    \
    PLBL_PIN(KR2,         "KR2",        "keyboard return 2")                    \
    PLBL_PIN(POT0,        "POT0",       "pot input 0")                          \
    PLBL_PIN(POT1,        "POT1",       "pot input 1")                          \
    PLBL_PIN(POT2,        "POT2",       "pot input 2")                          \
    PLBL_PIN(POT3,        "POT3",       "pot input 3")                          \
    PLBL_PIN(POT4,        "POT4",       "pot input 4")                          \
    PLBL_PIN(POT5,        "POT5",       "pot input 5")                          \
    PLBL_PIN(POT6,        "POT6",       "pot input 6")                          \
    PLBL_PIN(POT7,        "POT7",       "pot input 7")                          \
    PLBL_PIN(SIO_CLK_IN,  "SIO CLK IN", "serial I/O clock input")               \
    PLBL_PIN(SIO_CLK_OUT, "SIO CLK OUT", "serial I/O clock output")             \
    PLBL_PIN(SIO_IN,      "SIO IN",     "serial I/O data input")                \
    PLBL_PIN(SIO_OUT,     "SIO OUT",    "serial I/O data output")               \
    \
    /* ── Logic chip pins (I, Q, Y, S, G sequential sub-groups) ───── */ \
    PLBL_CAT(LOGIC)                                                             \
    PLBL_PIN(A,           "A",          "select A (74LS138)")                   \
    PLBL_PIN(B,           "B",          "select B (74LS138)")                   \
    PLBL_PIN(C,           "C",          "select C (74LS138)")                   \
    PLBL_PIN(DS,          "DS",         "data serial input (shift register)")   \
    PLBL_PIN(E1,          "E1",         "enable 1 (active-high)")               \
    PLBL_PIN(E2,          "E2",         "enable 2 (active-high)")               \
    PLBL_PIN(E3,          "E3",         "enable 3 (74LS138)")                   \
    PLBL_PIN(G,           "G",          "gate/enable")                          \
    PLBL_PIN(I0,          "I0",         "input 0")                              \
    PLBL_PIN(I1,          "I1",         "input 1")                              \
    PLBL_PIN(I2,          "I2",         "input 2")                              \
    PLBL_PIN(I3,          "I3",         "input 3")                              \
    PLBL_PIN(I4,          "I4",         "input 4")                              \
    PLBL_PIN(I5,          "I5",         "input 5")                              \
    PLBL_PIN(I6,          "I6",         "input 6")                              \
    PLBL_PIN(I7,          "I7",         "input 7")                              \
    PLBL_PIN(Q0,          "Q0",         "output 0")                             \
    PLBL_PIN(Q1,          "Q1",         "output 1")                             \
    PLBL_PIN(Q2,          "Q2",         "output 2")                             \
    PLBL_PIN(Q3,          "Q3",         "output 3")                             \
    PLBL_PIN(Q4,          "Q4",         "output 4")                             \
    PLBL_PIN(Q5,          "Q5",         "output 5")                             \
    PLBL_PIN(Q6,          "Q6",         "output 6")                             \
    PLBL_PIN(Q7,          "Q7",         "output 7")                             \
    PLBL_PIN(S0,          "S0",         "select 0")                             \
    PLBL_PIN(S1,          "S1",         "select 1")                             \
    PLBL_PIN(S2,          "S2",         "select 2")                             \
    PLBL_PIN(S3,          "S3",         "select 3")                             \
    PLBL_PIN(Y0,          "Y0",         "output 0")                             \
    PLBL_PIN(Y1,          "Y1",         "output 1")                             \
    PLBL_PIN(Y2,          "Y2",         "output 2")                             \
    PLBL_PIN(Y3,          "Y3",         "output 3")                             \
    PLBL_PIN(Y4,          "Y4",         "output 4")                             \
    PLBL_PIN(Y5,          "Y5",         "output 5")                             \
    PLBL_PIN(Y6,          "Y6",         "output 6")                             \
    PLBL_PIN(Y7,          "Y7",         "output 7")                             \
    \
    /* ── Differential pairs ──────────────────────────────────────── */ \
    PLBL_CAT(DIFFERENTIAL)                                                      \
    \
    /* ── No-connect ──────────────────────────────────────────────── */ \
    PLBL_CAT(NO_CONNECT)                                                        \
    PLBL_PIN(NC,          "NC",         "no connect")                           \
    /* end of PIN_LABELS */

// ============================================================================
// PinType enum — generated from CAT entries
// ============================================================================

enum class PinType {
#define PINTYPE_GEN_INV_(id, str, cmt)
#define PINTYPE_GEN_CAT_(t)            t,
#define PINTYPE_GEN_PIN_(id, str, cmt)
    PIN_LABELS(PINTYPE_GEN_INV_, PINTYPE_GEN_CAT_, PINTYPE_GEN_PIN_)
#undef PINTYPE_GEN_INV_
#undef PINTYPE_GEN_CAT_
#undef PINTYPE_GEN_PIN_
};

// ============================================================================
// PinLabel enum — generated from INV + CAT + PIN entries
// ============================================================================
//
// Layout:
//   [INV entries]  — active-low labels
//   ACTIVE_LOW_END — sentinel
//   [_X_BEGIN, PIN entries] per CAT — active-high labels with range sentinels
//   _LABEL_END     — final sentinel
//   UNKNOWN
//

enum class PinLabel {
    // Pass 1: emit active-low labels only
#define PINLABEL_INV1_(id, str, cmt)   id,
#define PINLABEL_CAT1_(t)
#define PINLABEL_PIN1_(id, str, cmt)
    PIN_LABELS(PINLABEL_INV1_, PINLABEL_CAT1_, PINLABEL_PIN1_)
#undef PINLABEL_INV1_
#undef PINLABEL_CAT1_
#undef PINLABEL_PIN1_

    ACTIVE_LOW_END,

    // Pass 2: emit category sentinels and active-high labels
#define PINLABEL_INV2_(id, str, cmt)
#define PINLABEL_CAT2_(t)              _##t##_BEGIN,
#define PINLABEL_PIN2_(id, str, cmt)   id,
    PIN_LABELS(PINLABEL_INV2_, PINLABEL_CAT2_, PINLABEL_PIN2_)
#undef PINLABEL_INV2_
#undef PINLABEL_CAT2_
#undef PINLABEL_PIN2_

    _LABEL_END,
    UNKNOWN = _LABEL_END
};

// ============================================================================
// PIN SIDE
// ============================================================================

enum class PinSide {
    LEFT,         // Left side pins (top to bottom)
    RIGHT,        // Right side pins (top to bottom) 
    TOP,          // Top side pins (left to right)
    BOTTOM
};

// ============================================================================
// PIN STRUCTURES
// ============================================================================

// Forward declaration
class ChipBase;

// Die pin definition structure with enum-based labels for performance
struct ChipPin {
    uint8_t pin_number;           // Physical pin number
    PinLabel label;               // Pin label enum for fast comparisons
    const char* alt_function;     // Alternate function name
    bool is_differential_pos;     // True if positive side of differential pair
    bool is_differential_neg;     // True if negative side of differential pair

    // Derived properties — computed from label
    PinType get_pin_type() const;
    uint8_t get_bit_index() const;
    bool get_invert_logic() const;
    const char* get_group_name() const;
};

// Pin signal state for real-time visualization
struct PinSignalState {
    uint8_t pin_number;           // Pin number to match ChipPin
    bool signal_level;            // Current signal level (high/low)
    bool drive_direction;         // True if pin drives output, false if receives input
    uint8_t signal_value;         // For multi-bit values or analog levels (0-255)
    bool high_impedance;          // True if pin is in high-impedance (Hi-Z) state
    bool has_pullup;              // Pin has pull-up resistor
    bool has_pulldown;            // Pin has pull-down resistor
    bool signal_valid;            // True if pin signal state is valid/available
    float analog_voltage;         // For analog pins (0.0 - VCC)
    bool is_pwm;                  // True if pin is PWM output
    float pwm_duty_cycle;         // PWM duty cycle (0.0 - 1.0)
};

// BGA grid position (for BGA/LGA packages)
struct BGAPosition {
    uint8_t row;    // Row (A, B, C, ...)
    uint8_t col;    // Column (1, 2, 3, ...)
};

// ============================================================================
// PIN DEFINITION MACROS
// ============================================================================

#define CHIP_PIN(num, lbl_enum) \
    {num, PinLabel::lbl_enum, nullptr, false, false}

inline ChipPin make_pin(uint8_t num, PinLabel label = PinLabel::NC,
                        const char* alt_function = nullptr) {
    return ChipPin{num, label, alt_function, false, false};
}

// ============================================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================================

uint8_t get_bit_index_from_label(PinLabel label);
bool get_invert_logic_from_label(PinLabel label);
const char* pin_type_to_group_name(PinType type);

// Enum-to-string conversion functions
const char* pin_label_to_string(PinLabel label);
const char* pin_label_to_description(PinLabel label);
std::string pin_label_to_display_string(PinLabel label);

// Derive pin type from pin label
PinType pin_label_to_pin_type(PinLabel label);

// ============================================================================
// PIN LABEL EQUIVALENCE
// ============================================================================
//
// Maps any PinLabel to its canonical functional identity.
// Labels that ARE canonical return themselves (via the default case).
// Every non-identity mapping targets a fixed point — verified by static_assert.
//
// Equivalence classes:
//   Polarity pairs:  _FOO  → FOO  (active-low → active-high canonical)
//   Power synonyms:  GND   → VSS,   VCC   → VDD
//   Name synonyms:   PHI_M → CLK,   MO    → AUDIO_OUT,  IC → RES, etc.
//   Memory I/O:      I_O1  → DQ0,   I_O2  → DQ1,  etc.
//
// MAINTENANCE: when adding a case `X → Y`, ensure Y is itself canonical
// (not mapped to something else).  The static_assert below will fire if not.
//
constexpr PinLabel pin_canonical(PinLabel label) {
    switch (label) {
    // ── Polarity pairs (active-low → active-high) ──────────────────
    case PinLabel::_ABORT:       return PinLabel::ABORT;
    case PinLabel::_AEC:         return PinLabel::AEC;
    case PinLabel::_AS_M68K:     return PinLabel::AS_M68K;
    case PinLabel::_BASIC:       return PinLabel::BASIC;
    case PinLabel::_BERR:        return PinLabel::BERR;
    case PinLabel::_BG:          return PinLabel::BG;
    case PinLabel::_BGACK:       return PinLabel::BGACK;
    case PinLabel::_BR:          return PinLabel::BR;
    case PinLabel::_BUSAK:       return PinLabel::BUSAK;
    case PinLabel::_BUSRQ:       return PinLabel::BUSRQ;
    case PinLabel::_CAS:         return PinLabel::CAS;
    case PinLabel::_CASRAM_PLA:  return PinLabel::CASRAM_PLA;
    case PinLabel::_CE:          return PinLabel::CE;
    case PinLabel::_CHAREN:      return PinLabel::CHAREN;
    case PinLabel::_CHAROM:      return PinLabel::CHAROM;
    case PinLabel::_CLR:         return PinLabel::CLR;
    case PinLabel::_CS:          return PinLabel::CS;
    case PinLabel::_CS0:         return PinLabel::CS0;
    case PinLabel::_CS1:         return PinLabel::CS1;
    case PinLabel::_CS2:         return PinLabel::CS2;
    case PinLabel::_CSR:         return PinLabel::CSR;
    case PinLabel::_CSW:         return PinLabel::CSW;
    case PinLabel::_DTACK:       return PinLabel::DTACK;
    case PinLabel::_E1:          return PinLabel::E1;
    case PinLabel::_E2:          return PinLabel::E2;
    case PinLabel::_EXROM:       return PinLabel::EXROM;
    case PinLabel::_FIRQ:        return PinLabel::FIRQ;
    case PinLabel::_G:           return PinLabel::G;
    case PinLabel::_GAME:        return PinLabel::GAME;
    case PinLabel::_HALT:        return PinLabel::HALT;
    case PinLabel::_HIRAM:       return PinLabel::HIRAM;
    case PinLabel::_IC:          return PinLabel::RES;    // /IC = reset (Yamaha)
    case PinLabel::_INT:         return PinLabel::INT;
    case PinLabel::_IO:          return PinLabel::IO;
    case PinLabel::_IORQ:        return PinLabel::IORQ;
    case PinLabel::_IPL0:        return PinLabel::IPL0;
    case PinLabel::_IPL1:        return PinLabel::IPL1;
    case PinLabel::_IPL2:        return PinLabel::IPL2;
    case PinLabel::_IRQ:         return PinLabel::IRQ;
    case PinLabel::_IRQA:        return PinLabel::IRQA;
    case PinLabel::_IRQB:        return PinLabel::IRQB;
    case PinLabel::_INTR:        return PinLabel::INTR;
    case PinLabel::_KERNAL:      return PinLabel::KERNAL;
    case PinLabel::_LDS:         return PinLabel::LDS;
    case PinLabel::_LORAM:       return PinLabel::LORAM;
    case PinLabel::_M1:          return PinLabel::M1;
    case PinLabel::_ML:          return PinLabel::ML;
    case PinLabel::_MREQ:        return PinLabel::MREQ;
    case PinLabel::_NMI:         return PinLabel::NMI;
    case PinLabel::_OE:          return PinLabel::OE;
    case PinLabel::_PARD:        return PinLabel::PARD;
    case PinLabel::_PAWR:        return PinLabel::PAWR;
    case PinLabel::_Q7:          return PinLabel::Q7;
    case PinLabel::_RAS:         return PinLabel::RAS;
    case PinLabel::_RD:          return PinLabel::RD;
    case PinLabel::_RES:         return PinLabel::RES;
    case PinLabel::_RFSH:        return PinLabel::RFSH;
    case PinLabel::_ROMH:        return PinLabel::ROMH;
    case PinLabel::_ROML:        return PinLabel::ROML;
    case PinLabel::_ROMSEL:      return PinLabel::ROMSEL;
    case PinLabel::_SO:          return PinLabel::SO;
    case PinLabel::_TEST:        return PinLabel::TESTA;
    case PinLabel::_UDS:         return PinLabel::UDS;
    case PinLabel::_VA14:        return PinLabel::VA14;
    case PinLabel::_VMA:         return PinLabel::VMA;
    case PinLabel::_VP:          return PinLabel::VP;
    case PinLabel::_VPA_M68K:    return PinLabel::VPA_M68K;
    case PinLabel::_VPB:         return PinLabel::VPB;
    case PinLabel::_WAIT:        return PinLabel::WAIT;
    case PinLabel::_WE:          return PinLabel::WE;
    case PinLabel::_WR:          return PinLabel::WR;
    case PinLabel::_WRAM:        return PinLabel::WRAM;
    case PinLabel::_Y0:          return PinLabel::Y0;
    case PinLabel::_Y1:          return PinLabel::Y1;
    case PinLabel::_Y2:          return PinLabel::Y2;
    case PinLabel::_Y3:          return PinLabel::Y3;
    case PinLabel::_Y4:          return PinLabel::Y4;
    case PinLabel::_Y5:          return PinLabel::Y5;
    case PinLabel::_Y6:          return PinLabel::Y6;
    case PinLabel::_Y7:          return PinLabel::Y7;

    // ── Power synonyms ─────────────────────────────────────────────
    case PinLabel::GND:          return PinLabel::VSS;
    case PinLabel::VCC:          return PinLabel::VDD;

    // ── Name synonyms (chip-specific → generic canonical) ──────────
    case PinLabel::ANALOG_OUT:   return PinLabel::AUDIO_OUT;
    case PinLabel::AUD:          return PinLabel::AUDIO_OUT;
    case PinLabel::E_CLK:        return PinLabel::ENABLE;  // 6800-bus E clock
    case PinLabel::IC:           return PinLabel::RES;     // IC = reset (Yamaha)
    case PinLabel::MO:           return PinLabel::AUDIO_OUT;
    case PinLabel::PHI_M:        return PinLabel::CLK;     // master clock

    // ── SSG port synonyms (IOA→PA, IOB→PB) ─────────────────────────
    case PinLabel::IOA0:         return PinLabel::PA0;
    case PinLabel::IOA1:         return PinLabel::PA1;
    case PinLabel::IOA2:         return PinLabel::PA2;
    case PinLabel::IOA3:         return PinLabel::PA3;
    case PinLabel::IOA4:         return PinLabel::PA4;
    case PinLabel::IOA5:         return PinLabel::PA5;
    case PinLabel::IOA6:         return PinLabel::PA6;
    case PinLabel::IOA7:         return PinLabel::PA7;
    case PinLabel::IOB0:         return PinLabel::PB0;
    case PinLabel::IOB1:         return PinLabel::PB1;
    case PinLabel::IOB2:         return PinLabel::PB2;
    case PinLabel::IOB3:         return PinLabel::PB3;
    case PinLabel::IOB4:         return PinLabel::PB4;
    case PinLabel::IOB5:         return PinLabel::PB5;
    case PinLabel::IOB6:         return PinLabel::PB6;
    case PinLabel::IOB7:         return PinLabel::PB7;

    // ── Power synonyms (analog) ────────────────────────────────────
    case PinLabel::AGND:         return PinLabel::VSS;
    case PinLabel::AVCC:         return PinLabel::VDD;

    // ── ADPCM memory data synonyms (DM0..7 → DQ0..7) ──────────────
    case PinLabel::DM0:          return PinLabel::DQ0;
    case PinLabel::DM1:          return PinLabel::DQ1;
    case PinLabel::DM2:          return PinLabel::DQ2;
    case PinLabel::DM3:          return PinLabel::DQ3;
    case PinLabel::DM4:          return PinLabel::DQ4;
    case PinLabel::DM5:          return PinLabel::DQ5;
    case PinLabel::DM6:          return PinLabel::DQ6;
    case PinLabel::DM7:          return PinLabel::DQ7;

    // ── Memory I/O synonyms (MOS 2114 I/O1..4 → DQ0..3) ──────────
    case PinLabel::I_O1:         return PinLabel::DQ0;
    case PinLabel::I_O2:         return PinLabel::DQ1;
    case PinLabel::I_O3:         return PinLabel::DQ2;
    case PinLabel::I_O4:         return PinLabel::DQ3;

    default: return label;
    }
}

// True if two pin labels represent the same underlying function,
// regardless of name variant or active-low/active-high polarity.
constexpr bool pin_equivalent(PinLabel a, PinLabel b) {
    return pin_canonical(a) == pin_canonical(b);
}

// Compile-time idempotency proof: pin_canonical(pin_canonical(x)) == pin_canonical(x)
// for every label in the enum.
constexpr bool pin_canonical_is_idempotent() {
    for (int i = 0; i <= static_cast<int>(PinLabel::UNKNOWN); ++i) {
        auto l = static_cast<PinLabel>(i);
        if (pin_canonical(pin_canonical(l)) != pin_canonical(l))
            return false;
    }
    return true;
}
static_assert(pin_canonical_is_idempotent(),
    "pin_canonical() is not idempotent — a chain exists (X→Y where Y→Z)");

// Compile-time coverage proof: every active-low label must resolve to a
// non-active-low canonical.  A missing case in pin_canonical() returns the
// label unchanged; the recursive call in pin_label_to_pin_type() then
// stack-overflows at runtime.  Catch it here instead.
constexpr bool pin_canonical_covers_all_active_low() {
    for (int i = 0; i < static_cast<int>(PinLabel::ACTIVE_LOW_END); ++i) {
        auto l = static_cast<PinLabel>(i);
        if (pin_canonical(l) < PinLabel::ACTIVE_LOW_END)
            return false;  // active-low mapped to active-low → infinite recursion
    }
    return true;
}
static_assert(pin_canonical_covers_all_active_low(),
    "pin_canonical() has an unmapped active-low label — add the missing case");

// ============================================================================
// PIN-TO-BUS-BIT MAPPING
// ============================================================================

struct PinBusMapping {
    int bus_bit;
    bool is_input;
    bool invert;
};

constexpr PinBusMapping get_pin_bus_mapping(PinLabel label) {
    switch (label) {
    // Control signals (active-high)
    case PinLabel::RW:     return { BUS_RW_BIT,    false, false };
    case PinLabel::SYNC:   return { BUS_SYNC_BIT,  false, false };
    case PinLabel::RDY:    return { BUS_RDY_BIT,   true,  false };
    case PinLabel::AEC:    return { BUS_AEC_BIT,   false, false };
    case PinLabel::BE:     return { BUS_BE_BIT,    true,  false };
    case PinLabel::BA:     return { BUS_BA_BIT,    false, false };
    // Interrupt signals (active-low, all inputs)
    case PinLabel::_IRQ:   return { BUS_IRQ_BIT,   true,  true };
    case PinLabel::_NMI:   return { BUS_NMI_BIT,   true,  true };
    case PinLabel::_RES:   return { BUS_RES_BIT,   true,  true };
    case PinLabel::_ABORT: return { BUS_ABORT_BIT, true,  true };
    // Special signals
    case PinLabel::_SO:    return { BUS_SO_BIT,    true,  true  };
    case PinLabel::_VP:    return { BUS_VP_BIT,    false, false };
    case PinLabel::_VPB:   return { BUS_VP_BIT,    false, false };
    case PinLabel::_ML:    return { BUS_ML_BIT,    false, true  };
    default:               return { -1,            false, false };
    }
}
