// This file contains the GUI rendering functions for the FAM65XX CPU family
// Moved from previous cimgui_interface.c for better organization

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Include the fam65xx core definitions
// Since fam65xx_core.hpp is C++, we need to define the essential structures here
// or create a C-compatible header. For now, we'll define the essential types.

// Forward declarations and essential definitions for fam65xx_t
struct fam65xx_t;
typedef struct fam65xx_t fam65xx_t;

// Include GUI interface
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>

// Essential CPU flag definitions
#define FLAG_C  0x01  // Carry
#define FLAG_Z  0x02  // Zero
#define FLAG_I  0x04  // Interrupt Disable
#define FLAG_D  0x08  // Decimal Mode
#define FLAG_B  0x10  // Break
#define FLAG_U  0x20  // Unused (always 1)
#define FLAG_V  0x40  // Overflow
#define FLAG_N  0x80  // Negative

// BRK flags for interrupt handling
#define FAM65XX_BRK_IRQ     (1<<0)
#define FAM65XX_BRK_NMI     (1<<1)
#define FAM65XX_BRK_RESET   (1<<2)

// Minimal fam65xx_t structure definition for GUI access
// This is a simplified version for GUI display purposes only
struct fam65xx_t {
    union {
        uint8_t reg8[16];
        uint16_t reg16[8];
    };
    
    struct {
        uint16_t am_index           : 4;
        uint16_t illegal_store      : 1;
        uint16_t _reserved          : 2;
        uint16_t can_skip_page_cross : 1;
        uint16_t rmw                : 1;
        uint16_t op_index           : 7;
    } opcode_entry;
    
    uint8_t cycle_index;
    void* current_handler;
    
    void* mem_read;
    void* mem_write;
    void* mem_user_data;
    
    uint32_t interrupt_shift_register;
    uint8_t brk_flags;
    uint8_t nmi_prev;
};

// Register access macros - simplified for GUI
#define CPU_A(cpu)     ((cpu)->reg8[8])
#define CPU_X(cpu)     ((cpu)->reg8[9])
#define CPU_Y(cpu)     ((cpu)->reg8[10])
#define CPU_P(cpu)     ((cpu)->reg8[11])
#define CPU_IR(cpu)    ((cpu)->reg8[12])
#define CPU_DL(cpu)    ((cpu)->reg8[13])
#define CPU_S(cpu)     ((cpu)->reg8[2])
#define CPU_PC(cpu)    ((cpu)->reg16[3])
#define CPU_AB(cpu)    ((cpu)->reg16[2])

// Flag names for processor status register
static const char* flag_names[] = {
    "C", "Z", "I", "D", "B", "U", "V", "N"
};

// Processor variant names
static const char* get_processor_name(fam65xx_t* cpu) {
    if (!cpu) return "Unknown CPU";
    
    // Since we can't easily determine the processor type from the CPU state alone,
    // we'll provide a generic name. In a real implementation, this could be
    // determined by checking which variant was instantiated or by examining
    // the opcode table characteristics.
    return "MOS 65xx Family CPU";
}

// Helper function to get addressing mode name
static const char* get_addressing_mode_name(uint8_t am_index) {
    static const char* am_names[] = {
        "Implicit",     // 0
        "Accumulator",  // 1
        "Immediate",    // 2
        "Zero Page",    // 3
        "Zero Page,X",  // 4
        "Zero Page,Y",  // 5
        "Absolute",     // 6
        "Absolute,X",   // 7
        "Absolute,Y",   // 8
        "Indirect",     // 9
        "Indexed Indirect", // 10 (zp,X)
        "Indirect Indexed", // 11 (zp),Y
        "Relative",     // 12
        "ZP Indirect",  // 13 (zp) - 65C02
        "Absolute Indexed Indirect", // 14 (abs,X) - 65C02/65C816
        "Stack Relative", // 15 - 65C816
    };
    
    if (am_index < sizeof(am_names) / sizeof(am_names[0])) {
        return am_names[am_index];
    }
    return "Unknown";
}

// Helper function to format processor flags
static void format_processor_flags(uint8_t flags, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%c%c%c%c%c%c%c%c",
             (flags & FLAG_N) ? 'N' : 'n',
             (flags & FLAG_V) ? 'V' : 'v',
             (flags & FLAG_U) ? 'U' : 'u',
             (flags & FLAG_B) ? 'B' : 'b',
             (flags & FLAG_D) ? 'D' : 'd',
             (flags & FLAG_I) ? 'I' : 'i',
             (flags & FLAG_Z) ? 'Z' : 'z',
             (flags & FLAG_C) ? 'C' : 'c');
}

// ============================================================================
// FAM65XX GUI DEBUG WINDOW
// ============================================================================

void fam65xx_render_debug_window(void* chip, bool* show_window) {
    fam65xx_t* cpu = (fam65xx_t*)chip;
    if (!cpu) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", get_processor_name(cpu));
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("%s", get_processor_name(cpu));
    igText("MOS Technology 65xx Family Microprocessor");
    igSeparator();
    
    // CPU Registers Section
    if (igCollapsingHeader_BoolPtr("CPU Registers", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Main registers
        igText("Accumulator (A):     $%02X (%d)", CPU_A(cpu), CPU_A(cpu));
        igText("X Index (X):         $%02X (%d)", CPU_X(cpu), CPU_X(cpu));
        igText("Y Index (Y):         $%02X (%d)", CPU_Y(cpu), CPU_Y(cpu));
        igText("Stack Pointer (S):   $%02X (Stack: $01%02X)", CPU_S(cpu), CPU_S(cpu));
        igText("Program Counter:     $%04X (%d)", CPU_PC(cpu), CPU_PC(cpu));
        
        igSeparator();
        
        // Processor status with detailed breakdown
        char flag_buffer[16];
        format_processor_flags(CPU_P(cpu), flag_buffer, sizeof(flag_buffer));
        igText("Processor Status (P): $%02X (%s)", CPU_P(cpu), flag_buffer);
        
        igIndent(16.0f);
        {
            int i;
            for (i = 0; i < 8; i++) {
                bool flag_set = (CPU_P(cpu) & (1 << i)) != 0;
                igText("  %s: %s", flag_names[i], flag_set ? "Set" : "Clear");
            }
        }
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Internal State Section
    if (igCollapsingHeader_BoolPtr("Internal State", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        igText("Instruction Register: $%02X", CPU_IR(cpu));
        igText("Data Latch:          $%02X", CPU_DL(cpu));
        igText("Address Bus:         $%04X", CPU_AB(cpu));
        igText("Cycle Index:         %d", cpu->cycle_index);
        
        igSeparator();
        
        // Current opcode information
        igText("Current Opcode Info:");
        igIndent(16.0f);
        igText("Addressing Mode:     %s (Index: %d)", 
               get_addressing_mode_name(cpu->opcode_entry.am_index), 
               cpu->opcode_entry.am_index);
        igText("Operation Index:     %d", cpu->opcode_entry.op_index);
        igText("RMW Operation:       %s", cpu->opcode_entry.rmw ? "Yes" : "No");
        igText("Can Skip Page Cross: %s", cpu->opcode_entry.can_skip_page_cross ? "Yes" : "No");
        igText("Illegal Store:       %s", cpu->opcode_entry.illegal_store ? "Yes" : "No");
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Interrupt State Section
    if (igCollapsingHeader_BoolPtr("Interrupt State", NULL, 0)) {
        igIndent(16.0f);
        
        igText("Interrupt Shift Register: $%08X", cpu->interrupt_shift_register);
        igText("BRK Flags:               $%02X", cpu->brk_flags);
        igText("Previous NMI State:      $%02X", cpu->nmi_prev);
        
        igSeparator();
        
        igText("Interrupt Status:");
        igIndent(16.0f);
        igText("IRQ Pending:    %s", (cpu->brk_flags & FAM65XX_BRK_IRQ) ? "Yes" : "No");
        igText("NMI Pending:    %s", (cpu->brk_flags & FAM65XX_BRK_NMI) ? "Yes" : "No");
        igText("Reset Pending:  %s", (cpu->brk_flags & FAM65XX_BRK_RESET) ? "Yes" : "No");
        igText("IRQ Disabled:   %s", (CPU_P(cpu) & FLAG_I) ? "Yes" : "No");
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Processor Features (if we can determine the variant)
    if (igCollapsingHeader_BoolPtr("Processor Features", NULL, 0)) {
        igIndent(16.0f);
        
        igText("This section would show processor-specific features");
        igText("such as illegal opcodes, CMOS enhancements, etc.");
        igText("Feature detection requires template system integration.");
        
        igSeparator();
        
        igText("Common 65xx Features:");
        igText("• 8-bit accumulator and index registers");
        igText("• 16-bit program counter");
        igText("• 256-byte stack (page 1)");
        igText("• 13+ addressing modes");
        igText("• Hardware interrupt support (IRQ, NMI, RESET)");
        igText("• Decimal mode arithmetic (BCD)");
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Memory Interface Section
    if (igCollapsingHeader_BoolPtr("Memory Interface", NULL, 0)) {
        igIndent(16.0f);
        
        igText("Memory Callbacks:");
        igText("Read Function:  %p", (void*)cpu->mem_read);
        igText("Write Function: %p", (void*)cpu->mem_write);
        igText("User Data:      %p", cpu->mem_user_data);
        
        igSeparator();
        
        igText("Bus Interface:");
        igText("• 16-bit address bus ($0000-$FFFF)");
        igText("• 8-bit data bus");
        igText("• R/W̅, SYNC, RDY control lines");
        igText("• IRQ̅, NMI̅, RES̅ interrupt lines");
        
        igUnindent(16.0f);
    }

    igEnd();
}

// ============================================================================
// FAM65XX GUI SETTINGS WINDOW
// ============================================================================

void fam65xx_render_settings_window(void* chip, bool* show_window) {
    fam65xx_t* cpu = (fam65xx_t*)chip;
    if (!cpu) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", get_processor_name(cpu));
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("%s Configuration", get_processor_name(cpu));
    igSeparator();
    
    igText("Processor Family: MOS Technology 65xx");
    igText("Architecture: 8-bit microprocessor");
    igText("Address Space: 64KB (16-bit addressing)");
    igText("Data Width: 8 bits");
    
    igSeparator();
    
    // Pin Configuration
    if (igCollapsingHeader_BoolPtr("Pin Configuration", NULL, 0)) {
        igIndent(16.0f);
        igText("MOS 65xx DIP-40 Package (40 pins):");
        igSeparator();
        
        igText("Power and Clock:");
        igText("  VCC (8) - +5V Power Supply");
        igText("  VSS (21) - Ground (0V)");
        igText("  φ0 (3) - Phase 0 Clock Input");
        igText("  φ1 (37) - Phase 1 Clock Output");
        igText("  φ2 (39) - Phase 2 Clock Output");
        
        igSeparator();
        
        igText("Address Bus (16 lines):");
        igText("  A0-A15 (9-20, 22-25) - Address Lines");
        
        igSeparator();
        
        igText("Data Bus (8 lines):");
        igText("  D0-D7 (26, 28-33) - Data Lines");
        
        igSeparator();
        
        igText("Control Lines:");
        igText("  R/W̅ (34) - Read/Write");
        igText("  SYNC (7) - Synchronize");
        igText("  RDY (2) - Ready");
        
        igSeparator();
        
        igText("Interrupt Lines:");
        igText("  IRQ̅ (4) - Interrupt Request");
        igText("  NMI̅ (6) - Non-Maskable Interrupt");
        igText("  RES̅ (40) - Reset");
        
        igSeparator();
        
        igText("Special:");
        igText("  SO̅ (38) - Set Overflow");
        igText("  BE (36) - Bus Enable");
        igText("  ML̅ (35) - Memory Lock");
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Processor Variants
    if (igCollapsingHeader_BoolPtr("Processor Variants", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        igText("Supported 65xx Family Members:");
        igSeparator();
        
        igText("• MOS 6502 (1975)");
        igIndent(16.0f);
        igText("  - Original NMOS design");
        igText("  - Illegal opcodes present");
        igText("  - NMOS-specific bugs");
        igText("  - Decimal mode affects N/Z flags");
        igUnindent(16.0f);
        
        igText("• MOS 6510 (C64)");
        igIndent(16.0f);
        igText("  - Based on 6502 with I/O port");
        igText("  - Memory-mapped I/O at $00/$01");
        igText("  - Bank switching capabilities");
        igUnindent(16.0f);
        
        igText("• WDC 65C02 (CMOS)");
        igIndent(16.0f);
        igText("  - CMOS redesign of 6502");
        igText("  - Additional instructions (STP, WAI, etc.)");
        igText("  - Fixed NMOS bugs");
        igText("  - New addressing modes");
        igUnindent(16.0f);
        
        igText("• Rockwell R65C02");
        igIndent(16.0f);
        igText("  - CMOS with bit manipulation");
        igText("  - RMB/SMB/BBR/BBS instructions");
        igUnindent(16.0f);
        
        igText("• WDC 65C816 (16-bit)");
        igIndent(16.0f);
        igText("  - 16-bit extension of 65C02");
        igText("  - 24-bit addressing");
        igText("  - Native and emulation modes");
        igUnindent(16.0f);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // CPU Controls
    if (igCollapsingHeader_BoolPtr("CPU Controls", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        if (igButton("Reset CPU", (ImVec2){0, 0})) {
            // Reset would require bus state - this is just UI
            // igText("Reset requested (requires bus state)");
        }
        
        igSameLine(0, -1);
        
        if (igButton("Trigger NMI", (ImVec2){0, 0})) {
            // NMI trigger would require pin manipulation
            // igText("NMI requested (requires pin control)");
        }
        
        igSameLine(0, -1);
        
        if (igButton("Trigger IRQ", (ImVec2){0, 0})) {
            // IRQ trigger would require pin manipulation
            // igText("IRQ requested (requires pin control)");
        }
        
        igSeparator();
        
        {
            static bool step_mode = false;
            igCheckbox("Single Step Mode", &step_mode);
            
            static bool trace_mode = false;
            igCheckbox("Instruction Trace", &trace_mode);
            
            static bool break_on_brk = true;
            igCheckbox("Break on BRK instruction", &break_on_brk);
        }
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Performance Settings
    if (igCollapsingHeader_BoolPtr("Performance Settings", NULL, 0)) {
        igIndent(16.0f);
        
        {
            static float clock_speed = 1.0f;
            igSliderFloat("Clock Speed Multiplier", &clock_speed, 0.1f, 10.0f, "%.1fx", 0);
            
            static bool accurate_timing = true;
            igCheckbox("Cycle-Accurate Timing", &accurate_timing);
            
            static bool illegal_opcodes = true;
            igCheckbox("Enable Illegal Opcodes", &illegal_opcodes);
        }
        
        igText("Note: Some settings require CPU restart to take effect");
        
        igUnindent(16.0f);
    }

    igEnd();
}