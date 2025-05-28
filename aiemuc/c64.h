#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>

// Compiler optimization hints
#ifdef _MSC_VER
#define likely(x)   (x)
#define unlikely(x) (x)
#else
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// Chip select bit definitions
#define RAM_CS      (1 << 0)
#define ROM_CS      (1 << 1) 
#define CHAR_ROM_CS (1 << 2)
#define IO_CS       (1 << 3)
#define VIC_CS      (1 << 4)
#define SID_CS      (1 << 5)
#define CIA1_CS     (1 << 6)
#define CIA2_CS     (1 << 7)

// Bus control line definitions  
#define IRQ_LINE    (1 << 0)
#define NMI_LINE    (1 << 1)
#define BA_LINE     (1 << 2)
#define AEC_LINE    (1 << 3)
#define RDY_LINE    (1 << 4)

// Main emulator functions
void c64_init(void);
void c64_emulate_frame(void);

#endif // C64_H