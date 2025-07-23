// This file now contains the gui_render_pla_debug function
// moved from cimgui_interface.c for better organization

#include "pla.h"
#include "../../gui/cimgui_interface.h"
#include "../../systems/c64/c64_bus.h"
#include "../../systems/c64/c64.h"
#include "../../chip/video/vic_ii/vicii_common.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>
#include <string.h>

// Helper function to decode ACID from encoded value - TODO : Move to c64_bus.h
// Decode read/write ACIDs from encoded byte - reverse of encode_acid_rw
static uint8_t decode_read_acid(uint8_t encoded) {
    uint8_t read_code = encoded & 0xF; // Extract lower 4 bits
    if (read_code == 0) return ACID_VIC_D0; // I/O pages - will be further resolved by address
    return read_code + ACID_IO2_DF; // Decode: add offset to get actual ACID (16-24 become 1-9)
}

static uint8_t decode_write_acid(uint8_t encoded) {
    uint8_t write_code = (encoded >> 5) & 0x7; // Extract upper 3 bits
    if (write_code == 0) return ACID_VIC_D0; // I/O pages - will be further resolved by address
    return write_code + ACID_IO2_DF; // Decode: extract upper 3 bits and add offset
}

// Helper function to get PLA mode description
static const char* get_pla_mode_cpu_description(uint8_t mode) {
    static char mode_desc[256];
    
    uint8_t loram = mode & 0x01;
    uint8_t hiram = (mode >> 1) & 0x01;
    uint8_t game = (mode >> 4) & 0x01;
    uint8_t exrom = (mode >> 3) & 0x01;
    uint8_t charen = (mode >> 2) & 0x01;
    
    snprintf(mode_desc, sizeof(mode_desc), 
             "#LORAM:%d #HIRAM:%d #EXROM:%d #GAME:%d #CHAREN:%d",
             1 - loram, 1 - hiram, 1 - charen, 1 - exrom, 1 - game);
    
    return mode_desc;
}


// Helper function to get PLA mode description
static const char* get_pla_mode_vicii_description(uint8_t mode, uint16_t bank) {
    static char mode_desc[256];
    
    uint8_t game = (mode >> 4) & 0x01;
    uint8_t exrom = (mode >> 3) & 0x01;
    uint8_t va14 = (bank >> 14) & 0x01;
    
    snprintf(mode_desc, sizeof(mode_desc), 
             "#GAME:%d #EXROM:%d #VA14:%d",
             1 - game, 1 - exrom, 1 - va14);
    
    return mode_desc;
}

// Helper function to get detailed chip information
static const char* get_chip_detail(uint8_t acid) {
    switch (acid) {
        case ACID_VIC_D0: return "VIC-II Video Interface Controller ($D000-$D0FF)";
        case ACID_VIC_D1: return "VIC-II Extended Registers ($D100-$D1FF)";
        case ACID_VIC_D2: return "VIC-II Mirror ($D200-$D2FF)";
        case ACID_VIC_D3: return "VIC-II Mirror ($D300-$D3FF)";
        case ACID_SID_D4: return "SID Sound Interface Device ($D400-$D4FF)";
        case ACID_SID_D5: return "SID Mirror ($D500-$D5FF)";
        case ACID_SID_D6: return "SID Mirror ($D600-$D6FF)";
        case ACID_SID_D7: return "SID Mirror ($D700-$D7FF)";
        case ACID_COLORRAM_D8: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case ACID_COLORRAM_D9: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case ACID_COLORRAM_DA: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case ACID_COLORRAM_DB: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case ACID_CIA1_DC: return "CIA1 Complex Interface Adapter ($DC00-$DCFF)";
        case ACID_CIA2_DD: return "CIA2 Complex Interface Adapter ($DD00-$DDFF)";
        case ACID_IO1_DE: return "I/O Expansion Area 1 ($DE00-$DEFF)";
        case ACID_IO2_DF: return "I/O Expansion Area 2 ($DF00-$DFFF)";
        case ACID_ZEROBANK: return "Zero Page RAM ($0000-$00FF)";
        case ACID_RAM: return "Main RAM (64KB total)";
        case ACID_ROML: return "Cartridge ROM Low ($8000-$9FFF)";
        case ACID_ROMH: return "Cartridge ROM High ($A000-$BFFF)";
        case ACID_UNMAPPED: return "Detached/Unmapped";
        case ACID_BASIC: return "BASIC ROM ($A000-$BFFF, 8KB)";
        case ACID_CHARROM: return "Character ROM ($D000-$DFFF, 4KB)";
        case ACID_KERNAL: return "KERNAL ROM ($E000-$FFFF, 8KB)";
        default: return "Unknown ACID";
    }
}

// ============================================================================
// PLA GUI DEBUG WINDOW
// ============================================================================

void pla_render_debug_window(void* chip, bool* show_window) {
    // The chip parameter is expected to be a c64_t* since PLA is part of the C64 bus
    c64_t* c64 = (c64_t*)chip;
    
    if (!c64 || !*show_window) {
        if (show_window) *show_window = false;
        return;
    }
    
    if (!igBegin("PLA Debug", show_window, 0)) {
        igEnd();
        return;
    }
    
    // Mode tracking and control
    bool has_bus = (c64 && c64->bus);
    uint8_t current_mode = has_bus ? c64->bus->pla_banking_mode : 0;
    
    // Static state for PLA debug window
    static bool auto_track_mode = true;
    static int pla_debug_selected_mode = 0;
    
    // Auto-track mode checkbox
    igCheckbox("Auto-track active mode", &auto_track_mode);
    
    if (auto_track_mode && has_bus) {
        pla_debug_selected_mode = current_mode;
    }
    
    igSameLine(0, -1.0f);
    igText("Current Mode: %d", current_mode);
    
    // Manual mode selector as active-low toggles in requested order: #LORAM, #HIRAM, #GAME, #EXROM, #CHAREN
    static bool loram_n = false, hiram_n = false, game_n = false, exrom_n = false, charen_n = false;
    // Extract bits from current mode (active-low)
    loram_n = ((pla_debug_selected_mode & 0x01) == 0);
    hiram_n = ((pla_debug_selected_mode & 0x02) == 0);
    game_n  = ((pla_debug_selected_mode & 0x10) == 0);
    exrom_n = ((pla_debug_selected_mode & 0x08) == 0);
    charen_n = ((pla_debug_selected_mode & 0x04) == 0);

    bool changed = false;
    igText("Viewing Mode:");
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#LORAM", &loram_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#HIRAM", &hiram_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#GAME", &game_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#EXROM", &exrom_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#CHAREN", &charen_n);

    if (changed) {
        // Reconstruct mode from toggles (active-low: 0 = checked)
        pla_debug_selected_mode = 0;
        if (!loram_n)  pla_debug_selected_mode |= 0x01;
        if (!hiram_n)  pla_debug_selected_mode |= 0x02;
        if (!charen_n) pla_debug_selected_mode |= 0x04;
        if (!exrom_n)  pla_debug_selected_mode |= 0x08;
        if (!game_n)   pla_debug_selected_mode |= 0x10;
        auto_track_mode = false;
    }
    
    igSeparator();
    
    // Tab bar for CPU and VIC-II views
    if (igBeginTabBar("PLA Views", ImGuiTabBarFlags_None)) {
        
        // CPU Memory View Tab
        if (igBeginTabItem("CPU Memory View", NULL, ImGuiTabItemFlags_None)) {
            // CPU Memory Banking Table
            igText("CPU Memory Banking (16 x 4KB banks):");
            igText("Mode %d - %s", pla_debug_selected_mode,
                   (pla_debug_selected_mode == current_mode) ? "(ACTIVE)" : "(Preview)");
            igText("Configuration: %s", get_pla_mode_cpu_description(pla_debug_selected_mode));
            
            igSeparator();
            
            if (igBeginTable("CPUBanking", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, (ImVec2){0, 0}, 0)) {
                // Table headers
                igTableSetupColumn("Bank", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Address Range", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Encoded", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Read ACID", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Read Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Read Offset", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Write ACID", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Write Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableSetupColumn("Write Offset", ImGuiTableColumnFlags_None, 0.0f, 0);
                igTableHeadersRow();
                
                acid_descriptor_t read_desc = {0};
                acid_descriptor_t write_desc = {0};
                // Table rows
                for (int bank = 0; bank < 16; bank++) {
                    igTableNextRow(ImGuiTableRowFlags_None, 0.0f);
                    igTableSetColumnIndex(0);
                    igText("$%X", bank);
                    igTableSetColumnIndex(1);

                    uint16_t bank_start = bank * 0x1000;

                    igText("$%04X-$%04X", bank_start, bank_start + 0x0FFF);
                    igTableSetColumnIndex(2);

                    // Get encoded value for this bank and mode
                    uint8_t encoded = encode_acid_rw(CHIP_UNMAPPED, CHIP_UNMAPPED);
                    if (has_bus && pla_debug_selected_mode < 32) {
                        encoded = c64->bus->cpu_chip_per_bank_per_mode[pla_debug_selected_mode][bank];
                    }
                    
                    igText("$%02X", encoded);
                    igTableSetColumnIndex(3);

                    // Decode CHIPs
                    uint8_t read_chip = decode_read_chip(encoded);

                    igText("%02d", read_chip);
                    igTableSetColumnIndex(4);
                    if (read_chip == CHIP_UNMAPPED) {
                        igText("Unmapped");
                        igTableSetColumnIndex(5);
                        igText("-");
                    } else {
                        // Calculate separate read and write in-chip offsets
                        c64_bus_get_acid_descriptor(has_bus ? c64->bus : NULL, read_acid, &read_desc);

                        // Compute effective base address for offset calculation (for ROMH remap, etc)
                        uint16_t read_effective_base = read_desc.base;
                        if (read_acid == ACID_ROMH && (bank_start >= 0xE000)) { // TODO : Is there a better way to check this?
                            // ROMH remapped to $E000/$F000: treat as if base is $E000
                            read_effective_base = 0xE000;
                        }
                        
                        // WAS uint16_t read_offset = (read_effective_base <= bank_start) ? (bank_start - read_effective_base) : 0;
                        uint16_t read_offset = bank_start - read_effective_base;

                        igText("%s", (read_chip == CHIP_D0_VIC) ? "I/O" : c64_bus_chip_to_title(read_chip));
                        igTableSetColumnIndex(5);
                        igText("$%04X", read_offset);
                    }

                    uint8_t write_chip = decode_write_chip(encoded);
                    
                    igTableSetColumnIndex(6);
                    igText("%02d", write_chip);
                    igTableSetColumnIndex(7);
                    if (write_chip == CHIP_UNMAPPED) {
                        igText("Unmapped");
                        igTableSetColumnIndex(8);
                        igText("-");
                    } else {
                        c64_bus_get_acid_descriptor(has_bus ? c64->bus : NULL, write_chip, &write_desc);

                        uint16_t write_effective_base = write_desc.base;
                        if (write_chip == CHIP_ROMH && (bank_start >= 0xE000)) { // NOTE : although ROMH is not writable, PLA modes 16-23 (LHGX xx01) still map it
                            // ROMH remapped to $E000/$F000: treat as if base is $E000
                            write_effective_base = 0xE000;
                        }
                        
                        uint16_t write_offset = bank_start - write_effective_base;

                        igText("%s", (write_chip == CHIP_D0_VIC) ? "I/O" : c64_bus_chip_to_title(write_chip));
                        igTableSetColumnIndex(8);
                        igText("$%04X", write_offset);
                    }
                }
                
                igEndTable();
            }
            
            igEndTabItem();
        }
        
        // VIC-II Memory View Tab
        if (igBeginTabItem("VIC-II Memory View", NULL, ImGuiTabItemFlags_None)) {
            igText("VIC-II Memory Banking (16 x 4KB banks):");
            igText("Mode %d - %s", pla_debug_selected_mode,
                   (pla_debug_selected_mode == current_mode) ? "(ACTIVE)" : "(Preview)");
                   
            // VIC-II specific information
            if (has_bus) {                
                // Get current VIC-II bank from CIA2 Port A bits 0-1
                uint8_t current_vicii_bank = 0;
                if (c64->cia2) {
                    uint8_t cia2_port_a = c64->cia2->reg[0]; // PRA register
                    current_vicii_bank = 3 - (cia2_port_a & 0x03); // Inverted bits 0-1
                }
                uint16_t current_vicii_bank_address = current_vicii_bank * 0x4000;
                igText("Configuration: %s", get_pla_mode_vicii_description(pla_debug_selected_mode, current_vicii_bank_address));
                
                igSeparator();
                igText("VIC-II Bank Control:");
                igText("CIA2 Port A bits 0-1: %d (Bank %d active)", c64->cia2 ? (c64->cia2->reg[0] & 0x03) : 0, current_vicii_bank);
                
                igSeparator();
                
                // VIC-II memory banking table (now 16 x 4KB banks, DRY with CPU table)
                if (igBeginTable("VICIIBanking", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, (ImVec2){0, 0}, 0)) {
                    igTableSetupColumn("Bank", ImGuiTableColumnFlags_None, 0.0f, 0);
                    igTableSetupColumn("Address Range", ImGuiTableColumnFlags_None, 0.0f, 0);
                    igTableSetupColumn("ACID", ImGuiTableColumnFlags_None, 0.0f, 0);
                    igTableSetupColumn("Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
                    igTableSetupColumn("Offset", ImGuiTableColumnFlags_None, 0.0f, 0);
                    igTableSetupColumn("Status", ImGuiTableColumnFlags_None, 0.0f, 0);
                    igTableHeadersRow();
                    acid_descriptor_t read_desc = {0};
                    for (int bank = 0; bank < 16; bank++) {
                        igTableNextRow(ImGuiTableRowFlags_None, 0.0f);
                        igTableSetColumnIndex(0);
                        igText("%d", bank);
                        igTableSetColumnIndex(1);

                        uint16_t bank_start = bank * 0x1000;

                        igText("$%04X-$%04X", bank_start, bank_start + 0x0FFF);
                        igTableSetColumnIndex(2);

                        // Get CHIP for this VIC-II bank and mode
                        uint8_t read_chip = CHIP_UNMAPPED;
                        if (has_bus && pla_debug_selected_mode < 32) {
                            read_chip = c64->bus->vicii_chip_per_bank_per_mode[pla_debug_selected_mode][bank];
                        }

                        igText("%02d", read_chip);
                        igTableSetColumnIndex(3);
                        igText("%s", c64_bus_acid_to_title(read_chip));
                        igTableSetColumnIndex(4);

                        c64_bus_get_acid_descriptor(has_bus ? c64->bus : NULL, read_acid, &read_desc);
                        uint16_t read_offset = (read_desc.base <= bank_start) ? (bank_start - read_desc.base) : 0;

                        igText("$%04X", read_offset);
                        igTableSetColumnIndex(5);
                        // Status: highlight if this 4KB bank is in the active VIC-II 16KB bank
                        igText(((bank_start / 0x4000) == current_vicii_bank) ? "ACTIVE" : "Inactive");
                    }
                    igEndTable();
                }
            } else {
                igText("Bus not initialized");
            }
            
            igEndTabItem();
        }
        
        igEndTabBar();
    }
    
    // Chip Information Legend (moved to bottom for better space utilization)
    igSeparator();
    igText("ACID Legend:");
    if (igBeginTable("ACIDLegend", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, (ImVec2){0, 150}, 0)) {
        igTableSetupColumn("ACID id", ImGuiTableColumnFlags_None, 0.0f, 0);
        igTableSetupColumn("Memory Range", ImGuiTableColumnFlags_None, 0.0f, 0);
        igTableSetupColumn("Size", ImGuiTableColumnFlags_None, 0.0f, 0);
        igTableSetupColumn("Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
        igTableSetupColumn("Title", ImGuiTableColumnFlags_None, 0.0f, 0);
        igTableHeadersRow();
        acid_descriptor_t desc;
        for (int acid = 0; acid < ACID_MAX; acid++) {
            igTableNextRow(ImGuiTableRowFlags_None, 0.0f);
            igTableSetColumnIndex(0);
            igText("%02d", acid);
            igTableSetColumnIndex(1);

            bool has_desc = c64_bus_get_acid_descriptor(has_bus ? c64->bus : NULL, acid, &desc);
            if (has_desc && desc.size > 0) {
                igText("$%04X-$%04X", desc.base, (uint16_t)(desc.base + desc.size - 1));
            } else {
                igText("-");
            }
            igTableSetColumnIndex(2);
            igText("%s", c64_bus_size_to_str(desc.size));
            igTableSetColumnIndex(3);
            igText("%s", (acid == ACID_UNMAPPED) ? "Unmapped" : c64_bus_acid_to_title(acid));
            igTableSetColumnIndex(4);
            if (has_desc) {
                igText("%s", desc.label);
            } else {
                igText("%s", c64_bus_acid_to_title(acid));
            }
        }
        igEndTable();
    }
    
    igEnd();
}
