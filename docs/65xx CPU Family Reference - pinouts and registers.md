# 65xx CPU Family Reference

## Part 1: Pinout Reference

### Core Processors (6502, 65C02, 6510, 6507)

| Pin | Name | I/O | 6502 | 65C02 | 6510 | 6507 | Description |
|-----|------|-----|------|-------|------|------|-------------|
| VSS | Ground | P | 1,21 | 21 | 21 | 2 | Ground (0V) |
| RDY | Ready | I | 2 | 2 | 2 | 3 | Halt during read cycles |
| Φ1 | Clock Out 1 | O | 3 | 3 | - | - | Inverted Φ2 |
| IRQ | Int Request | I | 4 | 4 | 3 | - | Maskable interrupt |
| AEC | Addr Enable | I | - | - | 5 | - | Tri-state buses (6510) |
| NMI | Non-Mask Int | I | 6 | 6 | 4 | - | Non-maskable interrupt |
| SYNC | Sync | O | 7 | 7 | - | - | Opcode fetch indicator |
| VDD | +5V Power | P | 8 | 8 | 6 | 4 | +5V supply |
| A0-A15 | Address Bus | O | 9-25 | 9-25 | 7-23 | 5-17 | Memory addressing |
| D0-D7 | Data Bus | IO | 26-33 | 26-33 | 30-37 | 18-25 | Data transfer |
| P0-P5 | I/O Port | IO | - | - | 24-29 | - | 6-bit port (6510 only) |
| R/W | Read/Write | O | 34 | 34 | 38 | 26 | Bus direction control |
| BE | Bus Enable | I | - | 36 | - | - | Tri-state control (65C02) |
| Φ0 | Clock In | I | 37 | 37 | 1 | 27 | External clock input |
| SO | Set Overflow | I | 38 | 38 | - | - | Set V flag |
| Φ2 | Clock Out 2 | O | 39 | 39 | 39 | 28 | Primary clock out |
| RES | Reset | I | 40 | 40 | 40 | 1 | Active-low reset input |
| VP | Vector Pull | O | - | 1 | - | - | Interrupt vector fetch |
| ML | Memory Lock | O | - | 5 | - | - | RMW instruction cycle |

### 65C816 (16-bit Extended Processor)

| Pin | Name | I/O | Pin# | Description |
|-----|------|-----|------|-------------|
| VP | Vector Pull | O | 1 | Vector fetch |
| RDY | Ready | I/O | 2 | Halt/WAI control |
| ABORT | Abort | I | 3 | Abort instruction |
| IRQ | Int Request | I | 4 | Maskable interrupt |
| ML | Memory Lock | O | 5 | RMW cycle |
| NMI | Non-Mask Int | I | 6 | Non-maskable interrupt |
| VPA | Valid Prog Addr | O | 7 | Program address valid |
| VDD | +5V Power | P | 8 | +5V supply |
| A0-A15 | Address Bus | O | 9-25 | 16-bit address |
| D0-D7/BA0-BA7 | Data/Bank Bus | IO | 26-33 | Muxed data/bank address |
| R/W | Read/Write | O | 34 | Bus direction |
| E | Emulation | O | 35 | Emulation mode flag |
| BE | Bus Enable | I | 36 | Tri-state control |
| Φ0 | Clock In | I | 37 | System clock input |
| MX | M/X Status | O | 38 | Register size status |
| VDA | Valid Data Addr | O | 39 | Data address valid |
| RES | Reset | I | 40 | Active-low reset |
| VSS | Ground | P | 20,21 | Ground |

### Minimal Variants (6503, 6504, 6505)

| Pin | Name | I/O | 6503 | 6504 | 6505 | Notes |
|-----|------|-----|------|------|------|-------|
| D0-D3 | Data | IO | 1-4 | - | 1-4 | 4-bit data only |
| A0-A1 | Address | O | 5-6 | - | 5-6 | Limited addressing |
| Φ0 | Clock In | I | 7 | 37 | 7 | Clock input |
| RES | Reset | I | 8 | 40 | 8 | Reset input |
| Φ1 | Clock Out 1 | O | - | 3 | - | 6504 only |
| IRQ | Int Request | I | - | 4 | - | Not on 6503/6505 |
| NMI | Non-Mask Int | I | - | 6 | - | Not on 6503/6505 |
| SYNC | Sync | O | - | 7 | - | 6504 only |
| A0-A15 | Address | O | - | 9-25 | - | Full address (6504) |
| D0-D7 | Data | IO | - | 26-33 | - | Full data (6504) |
| R/W | Read/Write | O | - | 34 | - | 6504 only |
| SO | Set Overflow | I | - | 38 | - | Not on 6503/6505 |
| Φ2 | Clock Out 2 | O | - | 39 | - | 6504 only |

### Multi-Processor Variants (6512-6515)

| Pin | Name | I/O | 6512 | 6513 | 6514 | 6515 | Description |
|-----|------|-----|------|------|------|------|-------------|
| CS0 | Chip Select 0 | I | 29 | 29 | 29 | 29 | Active-low select |
| CS1 | Chip Select 1 | I | 30 | 30 | 30 | 30 | Active-high select |
| CS2 | Chip Select 2 | I | 31 | 31 | 31 | 31 | Active-high select |

*Note: 6512-6515 are identical to 6502 except for chip select pins*

### Microcontroller Variants (65C134, 65C265 - 68-pin PLCC)

| Pin | Name | I/O | Pin# | Description |
|-----|------|-----|------|-------------|
| PWM1 | PWM Ch 1 | O | 34 | PWM output 1 |
| PWM0 | PWM Ch 0 | O | 35 | PWM output 0 |
| UART_TX | UART Transmit | O | 38 | Serial TX |
| UART_RX | UART Receive | I | 39 | Serial RX |
| SPI_CLK | SPI Clock | O | 40 | SPI clock |
| SPI_MOSI | SPI Master Out | O | 41 | SPI data out |
| SPI_MISO | SPI Master In | I | 42 | SPI data in |
| SPI_CS | SPI Chip Sel | O | 43 | SPI select |

*Note: These also include all standard 65C02 pins plus integrated peripherals*

---

## Part 2: User-Accessible Registers

### Standard 6502 Family Registers

| Reg | Name | Width | Description |
|-----|------|-------|-------------|
| **A** | Accumulator | 8-bit | Arithmetic/logic operations |
| **X** | X Index | 8-bit | Index register for addressing |
| **Y** | Y Index | 8-bit | Index register for addressing |
| **S** | Stack Pointer | 8-bit | Stack position (page $01) |
| **PC** | Program Counter | 16-bit | Next instruction address |
| **P** | Status Flags | 8-bit | Processor status (NV-BDIZC) |

*Applies to: 6502, 65C02, 6510, 6507, 6503, 6504, 6505, 6512-6515, 65C02S, 65C134, 65C265*

### 65C816 Registers (Emulation Mode - E=1)

| Reg | Name | Width | Description |
|-----|------|-------|-------------|
| **A** | Accumulator | 8-bit | Compatible with 6502 |
| **X** | X Index | 8-bit | Compatible with 6502 |
| **Y** | Y Index | 8-bit | Compatible with 6502 |
| **S** | Stack Pointer | 8-bit | Stack in page $01 only |
| **PC** | Program Counter | 16-bit | 16-bit addressing |
| **P** | Status Flags | 8-bit | NV-BDIZC flags |
| **PBR** | Program Bank | 8-bit | Implicit $00 |
| **DBR** | Data Bank | 8-bit | Implicit $00 |
| **D** | Direct Page | 16-bit | Fixed at $0000 |

### 65C816 Registers (Native Mode - E=0)

| Reg | Name | Width | Description |
|-----|------|-------|-------------|
| **C** | Accumulator | 8/16-bit | Controlled by M flag |
| **X** | X Index | 8/16-bit | Controlled by X flag |
| **Y** | Y Index | 8/16-bit | Controlled by X flag |
| **S** | Stack Pointer | 16-bit | Anywhere in bank $00 |
| **PC** | Program Counter | 16-bit | 16-bit program offset |
| **P** | Status Flags | 8-bit | NVMXDIZC flags |
| **PBR** | Program Bank | 8-bit | Bank for code execution |
| **DBR** | Data Bank | 8-bit | Default bank for data |
| **D** | Direct Page | 16-bit | Relocatable zero page |

### Status Register (P) Flags

| Bit | Flag | 6502 Family | 65C816 Emu | 65C816 Native | Description |
|-----|------|-------------|------------|---------------|-------------|
| 7 | **N** | ✓ | ✓ | ✓ | Negative result |
| 6 | **V** | ✓ | ✓ | ✓ | Signed overflow |
| 5 | **-** | ✓ (1) | ✓ (1) | **M** | Unused / Accumulator size |
| 4 | **B** | ✓ | ✓ | **X** | Break flag / Index size |
| 3 | **D** | ✓ | ✓ | ✓ | Decimal mode |
| 2 | **I** | ✓ | ✓ | ✓ | IRQ disable |
| 1 | **Z** | ✓ | ✓ | ✓ | Zero result |
| 0 | **C** | ✓ | ✓ | ✓ | Carry/borrow |

**65C816 Native Mode Flags:**
- **M flag (bit 5)**: 0 = 16-bit accumulator, 1 = 8-bit accumulator
- **X flag (bit 4)**: 0 = 16-bit indexes, 1 = 8-bit indexes

### 6510 Special Registers (Memory-Mapped)

| Address | Register | Width | Description |
|---------|----------|-------|-------------|
| $00 | **DDR** | 6-bit | Data direction (0=in, 1=out) |
| $01 | **DATA** | 6-bit | I/O port data |

**6510 Port Bits (Commodore 64):**
- P0: Cassette motor control
- P1: Cassette sense
- P2: Cassette write
- P3: CHAREN (I/O vs Char ROM)
- P4: HIRAM (Kernal ROM banking)
- P5: LORAM (BASIC ROM banking)

### 65C816 Register Control Instructions

| Instruction | Effect |
|-------------|--------|
| **XCE** | Exchange Carry with Emulation flag (mode switch) |
| **REP #$nn** | Reset status bits (e.g., REP #$30 = 16-bit A,X,Y) |
| **SEP #$nn** | Set status bits (e.g., SEP #$30 = 8-bit A,X,Y) |

---

## Processor Family Characteristics

| Processor | Package | Address Lines | Data Lines | Stack | Special Features |
|-----------|---------|---------------|------------|-------|-----------------|
| 6502 | 40-pin DIP | 16 (A0-A15) | 8 (D0-D7) | Page $01 | Original design |
| 65C02 | 40-pin DIP | 16 (A0-A15) | 8 (D0-D7) | Page $01 | CMOS, new opcodes |
| 65C816 | 40-pin DIP | 24 (bank+16) | 8 (D0-D7) | 16-bit ptr | 16-bit, 24-bit addr |
| 6510 | 40-pin DIP | 16 (A0-A15) | 8 (D0-D7) | Page $01 | 6-bit I/O port |
| 6507 | 28-pin DIP | 13 (A0-A12) | 8 (D0-D7) | Page $01 | No interrupts |
| 6503 | 8-pin DIP | 2 (A0-A1) | 4 (D0-D3) | Page $01 | Minimal control |
| 6504 | 28-pin DIP | 16 (A0-A15) | 8 (D0-D7) | Page $01 | Cost-reduced |
| 6505 | 8-pin DIP | 2 (A0-A1) | 4 (D0-D3) | Page $01 | Minimal control |
| 6512-6515 | 40-pin DIP | 16 (A0-A15) | 8 (D0-D7) | Page $01 | Chip select pins |
| 65C02S | 40-pin DIP | 16 (A0-A15) | 8 (D0-D7) | Page $01 | Static CMOS |
| 65C134 | 68-pin PLCC | 16 (A0-A15) | 8 (D0-D7) | Page $01 | Microcontroller |
| 65C265 | 68-pin PLCC | 16 (A0-A15) | 8 (D0-D7) | Page $01 | Microcontroller |

## Legend
- **I**: Input | **O**: Output | **IO**: Bidirectional | **P**: Power
- **✓**: Feature present | **-**: Not present
- **DIP**: Dual In-line Package | **PLCC**: Plastic Leaded Chip Carrier

## Primary Sources
- MOS 6502 Datasheet: https://www.princeton.edu/~mae412/HANDOUTS/Datasheets/6502.pdf
- MOS 6510 Datasheet: http://archive.6502.org/datasheets/mos_6510_mpu.pdf
- WDC 65C02 Datasheet: https://www.westerndesigncenter.com/wdc/documentation/w65c02s.pdf
- MOS 6500 Family Datasheet: http://archive.6502.org/datasheets/mos_6500_mpu_nov_1985.pdf
- WDC 65C816 Datasheet: https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf