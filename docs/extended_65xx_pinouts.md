# 65xx CPU Family Pinouts - Extended

| Pin Acronym | Name | I/O | 6502 | 65C02 | 65C816 | 6510 | 6507 | 6503 | 6504 | 6505 | 6512 | 6513 | 6514 | 6515 | 65C02S | 65C134 | 65C265 | Description | Notes |
|-------------|------|-----|------|-------|--------|------|------|------|------|------|------|------|------|------|--------|--------|--------|-------------|-------|
| RES | Reset | I | 40 | 40 | 40 | 40 | 1 | 8 | 40 | 8 | 40 | 8 | 40 | 8 | 40 | 4 | 4 | Active-low input to initialize the processor and start program execution. | Also called RESB on some chips; edge-sensitive. |
| RDY | Ready | I | 2 | 2 | 2 | 2 | 3 | - | 2 | - | 2 | - | 2 | - | 2 | 37 | 37 | Halts the processor during read cycles when low, allowing for DMA or slow memory. | Bidirectional on 65C02 and 65C816 (can be pulled low by WAI instruction). |
| Φ0 | Phase 0 Clock In | I | 37 | 37 | 37 | 1 | 27 | 7 | 37 | 7 | 37 | 7 | 37 | 7 | 37 | 65 | 65 | External clock input that drives the processor's timing. | Also called PHI2 on 65C816 (system clock input). |
| IRQ | Interrupt Request | I | 4 | 4 | 4 | 3 | - | - | 4 | - | 4 | - | 4 | - | 4 | 6 | 6 | Active-low input for maskable interrupts; jumps to IRQ vector if enabled. | Also called IRQB on CMOS variants. |
| NMI | Non-Maskable Interrupt | I | 6 | 6 | 6 | 4 | - | - | 6 | - | 6 | - | 6 | - | 6 | 8 | 8 | Active-low edge-sensitive input for non-maskable interrupts; jumps to NMI vector. | Also called NMIB on CMOS variants. |
| SO/SOB | Set Overflow | I | 38 | 38 | - | - | - | - | 38 | - | 38 | - | 38 | - | 38 | - | - | Active-low input to set the overflow (V) flag directly. | Active low transition; used for hardware-driven overflow; called SO on 6502, SOB on 65C02. |
| AEC | Address Enable Control | I | - | - | - | 5 | - | - | - | - | - | - | - | - | - | - | - | Active-low input to tri-state address, data, and R/W buses for DMA. | Specific to 6510 for Commodore systems. |
| BE | Bus Enable | I | - | 36 | 36 | - | - | - | - | - | - | - | - | - | 36 | 36 | 36 | Active-high input; low tri-states address, data, and R/W buses. | Allows external bus control. |
| ABORT | Abort | I | - | - | 3 | - | - | - | - | - | - | - | - | - | - | - | - | Active-low input to abort current instruction without modifying registers. | 65C816-specific for handling faults. |
| A0 | Address Bit 0 | O | 9 | 9 | 9 | 7 | 5 | 5 | 9 | 5 | 9 | 5 | 9 | 5 | 9 | 9 | 9 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A1 | Address Bit 1 | O | 10 | 10 | 10 | 8 | 6 | 6 | 10 | 6 | 10 | 6 | 10 | 6 | 10 | 10 | 10 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A2 | Address Bit 2 | O | 11 | 11 | 11 | 9 | 7 | - | 11 | - | 11 | - | 11 | - | 11 | 11 | 11 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A3 | Address Bit 3 | O | 12 | 12 | 12 | 10 | 8 | - | 12 | - | 12 | - | 12 | - | 12 | 12 | 12 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A4 | Address Bit 4 | O | 13 | 13 | 13 | 11 | 9 | - | 13 | - | 13 | - | 13 | - | 13 | 13 | 13 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A5 | Address Bit 5 | O | 14 | 14 | 14 | 12 | 10 | - | 14 | - | 14 | - | 14 | - | 14 | 14 | 14 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A6 | Address Bit 6 | O | 15 | 15 | 15 | 13 | 11 | - | 15 | - | 15 | - | 15 | - | 15 | 15 | 15 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A7 | Address Bit 7 | O | 16 | 16 | 16 | 14 | 12 | - | 16 | - | 16 | - | 16 | - | 16 | 16 | 16 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A8 | Address Bit 8 | O | 17 | 17 | 17 | 15 | 13 | - | 17 | - | 17 | - | 17 | - | 17 | 17 | 17 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A9 | Address Bit 9 | O | 18 | 18 | 18 | 16 | 14 | - | 18 | - | 18 | - | 18 | - | 18 | 18 | 18 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A10 | Address Bit 10 | O | 19 | 19 | 19 | 17 | 15 | - | 19 | - | 19 | - | 19 | - | 19 | 19 | 19 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A11 | Address Bit 11 | O | 20 | 20 | 20 | 18 | 16 | - | 20 | - | 20 | - | 20 | - | 20 | 20 | 20 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins. |
| A12 | Address Bit 12 | O | 22 | 22 | 22 | 19 | 17 | - | 22 | - | 22 | - | 22 | - | 22 | 22 | 22 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins; 6507 max A12. |
| A13 | Address Bit 13 | O | 23 | 23 | 23 | 20 | - | - | 23 | - | 23 | - | 23 | - | 23 | 23 | 23 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins; not on 6507. |
| A14 | Address Bit 14 | O | 24 | 24 | 24 | 22 | - | - | 24 | - | 24 | - | 24 | - | 24 | 24 | 24 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins; not on 6507. |
| A15 | Address Bit 15 | O | 25 | 25 | 25 | 23 | - | - | 25 | - | 25 | - | 25 | - | 25 | 25 | 25 | Part of the address bus for memory and I/O addressing. | Tri-state on chips with bus control pins; not on 6507. |
| D0 | Data Bit 0 | IO | 33 | 33 | 33 | 37 | 25 | 4 | 33 | 4 | 33 | 4 | 33 | 4 | 33 | 33 | 33 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA0 on 65C816. |
| D1 | Data Bit 1 | IO | 32 | 32 | 32 | 36 | 24 | 3 | 32 | 3 | 32 | 3 | 32 | 3 | 32 | 32 | 32 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA1 on 65C816. |
| D2 | Data Bit 2 | IO | 31 | 31 | 31 | 35 | 23 | 2 | 31 | 2 | 31 | 2 | 31 | 2 | 31 | 31 | 31 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA2 on 65C816. |
| D3 | Data Bit 3 | IO | 30 | 30 | 30 | 34 | 22 | 1 | 30 | 1 | 30 | 1 | 30 | 1 | 30 | 30 | 30 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA3 on 65C816. |
| D4 | Data Bit 4 | IO | 29 | 29 | 29 | 33 | 21 | - | 29 | - | 29 | - | 29 | - | 29 | 29 | 29 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA4 on 65C816. |
| D5 | Data Bit 5 | IO | 28 | 28 | 28 | 32 | 20 | - | 28 | - | 28 | - | 28 | - | 28 | 28 | 28 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA5 on 65C816. |
| D6 | Data Bit 6 | IO | 27 | 27 | 27 | 31 | 19 | - | 27 | - | 27 | - | 27 | - | 27 | 27 | 27 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA6 on 65C816. |
| D7 | Data Bit 7 | IO | 26 | 26 | 26 | 30 | 18 | - | 26 | - | 26 | - | 26 | - | 26 | 26 | 26 | Part of the bidirectional data bus for reading/writing data. | Tri-state on chips with bus control; multiplexed with BA7 on 65C816. |
| P0 | Port Bit 0 | IO | - | - | - | 29 | - | - | - | - | - | - | - | - | - | - | - | Bidirectional I/O port bit; in C64, controls cassette motor. | 6510-specific 6-bit port at $00/$01. |
| P1 | Port Bit 1 | IO | - | - | - | 28 | - | - | - | - | - | - | - | - | - | - | - | Bidirectional I/O port bit; in C64, controls cassette sense. | 6510-specific 6-bit port at $00/$01. |
| P2 | Port Bit 2 | IO | - | - | - | 27 | - | - | - | - | - | - | - | - | - | - | - | Bidirectional I/O port bit; in C64, controls cassette write. | 6510-specific 6-bit port at $00/$01. |
| P3 | Port Bit 3 | IO | - | - | - | 26 | - | - | - | - | - | - | - | - | - | - | - | Bidirectional I/O port bit; in C64, controls CHAREN (I/O vs. Char ROM). | 6510-specific 6-bit port at $00/$01. |
| P4 | Port Bit 4 | IO | - | - | - | 25 | - | - | - | - | - | - | - | - | - | - | - | Bidirectional I/O port bit; in C64, controls HIRAM (Kernal ROM banking). | 6510-specific 6-bit port at $00/$01. |
| P5 | Port Bit 5 | IO | - | - | - | 24 | - | - | - | - | - | - | - | - | - | - | - | Bidirectional I/O port bit; in C64, controls LORAM (BASIC ROM banking). | 6510-specific 6-bit port at $00/$01. |
| Φ1 | Phase 1 Clock Out | O | 3 | 3 | - | - | - | - | 3 | - | 3 | - | 3 | - | 3 | - | - | Clock output derived from Φ0; inverted Φ2. | Also called PHI1O on 65C02. |
| SYNC | Synchronization | O | 7 | 7 | - | - | - | - | 7 | - | 7 | - | 7 | - | 7 | 7 | 7 | High during opcode fetch cycles; useful for debugging. | Not on 6510 or 6507. |
| Φ2 | Phase 2 Clock Out | O | 39 | 39 | - | 39 | 28 | - | 39 | - | 39 | - | 39 | - | 39 | 39 | 39 | Primary clock output derived from Φ0. | Also called PHI2O on 65C02. |
| R/W | Read/Write | O | 34 | 34 | 34 | 38 | 26 | - | 34 | - | 34 | - | 34 | - | 34 | 34 | 34 | High for read, low for write; controls data bus direction. | Also called RWB on CMOS variants. |
| VP | Vector Pull | O | - | 1 | 1 | - | - | - | - | - | - | - | - | - | 1 | 1 | 1 | Active-low output indicating interrupt vector fetch. | Also called VPB on some datasheets. |
| ML | Memory Lock | O | - | 5 | 5 | - | - | - | - | - | - | - | - | - | 5 | 5 | 5 | Active-low during modify/write cycles of RMW instructions. | Also called MLB on CMOS variants; for multiprocessor systems. |
| VPA | Valid Program Address | O | - | - | 7 | - | - | - | - | - | - | - | - | - | - | - | - | High when address bus holds a valid program opcode address. | 65C816-specific for cache control. |
| VDA | Valid Data Address | O | - | - | 39 | - | - | - | - | - | - | - | - | - | - | - | - | High when address bus holds a valid data address. | 65C816-specific for cache control. |
| MX | M/X Select | O | - | - | 38 | - | - | - | - | - | - | - | - | - | - | - | - | Outputs status of M (accumulator) and X (index) flags. | 65C816-specific; valid during PHI2 transitions. |
| E | Emulation | O | - | - | 35 | - | - | - | - | - | - | - | - | - | - | - | - | Outputs status of emulation mode flag. | 65C816-specific. |
| CS0 | Chip Select 0 | I | - | - | - | - | - | - | - | - | 29 | 29 | 29 | 29 | - | - | - | Active-low chip select input. | 6512-6515 series for multi-processor systems. |
| CS1 | Chip Select 1 | I | - | - | - | - | - | - | - | - | 30 | 30 | 30 | 30 | - | - | - | Active-high chip select input. | 6512-6515 series for multi-processor systems. |
| CS2 | Chip Select 2 | I | - | - | - | - | - | - | - | - | 31 | 31 | 31 | 31 | - | - | - | Active-high chip select input. | 6512-6515 series for multi-processor systems. |
| PWM0 | PWM Channel 0 | O | - | - | - | - | - | - | - | - | - | - | - | - | - | 35 | 35 | Pulse Width Modulation output channel 0. | 65C134/65C265 integrated peripherals. |
| PWM1 | PWM Channel 1 | O | - | - | - | - | - | - | - | - | - | - | - | - | - | 34 | 34 | Pulse Width Modulation output channel 1. | 65C134/65C265 integrated peripherals. |
| UART_TX | UART Transmit | O | - | - | - | - | - | - | - | - | - | - | - | - | - | 38 | 38 | UART serial data output. | 65C134/65C265 integrated UART. |
| UART_RX | UART Receive | I | - | - | - | - | - | - | - | - | - | - | - | - | - | 39 | 39 | UART serial data input. | 65C134/65C265 integrated UART. |
| SPI_CLK | SPI Clock | O | - | - | - | - | - | - | - | - | - | - | - | - | - | 40 | 40 | SPI serial clock output. | 65C134/65C265 integrated SPI interface. |
| SPI_MOSI | SPI Master Out | O | - | - | - | - | - | - | - | - | - | - | - | - | - | 41 | 41 | SPI master data output, slave data input. | 65C134/65C265 integrated SPI interface. |
| SPI_MISO | SPI Master In | I | - | - | - | - | - | - | - | - | - | - | - | - | - | 42 | 42 | SPI master data input, slave data output. | 65C134/65C265 integrated SPI interface. |
| SPI_CS | SPI Chip Select | O | - | - | - | - | - | - | - | - | - | - | - | - | - | 43 | 43 | SPI chip select output. | 65C134/65C265 integrated SPI interface. |
| VSS | Ground | P | 1,21 | 21 | 20,21 | 21 | 2 | - | 1,21 | - | 1,21 | - | 1,21 | - | 21 | 21 | 21 | Ground connection (0V). | Multiple pins on some chips for better power distribution. |
| VDD | +5V Power | P | 8 | 8 | 8 | 6 | 4 | - | 8 | - | 8 | - | 8 | - | 8 | 8 | 8 | Positive power supply (+5V). | Operating voltage. |
| NC | No Connection | - | 5,35,36 | 35 | - | - | - | - | 5,35,36 | - | 5,35,36 | - | 5,35,36 | - | 35 | - | - | Unused pins; no internal connection. | Repurposed in variants. |
| BA0 | Bank Address Bit 0 | O | - | - | 33 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D0. | 65C816-specific for 24-bit addressing. |
| BA1 | Bank Address Bit 1 | O | - | - | 32 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D1. | 65C816-specific for 24-bit addressing. |
| BA2 | Bank Address Bit 2 | O | - | - | 31 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D2. | 65C816-specific for 24-bit addressing. |
| BA3 | Bank Address Bit 3 | O | - | - | 30 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D3. | 65C816-specific for 24-bit addressing. |
| BA4 | Bank Address Bit 4 | O | - | - | 29 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D4. | 65C816-specific for 24-bit addressing. |
| BA5 | Bank Address Bit 5 | O | - | - | 28 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D5. | 65C816-specific for 24-bit addressing. |
| BA6 | Bank Address Bit 6 | O | - | - | 27 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D6. | 65C816-specific for 24-bit addressing. |
| BA7 | Bank Address Bit 7 | O | - | - | 26 | - | - | - | - | - | - | - | - | - | - | - | - | Part of the bank address bus; multiplexed with D7. | 65C816-specific for 24-bit addressing. |

## Additional Family Members Added

### 6503 (8-pin DIP)
- Minimal 8-bit microprocessor with only 4 data lines (D0-D3)
- Very limited address space (2 address lines: A0-A1)
- Used in simple control applications
- No interrupt pins

### 6504 (28-pin DIP) 
- Similar to standard 6502 but in smaller package
- Full data bus but limited features
- Cost-reduced version for embedded applications

### 6505 (8-pin DIP)
- Another minimal variant similar to 6503
- Extremely limited I/O capability
- Used in very simple control applications

### 6512-6515 Series (40-pin DIP)
- Multi-processor variants with chip select inputs (CS0, CS1, CS2)
- Designed for systems with multiple CPUs sharing a bus
- 6512: Basic multi-processor support
- 6513: Enhanced multi-processor features  
- 6514: Additional multi-processor capabilities
- 6515: Full-featured multi-processor variant

### 65C02S (40-pin DIP)
- Static CMOS version of 65C02
- Identical pinout to 65C02
- Can be fully stopped (0 Hz clock)
- Lower power consumption

### 65C134 (68-pin PLCC)
- Microcontroller with integrated peripherals
- Includes PWM channels, UART, SPI interface
- Enhanced I/O capabilities for embedded applications
- Modern package format

### 65C265 (68-pin PLCC)  
- Advanced microcontroller variant
- Similar peripherals to 65C134
- Enhanced feature set for complex embedded systems
- Larger program/data memory addressing

## Legend
- **I**: Input
- **O**: Output  
- **IO**: Input/Output (bidirectional)
- **P**: Power
- **-**: None or No Connection
- Pin numbers are listed as comma-separated values if a function uses multiple pins. A dash (-) indicates the pin is not present on that chip.

## Primary Sources
- MOS 6502 Datasheet: https://www.princeton.edu/~mae412/HANDOUTS/Datasheets/6502.pdf
- MOS 6510 Datasheet: http://archive.6502.org/datasheets/mos_6510_mpu.pdf
- WDC 65C02 Datasheet: https://www.westerndesigncenter.com/wdc/documentation/w65c02s.pdf
- MOS 6500 Family (including 6507) Datasheet: http://archive.6502.org/datasheets/mos_6500_mpu_nov_1985.pdf
- WDC 65C816 Datasheet: https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf