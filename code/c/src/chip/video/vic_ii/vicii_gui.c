#include "vicii_common.h"
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// COMMON VIC-II GUI RENDERING FUNCTIONS
// ============================================================================

static const char* get_vicii_type_name(vicii_t* vicii) {
    if (vicii && vicii->desc && vicii->desc->description) {
        return vicii->desc->description;
    }
    return "Unknown VIC-II";
}

static const char* get_video_standard(vicii_t* vicii) {
    if (vicii->timing.cycles_per_line == 65 && vicii->timing.total_lines == 262) {
        return "NTSC 60Hz";
    } else if (vicii->timing.cycles_per_line == 63 && vicii->timing.total_lines == 312) {
        return "PAL 50Hz";
    }
    return "Unknown";
}

static const char* get_screen_mode(uint8_t cr1, uint8_t cr2) {
    bool ecm = (cr1 & 0x40) != 0;  // Extended Color Mode
    bool bmm = (cr1 & 0x20) != 0;  // Bitmap Mode
    bool mcm = (cr2 & 0x10) != 0;  // Multicolor Mode
    
    if (!ecm && !bmm && !mcm) return "Standard Text";
    if (!ecm && !bmm && mcm)  return "Multicolor Text";
    if (!ecm && bmm && !mcm)  return "Standard Bitmap";
    if (!ecm && bmm && mcm)   return "Multicolor Bitmap";
    if (ecm && !bmm && !mcm)  return "Extended Color Text";
    return "Invalid Mode";
}

// Hardware-accurate VIC-II chip layout (MOS 6567/6569 - 40-pin DIP)
static void render_vicii_chip_layout(vicii_t* vicii) {
    if (igCollapsingHeader_TreeNodeFlags("Hardware Layout - MOS 6567/6569 VIC-II", ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        igText("Package: 40-pin DIP");
        igText("Video Interface Device (VIC-II)");
        igSeparator();
        
        // Left side pins (1-20)
        igColumns(2, "vic_pinout", true);
        igText("LEFT SIDE:");
        igText("1  - VDD (+5V)");
        igText("2  - PHI0 (Clock)");
        igText("3  - AEC (Address Enable)");
        igText("4  - BA (Bus Available)");
        igText("5  - R/W (Read/Write)");
        igText("6  - IRQ (Interrupt)");
        igText("7  - A6 (Address)");
        igText("8  - A7 (Address)");
        igText("9  - A8 (Address)");
        igText("10 - A9 (Address)");
        igText("11 - A10 (Address)");
        igText("12 - A11 (Address)");
        igText("13 - A12 (Address)");
        igText("14 - A13 (Address)");
        igText("15 - CAS (Column Addr Strobe)");
        igText("16 - RAS (Row Addr Strobe)");
        igText("17 - LUMA (Luminance)");
        igText("18 - CHROMA (Chrominance)");
        igText("19 - SYNC (Composite Sync)");
        igText("20 - VSS (Ground)");
        
        igNextColumn();
        igText("RIGHT SIDE:");
        igText("21 - VSS (Ground)");
        igText("22 - A5 (Address)");
        igText("23 - A4 (Address)");
        igText("24 - A3 (Address)");
        igText("25 - A2 (Address)");
        igText("26 - A1 (Address)");
        igText("27 - A0 (Address)");
        igText("28 - D7 (Data)");
        igText("29 - D6 (Data)");
        igText("30 - D5 (Data)");
        igText("31 - D4 (Data)");
        igText("32 - D3 (Data)");
        igText("33 - D2 (Data)");
        igText("34 - D1 (Data)");
        igText("35 - D0 (Data)");
        igText("36 - PHI2 (Clock)");
        igText("37 - COLOR (Color Signal)");
        igText("38 - CS (Chip Select)");
        igText("39 - SOUND (Audio Out)");
        igText("40 - VCC (+5V)");
        
        igColumns(1, NULL, false);
        igUnindent(16.0f);
    }
}

void vicii_gui_render_debug_window(void* chip, bool* show_window, const char* window_title) {
    if (!*show_window) return;
    
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) {
        *show_window = false;
        return;
    }
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    // Create two-column layout: chip visualization on left, debugging info on right
    igColumns(2, "vicii_debug_columns", true);
    
    // Left column: Hardware chip layout
    render_vicii_chip_layout(vicii);
    
    igNextColumn();
    
    // Right column: Register information
    igText("%s", get_vicii_type_name(vicii));
    igSeparator();
    
    // Basic chip information
    if (igCollapsingHeader_TreeNodeFlags("Chip Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        igText("Video Standard: %s", get_video_standard(vicii));
        igText("Cycles per Line: %d", vicii->timing.cycles_per_line);
        igText("Total Lines: %d", vicii->timing.total_lines);
        igText("Current Bank: %d", vicii->memory.bank);
    }
    
    // Raster information
    if (igCollapsingHeader_TreeNodeFlags("Raster Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        igText("Raster Line: %d", vicii->timing.raster_counter);
        igText("Raster Cycle: %d", vicii->timing.x_cycle);
        igText("Badline Condition: %s", vicii->video_logic.is_bad_line ? "YES" : "NO");
        igText("X Coordinate: %d", vicii->timing.x_coordinate);
        
        // Progress bar for raster position
        float raster_progress = (float)vicii->timing.raster_counter / (float)vicii->timing.total_lines;
        igProgressBar(raster_progress, (ImVec2){-1, 0}, NULL);
        igText("Raster Progress: %.1f%%", raster_progress * 100.0f);
    }
    
    // Control registers
    if (igCollapsingHeader_TreeNodeFlags("Control Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint8_t cr1 = vicii->registers.data[0x11];
        uint8_t cr2 = vicii->registers.data[0x16];
        uint8_t memory_setup = vicii->registers.data[0x18];
        
        igText("Control Register 1 ($D011): $%02X", cr1);
        igIndent(20.0f);
        igText("RST8: %d  ECM: %d  BMM: %d  DEN: %d  RSEL: %d", 
               (cr1 >> 7) & 1, (cr1 >> 6) & 1, (cr1 >> 5) & 1, 
               (cr1 >> 4) & 1, (cr1 >> 3) & 1);
        igText("YSCROLL: %d", cr1 & 0x07);
        igUnindent(20.0f);
        
        igText("Control Register 2 ($D016): $%02X", cr2);
        igIndent(20.0f);
        igText("RES: %d  MCM: %d  CSEL: %d  XSCROLL: %d", 
               (cr2 >> 5) & 1, (cr2 >> 4) & 1, (cr2 >> 3) & 1, cr2 & 0x07);
        igUnindent(20.0f);
        
        igText("Memory Setup ($D018): $%02X", memory_setup);
        igIndent(20.0f);
        igText("VM: %d  CB: %d", (memory_setup >> 4) & 0x0F, (memory_setup >> 1) & 0x07);
        igText("Video Matrix Base: $%04X", ((memory_setup >> 4) & 0x0F) * 0x400);
        igText("Character Base: $%04X", ((memory_setup >> 1) & 0x07) * 0x800);
        igUnindent(20.0f);
        
        igText("Current Screen Mode: %s", get_screen_mode(cr1, cr2));
    }
    
    // Display position and scrolling
    if (igCollapsingHeader_TreeNodeFlags("Display Position", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint8_t cr1 = vicii->registers.data[0x11];
        uint8_t cr2 = vicii->registers.data[0x16];
        
        igText("Horizontal Scroll: %d", cr2 & 0x07);
        igText("Vertical Scroll: %d", cr1 & 0x07);
        igText("Display Enable: %s", (cr1 & 0x10) ? "ON" : "OFF");
        igText("25 Row Mode: %s", (cr1 & 0x08) ? "YES" : "NO (24 rows)");
        igText("40 Column Mode: %s", (cr2 & 0x08) ? "YES" : "NO (38 columns)");
    }
    
    // Sprites
    if (igCollapsingHeader_TreeNodeFlags("Sprites", ImGuiTreeNodeFlags_None)) {
        uint8_t sprite_enable = vicii->registers.data[0x15];
        uint8_t sprite_x_msb = vicii->registers.data[0x10];
        uint8_t sprite_multicolor = vicii->registers.data[0x1C];
        uint8_t sprite_priority = vicii->registers.data[0x1B];
        uint8_t sprite_expand_x = vicii->registers.data[0x1D];
        uint8_t sprite_expand_y = vicii->registers.data[0x17];
        
        for (int i = 0; i < VICII_NUM_SPRITES; i++) {
            // Push unique ID for each sprite to prevent conflicts
            igPushID_Int(i);
            
            bool enabled = (sprite_enable >> i) & 1;
            uint16_t x = vicii->registers.data[i * 2] | (((sprite_x_msb >> i) & 1) << 8);
            uint8_t y = vicii->registers.data[i * 2 + 1];
            bool multicolor = (sprite_multicolor >> i) & 1;
            bool priority = (sprite_priority >> i) & 1;
            bool expand_x = (sprite_expand_x >> i) & 1;
            bool expand_y = (sprite_expand_y >> i) & 1;
            
            char sprite_label[64];
            snprintf(sprite_label, sizeof(sprite_label), enabled ? "Sprite %d (ENABLED)" : "Sprite %d (disabled)", i);
            
            // Use collapsing header instead of tree node for consistency
            if (igCollapsingHeader_BoolPtr(sprite_label, NULL, ImGuiTreeNodeFlags_None)) {
                igText("Position: X=%d, Y=%d", x, y);
                igText("Color: $%02X", vicii->registers.data[0x27 + i]);
                igText("Multicolor: %s", multicolor ? "YES" : "NO");
                igText("Priority: %s", priority ? "Behind BG" : "In front of BG");
                igText("Expand X: %s", expand_x ? "2x" : "1x");
                igText("Expand Y: %s", expand_y ? "2x" : "1x");
                igText("Data Pointer: $%02X", vicii->registers.data[0x3F8 + i]);
            } else {
                igSameLine(0, -1.0f);
                igText("X=%d Y=%d Color=$%02X", x, y, vicii->registers.data[0x27 + i]);
            }
            
            igPopID(); // Pop sprite ID
        }
        
        igSeparator();
        igText("Sprite Multicolor 0: $%02X", vicii->registers.data[0x25]);
        igText("Sprite Multicolor 1: $%02X", vicii->registers.data[0x26]);
    }
    
    // Colors
    if (igCollapsingHeader_TreeNodeFlags("Colors", ImGuiTreeNodeFlags_None)) {
        igText("Border Color: $%02X", vicii->registers.data[0x20]);
        igText("Background Color 0: $%02X", vicii->registers.data[0x21]);
        igText("Background Color 1: $%02X", vicii->registers.data[0x22]);
        igText("Background Color 2: $%02X", vicii->registers.data[0x23]);
        igText("Background Color 3: $%02X", vicii->registers.data[0x24]);
    }
    
    // Collision detection
    if (igCollapsingHeader_TreeNodeFlags("Collision Detection", ImGuiTreeNodeFlags_None)) {
        igText("Sprite-Sprite Collision: $%02X", vicii->registers.data[VICII_REGS_SIZE]);
        igText("Sprite-Background Collision: $%02X", vicii->registers.data[VICII_REGS_SIZE + 1]);
        
        if (igButton("Clear Collisions", (ImVec2){0, 0})) {
            // Clear collision registers (would need to implement this properly)
            vicii->registers.data[VICII_REGS_SIZE] = 0;
            vicii->registers.data[VICII_REGS_SIZE + 1] = 0;
        }
    }
    
    // Interrupt status
    if (igCollapsingHeader_TreeNodeFlags("Interrupts", ImGuiTreeNodeFlags_None)) {
        uint8_t irq_status = vicii->registers.data[0x19];
        uint8_t irq_enable = vicii->registers.data[0x1A];
        
        igText("IRQ Status ($D019): $%02X", irq_status);
        igIndent(20.0f);
        igText("IRQ: %s", (irq_status & 0x80) ? "ACTIVE" : "inactive");
        igText("Raster IRQ: %s", (irq_status & 0x01) ? "SET" : "clear");
        igText("Sprite-BG IRQ: %s", (irq_status & 0x02) ? "SET" : "clear");
        igText("Sprite-Sprite IRQ: %s", (irq_status & 0x04) ? "SET" : "clear");
        igText("Light Pen IRQ: %s", (irq_status & 0x08) ? "SET" : "clear");
        igUnindent(20.0f);
        
        igText("IRQ Enable ($D01A): $%02X", irq_enable);
        igIndent(20.0f);
        igText("Raster IRQ: %s", (irq_enable & 0x01) ? "ENABLED" : "disabled");
        igText("Sprite-BG IRQ: %s", (irq_enable & 0x02) ? "ENABLED" : "disabled");
        igText("Sprite-Sprite IRQ: %s", (irq_enable & 0x04) ? "ENABLED" : "disabled");
        igText("Light Pen IRQ: %s", (irq_enable & 0x08) ? "ENABLED" : "disabled");
        igUnindent(20.0f);
        
        uint16_t raster_irq = vicii->registers.data[0x12] | ((vicii->registers.data[0x11] & 0x80) << 1);
        igText("Raster IRQ Line: %d", raster_irq);
    }
    
    // Raw register dump
    if (igCollapsingHeader_TreeNodeFlags("Raw Register Dump", ImGuiTreeNodeFlags_None)) {
        for (int i = 0; i < 47; i += 8) {
            char line[256] = {0};
            int pos = 0;
            pos += snprintf(line + pos, sizeof(line) - pos, "$D%03X: ", 0x000 + i);
            
            for (int j = 0; j < 8 && (i + j) < 47; j++) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", vicii->registers.data[i + j]);
            }
            
            igText("%s", line);
        }
    }

    // Reset to single column at the end
    igColumns(1, NULL, false);

    igEnd();
}

void vicii_gui_render_settings_window(void* chip, bool* show_window, const char* window_title) {
    if (!*show_window) return;
    
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) {
        *show_window = false;
        return;
    }
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("VIC-II Configuration");
    igSeparator();
    
    igText("Chip Type: %s", get_vicii_type_name(vicii));
    igText("Video Standard: %s", get_video_standard(vicii));
    igText("Timing: %d cycles/line, %d lines/frame", vicii->timing.cycles_per_line, vicii->timing.total_lines);
    
    igSeparator();
    
    igText("Display Settings");
    // Add interactive controls here later if needed
    igText("(Settings controls will be added here)");

    igEnd();
}
