#include "system_selection_dialog.h"
#include "../core/emulated_system.h"
#include "../core/system_registry.h"
#include "imgui.h"
#include <stdio.h>
#include <cstring>
#include <cctype>

SystemSelectionDialog::SystemSelectionDialog()
    : is_open_(false)
    , selected_system_index_(-1)
    , selected_memory_option_(-1)
    , selected_region_option_(-1)
    , selected_system_name_(nullptr)
    , selection_confirmed_(false)
    , region_filter_(0)  // 0 = All Regions
{
    search_filter_[0] = '\0';
}

void SystemSelectionDialog::open() {
    is_open_ = true;
    selected_system_index_ = -1;
    selected_memory_option_ = -1;
    selected_region_option_ = -1;
    selected_system_name_ = nullptr;
    selection_confirmed_ = false;
    search_filter_[0] = '\0';
    region_filter_ = 0;  // Reset to All Regions
}

void SystemSelectionDialog::close() {
    is_open_ = false;
}

void SystemSelectionDialog::reset() {
    selected_system_index_ = -1;
    selected_memory_option_ = -1;
    selected_region_option_ = -1;
    selected_system_name_ = nullptr;
    selection_confirmed_ = false;
}

void SystemSelectionDialog::render(bool allow_cancel) {
    // Open the popup on first frame when flag is set (must be before BeginPopupModal)
    if (is_open_ && !ImGui::IsPopupOpen("Select System to Emulate")) {
        ImGui::OpenPopup("Select System to Emulate");
    }
    
    if (!is_open_) {
        return;
    }
    
    // Get registered systems
    const auto& systems = SystemRegistry::instance().get_systems();
    
    if (systems.empty()) {
        ImGui::OpenPopup("Error");
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("ERROR: No systems registered!");
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                close();
            }
            ImGui::EndPopup();
        }
        return;
    }
    
    // Center the dialog
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, 
                                     viewport->Pos.y + viewport->Size.y * 0.5f),
                             ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
    
    // Create modal dialog
    bool dialog_open = true;
    if (ImGui::BeginPopupModal("Select System to Emulate", &dialog_open, 
                               ImGuiWindowFlags_NoResize)) {
        
        ImGui::Text("Choose a system configuration to emulate:");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Filtering options
        ImGui::PushItemWidth(300);
        ImGui::InputTextWithHint("##SearchFilter", "Search systems...", search_filter_, sizeof(search_filter_));
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::PushItemWidth(150);
        const char* region_items[] = { "All Regions", "NTSC", "PAL", "PAL-M", "SECAM" };
        ImGui::Combo("##RegionFilter", &region_filter_, region_items, IM_ARRAYSIZE(region_items));
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // System list with tree nodes for configurations
        ImGui::BeginChild("SystemList", ImVec2(0, -40), true);
        for (size_t i = 0; i < systems.size(); i++) {
            const auto& [descriptor, factory] = systems[i];
            const auto& hardware_traits = descriptor.hardware_traits;
            
            // Apply filters
            bool passes_filter = true;
            
            // Text search filter (case-insensitive)
            if (search_filter_[0] != '\0') {
                char name_lower[256];
                char desc_lower[256];
                char filter_lower[256];
                
                // Convert to lowercase for comparison
                snprintf(name_lower, sizeof(name_lower), "%s", descriptor.name);
                snprintf(desc_lower, sizeof(desc_lower), "%s", descriptor.description);
                snprintf(filter_lower, sizeof(filter_lower), "%s", search_filter_);
                
                for (char* p = name_lower; *p; p++) *p = tolower(*p);
                for (char* p = desc_lower; *p; p++) *p = tolower(*p);
                for (char* p = filter_lower; *p; p++) *p = tolower(*p);
                
                if (strstr(name_lower, filter_lower) == nullptr &&
                    strstr(desc_lower, filter_lower) == nullptr) {
                    passes_filter = false;
                }
            }
            
            // Region filter (0 = All, 1 = NTSC, 2 = PAL, 3 = PAL-M, 4 = SECAM)
            if (passes_filter && region_filter_ > 0) {
                VideoRegion target_region = (VideoRegion)(region_filter_ - 1);
                bool has_matching_region = false;
                for (const auto& region_opt : hardware_traits.region_options) {
                    if (region_opt.region == target_region) {
                        has_matching_region = true;
                        break;
                    }
                }
                // Only filter out if system has regions but none match
                if (!has_matching_region && !hardware_traits.region_options.empty()) {
                    passes_filter = false;
                }
            }
            
            // Skip if doesn't pass filters
            if (!passes_filter) {
                continue;
            }
            
            // Check if system has configuration options
            bool has_memory_options = !hardware_traits.memory_options.empty();
            bool has_region_options = !hardware_traits.region_options.empty();
            bool has_configurations = has_memory_options || has_region_options;
            
            ImGui::PushID((int)i);
            
            if (has_configurations) {
                // System with configurations - use tree node
                ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                bool node_open = ImGui::TreeNodeEx(descriptor.name, node_flags);
                
                // Show description as tooltip on system name
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", descriptor.description);
                    if (descriptor.supported_extensions) {
                        ImGui::Separator();
                        ImGui::Text("Supported files:");
                        for (const char** ext = descriptor.supported_extensions; *ext != nullptr; ext++) {
                            ImGui::BulletText("%s", *ext);
                        }
                    }
                    ImGui::EndTooltip();
                }
                
                if (node_open) {
                    // Show memory options if available
                    if (has_memory_options) {
                        ImGui::TextDisabled("Memory Configuration:");
                        ImGui::Indent();
                        for (size_t mem_idx = 0; mem_idx < hardware_traits.memory_options.size(); mem_idx++) {
                            const auto& mem_opt = hardware_traits.memory_options[mem_idx];
                            bool is_selected = (selected_system_index_ == (int)i &&
                                              selected_memory_option_ == (int)mem_idx);
                            
                            char label[256];
                            snprintf(label, sizeof(label), "%s%s", mem_opt.name,
                                   mem_opt.is_default ? " (default)" : "");
                            
                            if (ImGui::Selectable(label, is_selected)) {
                                selected_system_index_ = (int)i;
                                selected_memory_option_ = (int)mem_idx;
                                selected_system_name_ = descriptor.short_name;
                                // Set default region if not selected
                                if (selected_region_option_ < 0 && has_region_options) {
                                    for (size_t r = 0; r < hardware_traits.region_options.size(); r++) {
                                        if (hardware_traits.region_options[r].is_default) {
                                            selected_region_option_ = (int)r;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            // Double-click to confirm
                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                selection_confirmed_ = true;
                                close();
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::Unindent();
                    }
                    
                    // Show region options if available
                    if (has_region_options) {
                        if (has_memory_options) ImGui::Spacing();
                        ImGui::TextDisabled("Video Region:");
                        ImGui::Indent();
                        for (size_t reg_idx = 0; reg_idx < hardware_traits.region_options.size(); reg_idx++) {
                            const auto& reg_opt = hardware_traits.region_options[reg_idx];
                            bool is_selected = (selected_system_index_ == (int)i &&
                                              selected_region_option_ == (int)reg_idx);
                            
                            char label[256];
                            snprintf(label, sizeof(label), "%s%s", reg_opt.name,
                                   reg_opt.is_default ? " (default)" : "");
                            
                            if (ImGui::Selectable(label, is_selected)) {
                                selected_system_index_ = (int)i;
                                selected_region_option_ = (int)reg_idx;
                                selected_system_name_ = descriptor.short_name;
                                // Set default memory if not selected
                                if (selected_memory_option_ < 0 && has_memory_options) {
                                    for (size_t m = 0; m < hardware_traits.memory_options.size(); m++) {
                                        if (hardware_traits.memory_options[m].is_default) {
                                            selected_memory_option_ = (int)m;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            // Double-click to confirm
                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                selection_confirmed_ = true;
                                close();
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::Unindent();
                    }
                    
                    ImGui::TreePop();
                }
            } else {
                // System without configurations - simple selectable
                bool is_selected = (selected_system_index_ == (int)i);
                
                if (ImGui::Selectable(descriptor.name, is_selected, 0, ImVec2(0, 0))) {
                    selected_system_index_ = (int)i;
                    selected_system_name_ = descriptor.short_name;
                    selected_memory_option_ = -1;
                    selected_region_option_ = -1;
                }
                
                // Double-click to confirm
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    selection_confirmed_ = true;
                    close();
                    ImGui::CloseCurrentPopup();
                }
                
                // Show description as tooltip
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", descriptor.description);
                    if (descriptor.supported_extensions) {
                        ImGui::Separator();
                        ImGui::Text("Supported files:");
                        for (const char** ext = descriptor.supported_extensions; *ext != nullptr; ext++) {
                            ImGui::BulletText("%s", *ext);
                        }
                    }
                    ImGui::EndTooltip();
                }
                
                // Show description inline
                if (descriptor.description) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(" - %s", descriptor.description);
                }
            }
            
            ImGui::PopID();
        }
        ImGui::EndChild();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons
        ImGui::BeginDisabled(selected_system_index_ < 0);
        if (ImGui::Button("OK", ImVec2(120, 0)) || 
            (selected_system_index_ >= 0 && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            // Confirm selection
            selection_confirmed_ = true;
            close();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        
        if (allow_cancel) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)) || !dialog_open) {
                close();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
    
    // Handle popup closure
    if (!dialog_open && allow_cancel) {
        close();
    }
}
