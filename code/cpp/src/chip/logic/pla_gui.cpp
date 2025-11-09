#include "pla.h"
#include "../../gui/imgui_interface.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#include "../../systems/c64/c64_bus.h"
#include "../../systems/c64/c64.h"
#include "../../chip/video/vic_ii/vicii_common.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <string.h>
#include <memory>

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

// Helper function to get detailed chip information for I/O areas
static const char* get_io_chip_detail(uint8_t io_page) {
    switch (io_page) {
        case 0: return "VIC-II Video Interface Controller ($D000-$D0FF)";
        case 1: return "VIC-II Extended Registers ($D100-$D1FF)";
        case 2: return "VIC-II Mirror ($D200-$D2FF)";
        case 3: return "VIC-II Mirror ($D300-$D3FF)";
        case 4: return "SID Sound Interface Device ($D400-$D4FF)";
        case 5: return "SID Mirror ($D500-$D5FF)";
        case 6: return "SID Mirror ($D600-$D6FF)";
        case 7: return "SID Mirror ($D700-$D7FF)";
        case 8: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case 9: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case 10: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case 11: return "Color RAM ($D800-$DBFF, 1KB 4-bit)";
        case 12: return "CIA1 Complex Interface Adapter ($DC00-$DCFF)";
        case 13: return "CIA2 Complex Interface Adapter ($DD00-$DDFF)";
        case 14: return "I/O Expansion Area 1 ($DE00-$DEFF)";
        case 15: return "I/O Expansion Area 2 ($DF00-$DFFF)";
        default: return "Unknown I/O page";
    }
}

// Helper function to get memory bank usage notes (the elaborate overview that was lost)
static const char* get_memory_bank_notes(int bank) {
    switch (bank) {
        case 0: return "Zero page, stack, RAM";
        case 1: return "Basic ML program start";
        case 2: return "User programs/data";
        case 3: return "User programs/data";
        case 4: return "User programs/data";
        case 5: return "User programs/data";
        case 6: return "User programs/data";
        case 7: return "User programs/data";
        case 8: return "Cartridge ROM Low";
        case 9: return "Cartridge ROM Low";
        case 0xA: return "BASIC ROM / RAM";
        case 0xB: return "BASIC ROM / RAM";
        case 0xC: return "Upper RAM";
        case 0xD: return "I/O / Character ROM";
        case 0xE: return "KERNAL ROM / RAM";
        case 0xF: return "KERNAL ROM / RAM";
        default: return "";
    }
}

// ============================================================================
// C64 PLA LAYOUT (28-pin DIP)
// ============================================================================

inline ChipLayout create_pla_layout() {
    // Start with DIP-28 base layout
    ChipLayout layout = create_dip28_layout();
    
    // Update package info for C64 PLA
    layout.markings = {
        "906114-01",                 // part_number
        "Commodore",                 // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate C64 PLA pinout (28-pin DIP) - Official Specifications
    // Right-hand pins (15-28) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, NC,       VCC, 28);          // FE/NC (Programming)    / +5V Power
    PIN_LR(layout,  2, A13,      A12, 27);          // I7 (A13)              / I8 (A12)
    PIN_LR(layout,  3, A14,      AEC, 26);          // I6 (A14)              / I10 (#AEC)
    PIN_LR(layout,  4, A15,      RW, 25);           // I5 (A15)              / I11 (R/#W)
    PIN_LR(layout,  5, UNKNOWN,  EXROM, 24);        // I4 (#VA14)            / I12 (#EXROM)
    PIN_LR(layout,  6, CHAREN,   GAME, 23);         // I3 (#CHAREN)          / I13 (#GAME)
    PIN_LR(layout,  7, HIRAM,    UNKNOWN, 22);      // I2 (#HIRAM)           / I14 (VA13)
    PIN_LR(layout,  8, LORAM,    UNKNOWN, 21);      // I1 (#LORAM)           / I15 (VA12)
    PIN_LR(layout,  9, CAS,      CS, 20);           // I0 (#CAS)             / #CE (Chip Enable)
    PIN_LR(layout, 10, ROMH,     CASRAM, 19);       // F7 (#ROMH)            / F0 (#CASRAM)
    PIN_LR(layout, 11, ROML,     BASIC, 18);        // F6 (#ROML)            / F1 (#BASIC)
    PIN_LR(layout, 12, IO,       KERNAL, 17);       // F5 (#I/O)             / F2 (#KERNAL)
    PIN_LR(layout, 13, GRW,      CHAROM, 16);       // F4 (GR/#W)            / F3 (#CHAROM)
    PIN_LR(layout, 14, VSS,      CASRAM_PLA, 15);   // VSS (Ground)          / F0 (#CASRAM)
    
    return layout;
}

// ============================================================================
// PLA GUI DEBUG WINDOW
// ============================================================================

// Helper function to get PLA pin states for visualization
static std::vector<PinSignalState> get_pla_pin_states(c64_t* c64, const ChipLayout* layout) {
    std::vector<PinSignalState> pin_states;
    if (!c64 || !layout) return pin_states;
    
    int total_pins = layout->get_total_pins();
    pin_states.resize(total_pins);
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = PinSignalState{
            .pin_number = static_cast<uint8_t>(i + 1),
            .signal_level = false,
            .drive_direction = false,
            .signal_value = 0,
            .high_impedance = true,
            .has_pullup = false,
            .has_pulldown = false,
            .signal_valid = true,
            .analog_voltage = 0.0f,
            .is_pwm = false,
            .pwm_duty_cycle = 0.0f
        };
    }
    
    // Set pin states based on PLA logic
    uint8_t current_mode = c64->bus.pla_banking_mode;
    
    // Power pins are always active
    pin_states[13].signal_level = false;  // VSS (Ground, pin 14)
    pin_states[13].high_impedance = false;
    pin_states[27].signal_level = true;   // VCC (+5V, pin 28)
    pin_states[27].high_impedance = false;
    
    // Control signal pins based on current banking mode
    pin_states[9].signal_level = (current_mode & 0x04) != 0;   // CHAREN (pin 10)
    pin_states[9].high_impedance = false;
    pin_states[10].signal_level = (current_mode & 0x02) != 0;  // HIRAM (pin 11)
    pin_states[10].high_impedance = false;
    pin_states[11].signal_level = (current_mode & 0x01) != 0;  // LORAM (pin 12)
    pin_states[11].high_impedance = false;
    pin_states[22].signal_level = (current_mode & 0x10) != 0;  // GAME (pin 23)
    pin_states[22].high_impedance = false;
    pin_states[23].signal_level = (current_mode & 0x08) != 0;  // EXROM (pin 24)
    pin_states[23].high_impedance = false;
    
    return pin_states;
}

// Use global renderer for PLA chip visualization
static ChipLayout& get_pla_layout() {
    static ChipLayout layout = create_pla_layout();
    return layout;
}

void pla_render_debug_window(void* chip, bool* show_window) {
    // The chip parameter is expected to be a c64_t* since PLA is part of the C64 bus
    c64_t* c64 = (c64_t*)chip;
    
    if (!c64 || !show_window || !*show_window) return;
        
#ifdef IMGUI_VERSION
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "PLA Debug");
    
    if (!ImGui::Begin(window_title, show_window)) {
        ImGui::End();
        return;
    }

    // Create two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetWindowSize();
    
    // Left column: Chip Visualization (fixed width ~250px)
    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();
        
        // Calculate chip center for visualization
        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 200.0f; // Space for the chip
        
        // Get global renderer and chip layout
        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_pla_layout();
        
        // Get current pin states from PLA
        std::vector<PinSignalState> pin_states = get_pla_pin_states(c64, &layout);
        
        // Render the chip using global renderer
        renderer.render(layout, chip_center, pin_states, "PLA");
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // PLA State Section
        ImGui::Text("Programmable Logic Array - C64 PLA");
        ImGui::Separator();

        // Mode tracking and control
        uint8_t current_mode = c64->bus.pla_banking_mode;
        
        // Static state for PLA debug window
        static bool auto_track_mode = true;
        static int pla_debug_selected_mode = 0;
        
        // Auto-track mode checkbox
        ImGui::Checkbox("Auto-track active mode", &auto_track_mode);
        
        if (auto_track_mode) {
            pla_debug_selected_mode = current_mode;
        }
        
        ImGui::SameLine(0, -1.0f);
        ImGui::Text("Current Mode: %d", current_mode);
        
        // Manual mode selector as active-low toggles in requested order: #LORAM, #HIRAM, #GAME, #EXROM, #CHAREN
        static bool loram_n = false, hiram_n = false, game_n = false, exrom_n = false, charen_n = false;
        // Extract bits from current mode (active-low)
        loram_n = ((pla_debug_selected_mode & 0x01) == 0);
        hiram_n = ((pla_debug_selected_mode & 0x02) == 0);
        game_n  = ((pla_debug_selected_mode & 0x10) == 0);
        exrom_n = ((pla_debug_selected_mode & 0x08) == 0);
        charen_n = ((pla_debug_selected_mode & 0x04) == 0);

        bool changed = false;
        ImGui::Text("Viewing Mode:");
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#LORAM", &loram_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#HIRAM", &hiram_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#GAME", &game_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#EXROM", &exrom_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#CHAREN", &charen_n);

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
        
        ImGui::Separator();
        
        // Tab bar for CPU and VIC-II views
        if (ImGui::BeginTabBar("PLA Views", ImGuiTabBarFlags_None)) {
            
            // CPU Memory View Tab
            if (ImGui::BeginTabItem("CPU Memory View", NULL, ImGuiTabItemFlags_None)) {
                // CPU Memory Banking Table
                ImGui::Text("CPU Memory Banking (16 x 4KB banks):");
                ImGui::Text("Mode %d - %s", pla_debug_selected_mode,
                       (pla_debug_selected_mode == current_mode) ? "(ACTIVE)" : "(Preview)");
                ImGui::Text("Configuration: %s", get_pla_mode_cpu_description(pla_debug_selected_mode));
                
                ImGui::Separator();
                
                if (ImGui::BeginTable("CPUBanking", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 0))) {
                    // Table headers
                    ImGui::TableSetupColumn("Bank", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Address Range", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Encoded", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Read Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Write Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Read Offset", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Write Offset", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Notes", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableHeadersRow();
                    
                    // Table rows - properly handle PLA-dependent chip mapping
                    for (int bank = 0; bank < 16; bank++) {
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);
                        
                        uint16_t bank_start = bank * 0x1000;
                        uint16_t bank_end = bank_start + 0x0FFF;
                        
                        // Get encoded value for this bank and mode - this is the key to PLA-aware mapping
                        uint8_t encoded = 0;
                        if (pla_debug_selected_mode < 32) {
                            encoded = c64->bus.cpu_encoded_chip_per_bank_per_mode[pla_debug_selected_mode][bank];
                        }
                        
                        // Decode chips from the PLA-generated encoding
                        uint8_t read_chip = decode_read_chip(encoded);
                        uint8_t write_chip = decode_write_chip(encoded);
                        
                        // Special handling for I/O area - this changes based on PLA mode
                        if (read_chip == CHIP_IO || write_chip == CHIP_IO) {
                            // Show I/O pages as individual rows (16 pages, $D000-$DFFF)
                            for (int page = 0; page < 16; page++) {
                                if (page > 0) {
                                    ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);
                                }
                                uint16_t page_start = bank_start + (page * 0x100);
                                uint16_t page_end = page_start + 0xFF;
                                
                                // For I/O pages, the actual chip depends on the page number
                                uint8_t page_read_chip = (read_chip == CHIP_IO) ? page : read_chip;
                                uint8_t page_write_chip = (write_chip == CHIP_IO) ? page : write_chip;
                                
                                chip_description_t read_desc = {0};
                                chip_description_t write_desc = {0};
                                c64_bus_get_chip_description(&c64->bus, page_read_chip, &read_desc);
                                c64_bus_get_chip_description(&c64->bus, page_write_chip, &write_desc);
                                uint16_t read_offset = (read_desc.base <= page_start) ? (page_start - read_desc.base) : 0;
                                uint16_t write_offset = (write_desc.base <= page_start) ? (page_start - write_desc.base) : 0;
    
                                ImGui::TableSetColumnIndex(0);
                                if (page == 0) {
                                    ImGui::Text("$%X", bank);
                                } else {
                                    ImGui::Text("  .%X", page); // Sub-page indicator
                                }
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("$%04X-$%04X", page_start, page_end);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%02X", encoded);
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Text("%s", get_io_chip_detail(page_read_chip));
                                ImGui::TableSetColumnIndex(4);
                                ImGui::Text("%s", get_io_chip_detail(page_write_chip));
                                ImGui::TableSetColumnIndex(5);
                                ImGui::Text("$%04X", read_offset);
                                ImGui::TableSetColumnIndex(6);
                                ImGui::Text("$%04X", write_offset);
                                ImGui::TableSetColumnIndex(7);
                                // I/O area notes - these are always the same regardless of PLA mode
                                if (read_chip == CHIP_IO) {
                                    ImGui::Text("I/O Area");
                                } else {
                                    const char* chip_detail = get_io_chip_detail(page);
                                    if (strstr(chip_detail, "VIC-II")) {
                                        ImGui::Text("VIC-II registers");
                                    } else if (strstr(chip_detail, "SID")) {
                                        ImGui::Text("SID registers");
                                    } else if (strstr(chip_detail, "Color RAM")) {
                                        ImGui::Text("Color RAM");
                                    } else if (strstr(chip_detail, "CIA1")) {
                                        ImGui::Text("CIA1 registers");
                                    } else if (strstr(chip_detail, "CIA2")) {
                                        ImGui::Text("CIA2 registers");
                                    } else if (strstr(chip_detail, "I/O Expansion")) {
                                        ImGui::Text("Expansion I/O");
                                    } else {
                                        ImGui::Text("I/O page");
                                    }
                                }
                            }
                        } else {
                            // Regular bank - chip mapping depends on PLA mode
                            chip_description_t read_desc = {0};
                            chip_description_t write_desc = {0};
                            c64_bus_get_chip_description(&c64->bus, read_chip, &read_desc);
                            c64_bus_get_chip_description(&c64->bus, write_chip, &write_desc);
                            
                            // Calculate offsets - these can vary based on chip remapping
                            uint16_t read_offset = (read_desc.base <= bank_start) ? (bank_start - read_desc.base) : 0;
                            uint16_t write_offset = (write_desc.base <= bank_start) ? (bank_start - write_desc.base) : 0;
                            
                            // Special case for ROMH remap (appears at $E000/$F000 instead of $A000/$B000)
                            if (read_chip == CHIP_ROMH && (bank_start >= 0xE000)) {
                                read_offset = bank_start - 0xE000;
                            }
                            if (write_chip == CHIP_ROMH && (bank_start >= 0xE000)) {
                                write_offset = bank_start - 0xE000;
                            }
    
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("$%X", bank);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("$%04X-$%04X", bank_start, bank_end);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%02X", encoded);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%s", c64_bus_chip_to_title(read_chip));
                            ImGui::TableSetColumnIndex(4);
                            ImGui::Text("%s", c64_bus_chip_to_title(write_chip));
                            ImGui::TableSetColumnIndex(5);
                            ImGui::Text("$%04X", read_offset);
                            ImGui::TableSetColumnIndex(6);
                            ImGui::Text("$%04X", write_offset);
                            ImGui::TableSetColumnIndex(7);
                            // Enhanced notes that reflect the actual PLA-dependent chip mapping
                            const char* base_notes = get_memory_bank_notes(bank);
                            // Add context about what's actually mapped based on PLA mode
                            if (read_chip != write_chip) {
                                ImGui::Text("%s (R:%s/W:%s)", base_notes,
                                           c64_bus_chip_to_title(read_chip),
                                           c64_bus_chip_to_title(write_chip));
                            } else if (read_chip == CHIP_UNMAPPED) {
                                ImGui::Text("%s (unmapped)", base_notes);
                            } else {
                                ImGui::Text("%s (%s)", base_notes, c64_bus_chip_to_title(read_chip));
                            }
                        }
                    }
                    
                    ImGui::EndTable();
                }
                
                ImGui::EndTabItem();
            }
            
            // VIC-II Memory View Tab
            if (ImGui::BeginTabItem("VIC-II Memory View", NULL, ImGuiTabItemFlags_None)) {
                ImGui::Text("VIC-II Memory Banking (16 x 4KB banks):");
                ImGui::Text("Mode %d - %s", pla_debug_selected_mode,
                       (pla_debug_selected_mode == current_mode) ? "(ACTIVE)" : "(Preview)");
                       
                // VIC-II specific information
                // Get current VIC-II bank from CIA2 Port A bits 0-1 (would need CIA2 access)
                uint8_t current_vic_bank = 0; // Default bank 0 for now
                uint16_t current_vic_bank_address = current_vic_bank * 0x4000;
                ImGui::Text("Configuration: %s", get_pla_mode_vicii_description(pla_debug_selected_mode, current_vic_bank_address));
                
                ImGui::Separator();
                ImGui::Text("VIC-II Bank Control:");
                ImGui::Text("CIA2 Port A bits 0-1: %d (Bank %d active)", 0, current_vic_bank); // Simplified for now
                
                ImGui::Separator();
                
                // VIC-II memory banking table (16 x 4KB banks)
                if (ImGui::BeginTable("VICIIBanking", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 0))) {
                    ImGui::TableSetupColumn("Bank", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Address Range", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Chip Title", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_None, 0.0f, 0);
                    ImGui::TableHeadersRow();
                    
                    for (int bank = 0; bank < 16; bank++) {
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", bank);
                        ImGui::TableSetColumnIndex(1);

                        uint16_t bank_start = bank * 0x1000;
                        ImGui::Text("$%04X-$%04X", bank_start, bank_start + 0x0FFF);
                        ImGui::TableSetColumnIndex(2);

                        // Get chip for this VIC-II bank and mode - PLA-dependent!
                        uint8_t read_chip = CHIP_RAM; // Default to RAM
                        if (pla_debug_selected_mode < 32) {
                            read_chip = c64->bus.vicii_chip_per_bank_per_mode[pla_debug_selected_mode][bank];
                        }

                        ImGui::Text("%02d", read_chip);
                        ImGui::TableSetColumnIndex(3);
                        // Show what chip VIC-II actually sees at this address in this PLA mode
                        const char* chip_title = c64_bus_chip_to_title(read_chip);
                        if (read_chip == CHIP_CHARROM && pla_debug_selected_mode != current_mode) {
                            ImGui::Text("%s (mode-dep)", chip_title); // Character ROM visibility depends on PLA mode
                        } else if (read_chip == CHIP_RAM && bank >= 0xA && bank <= 0xF) {
                            ImGui::Text("%s (under ROM)", chip_title); // RAM under ROM areas
                        } else {
                            ImGui::Text("%s", chip_title);
                        }
                        ImGui::TableSetColumnIndex(4);

                        chip_description_t read_desc = {0};
                        c64_bus_get_chip_description(&c64->bus, read_chip, &read_desc);
                        uint16_t read_offset = (read_desc.base <= bank_start) ? (bank_start - read_desc.base) : 0;

                        ImGui::Text("$%04X", read_offset);
                        ImGui::TableSetColumnIndex(5);
                        // Status: highlight if this 4KB bank is in the active VIC-II 16KB bank
                        bool is_active_bank = ((bank_start / 0x4000) == current_vic_bank);
                        if (is_active_bank && pla_debug_selected_mode == current_mode) {
                            ImGui::Text("ACTIVE");
                        } else if (is_active_bank) {
                            ImGui::Text("ACTIVE (diff mode)");
                        } else {
                            ImGui::Text("Inactive");
                        }
                    }
                    ImGui::EndTable();
                }
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        // Chip Information Legend (moved to bottom for better space utilization)
        ImGui::Separator();
        ImGui::Text("CHIP Legend:");
        if (ImGui::BeginTable("CHIPLegend", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 150))) {
            ImGui::TableSetupColumn("CHIP ID", ImGuiTableColumnFlags_None, 0.0f, 0);
            ImGui::TableSetupColumn("Memory Range", ImGuiTableColumnFlags_None, 0.0f, 0);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_None, 0.0f, 0);
            ImGui::TableSetupColumn("Chip", ImGuiTableColumnFlags_None, 0.0f, 0);
            ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_None, 0.0f, 0);
            ImGui::TableHeadersRow();
            
            // Show all valid chip IDs
            for (size_t i = 0; i < VALID_CHIP_COUNT; i++) {
                uint8_t chip = VALID_CHIP_IDS[i];
                ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%02d", chip);
                ImGui::TableSetColumnIndex(1);

                chip_description_t desc = {0};
                bool has_desc = c64_bus_get_chip_description(&c64->bus, chip, &desc);
                if (has_desc && desc.size > 0) {
                    ImGui::Text("$%04X-$%04X", desc.base, (uint16_t)(desc.base + desc.size - 1));
                } else {
                    ImGui::Text("-");
                }
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", c64_bus_size_to_str(desc.size));
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", (chip == CHIP_UNMAPPED) ? "Unmapped" : c64_bus_chip_to_title(chip));
                ImGui::TableSetColumnIndex(4);
                if (has_desc) {
                    ImGui::Text("%s", desc.label);
                } else {
                    ImGui::Text("%s", c64_bus_chip_to_title(chip));
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
#endif
}

// ============================================================================
// PLA GUI SETTINGS WINDOW
// ============================================================================

void pla_render_settings_window(void* chip, bool* show_window) {
    c64_t* c64 = (c64_t*)chip;
    if (!c64) return;
    
    if (!*show_window) return;
    
#ifdef IMGUI_VERSION
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "PLA Settings");
    
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        return;
    }

    // Show PLA information
    ImGui::Text("Programmable Logic Array - C64 PLA Configuration");
    ImGui::Separator();
    ImGui::Text("Chip Type: Commodore 906114-01 PLA");
    ImGui::Text("Package: 28-pin DIP");

    // PLA State Section
    ImGui::Text("Extended PLA Debug Information");
    ImGui::Separator();
    
    uint8_t current_mode = c64->bus.pla_banking_mode;
    ImGui::Text("Current Banking Mode: %d ($%02X)", current_mode, current_mode);
    ImGui::Text("Configuration: %s", get_pla_mode_cpu_description(current_mode));
    
    ImGui::Separator();
    
    // Control signals breakdown
    ImGui::Text("Control Signals");
    ImGui::Separator();
    
    ImGui::Text("LORAM: %s", (current_mode & 0x01) ? "High" : "Low");
    ImGui::Text("HIRAM: %s", (current_mode & 0x02) ? "High" : "Low");
    ImGui::Text("CHAREN: %s", (current_mode & 0x04) ? "High" : "Low");
    ImGui::Text("EXROM: %s", (current_mode & 0x08) ? "High" : "Low");
    ImGui::Text("GAME: %s", (current_mode & 0x10) ? "High" : "Low");
    
    ImGui::End();
#endif
}
