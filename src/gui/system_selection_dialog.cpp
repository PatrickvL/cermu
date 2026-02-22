#include "system_selection_dialog.h"
#include "../core/emulated_system.h"
#include "../core/system_registry.h"
#include "../core/formats/format_handler.h"
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
    , search_descriptions_(false)  // By default, only search names
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
    search_descriptions_ = false;  // Reset to name-only search
    region_filter_ = 0;  // Reset to All Regions
    selected_peripherals_.clear();  // Clear peripheral selections
    selected_custom_settings_.clear();  // Clear custom option selections
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
    selected_peripherals_.clear();  // Clear peripheral selections
    selected_custom_settings_.clear();  // Clear custom option selections
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
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));
    
    // Create modal dialog (resizable)
    bool dialog_open = true;
    if (ImGui::BeginPopupModal("Select System to Emulate", &dialog_open, 0)) {
        
        ImGui::Text("Choose a system configuration to emulate:");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Filtering options
        ImGui::PushItemWidth(300);
        ImGui::InputTextWithHint("##SearchFilter", "Search systems...", search_filter_, sizeof(search_filter_));
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::Checkbox("Search descriptions", &search_descriptions_);
        
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
                snprintf(filter_lower, sizeof(filter_lower), "%s", search_filter_);
                
                for (char* p = name_lower; *p; p++) *p = tolower(*p);
                for (char* p = filter_lower; *p; p++) *p = tolower(*p);
                
                bool name_matches = strstr(name_lower, filter_lower) != nullptr;
                bool desc_matches = false;
                
                // Only search descriptions if checkbox is enabled
                if (search_descriptions_) {
                    snprintf(desc_lower, sizeof(desc_lower), "%s", descriptor.description);
                    for (char* p = desc_lower; *p; p++) *p = tolower(*p);
                    desc_matches = strstr(desc_lower, filter_lower) != nullptr;
                }
                
                if (!name_matches && !desc_matches) {
                    passes_filter = false;
                }
            }
            
            // Video standard filter (0 = All, 1 = NTSC, 2 = PAL, 3 = PAL-M, 4 = SECAM)
            if (passes_filter && region_filter_ > 0) {
                VideoStandard target_standard = (VideoStandard)(region_filter_ - 1);
                bool has_match = false;
                for (const auto& std_cfg : hardware_traits.video_standard_configs) {
                    if (std_cfg.standard == target_standard) {
                        has_match = true;
                        break;
                    }
                }
                // Only filter out if system has standards but none match
                if (!has_match && !hardware_traits.video_standard_configs.empty()) {
                    passes_filter = false;
                }
            }
            
            // Skip if doesn't pass filters
            if (!passes_filter) {
                continue;
            }
            
            // Check if system has configuration options
            bool has_memory_options = !hardware_traits.memory_options.empty();
            bool has_video_standard_configs = !hardware_traits.video_standard_configs.empty();
            bool has_custom_options_for_config = !hardware_traits.custom_options.empty();
            bool has_configurations = has_memory_options || has_video_standard_configs || has_custom_options_for_config;
            
            ImGui::PushID((int)i);
            
            if (has_configurations) {
                // System with configurations - use tree node
                ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                bool node_open = ImGui::TreeNodeEx(descriptor.name, node_flags);
                // Clicking the tree node header itself selects the system with defaults
                if (ImGui::IsItemClicked() && !node_open) {
                    selected_system_index_ = (int)i;
                    selected_system_name_ = descriptor.short_name;
                    // Set default configurations
                    if (has_memory_options) {
                        for (size_t m = 0; m < hardware_traits.memory_options.size(); m++) {
                            if (hardware_traits.memory_options[m].is_default) {
                                selected_memory_option_ = (int)m;
                                break;
                            }
                        }
                    } else {
                        selected_memory_option_ = -1;
                    }
                    if (has_video_standard_configs) {
                        for (size_t r = 0; r < hardware_traits.video_standard_configs.size(); r++) {
                            if (hardware_traits.video_standard_configs[r].is_default) {
                                selected_region_option_ = (int)r;
                                break;
                            }
                        }
                    } else {
                        selected_region_option_ = -1;
                    }
                    // Initialize peripheral map with defaults
                    selected_peripherals_.clear();
                    for (const auto& periph_opt : hardware_traits.peripheral_options) {
                        selected_peripherals_[periph_opt.id] = periph_opt.enabled_by_default;
                    }
                    // Initialize custom settings with defaults
                    selected_custom_settings_.clear();
                    for (const auto& custom_opt : hardware_traits.custom_options) {
                        if (custom_opt.default_index >= 0 && custom_opt.default_index < (int)custom_opt.choices.size()) {
                            selected_custom_settings_[custom_opt.id] = custom_opt.choices[custom_opt.default_index];
                        }
                    }
                }
                
                // Show description as tooltip on system name
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", descriptor.description);
                    if (descriptor.supported_formats) {
                        ImGui::Separator();
                        ImGui::Text("Supported formats:");
                        for (const format_descriptor_t* const* fmt = descriptor.supported_formats; *fmt != nullptr; fmt++) {
                            ImGui::BulletText("%s (%s)", (*fmt)->name, (*fmt)->description);
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
                                if (selected_region_option_ < 0 && has_video_standard_configs) {
                                    for (size_t r = 0; r < hardware_traits.video_standard_configs.size(); r++) {
                                        if (hardware_traits.video_standard_configs[r].is_default) {
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
                    if (has_video_standard_configs) {
                        if (has_memory_options) ImGui::Spacing();
                        ImGui::TextDisabled("Video Region:");
                        ImGui::Indent();
                        for (size_t reg_idx = 0; reg_idx < hardware_traits.video_standard_configs.size(); reg_idx++) {
                            const auto& reg_opt = hardware_traits.video_standard_configs[reg_idx];
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
                    
                    // Show peripheral options if available
                    bool has_peripheral_options = !hardware_traits.peripheral_options.empty();
                    if (has_peripheral_options) {
                        if (has_memory_options || has_video_standard_configs) ImGui::Spacing();
                        ImGui::TextDisabled("Peripherals:");
                        ImGui::Indent();
                        
                        // Initialize peripheral map if this system is being selected for the first time
                        if (selected_system_index_ == (int)i && selected_peripherals_.empty()) {
                            for (const auto& periph_opt : hardware_traits.peripheral_options) {
                                selected_peripherals_[periph_opt.id] = periph_opt.enabled_by_default;
                            }
                        }
                        
                        for (size_t periph_idx = 0; periph_idx < hardware_traits.peripheral_options.size(); periph_idx++) {
                            const auto& periph_opt = hardware_traits.peripheral_options[periph_idx];
                            
                            // Get current checkbox state
                            bool is_enabled = selected_peripherals_[periph_opt.id];
                            
                            char label[256];
                            snprintf(label, sizeof(label), "%s##periph_%zu", periph_opt.name, periph_idx);
                            
                            if (ImGui::Checkbox(label, &is_enabled)) {
                                // Ensure system is selected when toggling peripherals
                                if (selected_system_index_ != (int)i) {
                                    selected_system_index_ = (int)i;
                                    selected_system_name_ = descriptor.short_name;
                                    // Set default memory and region if not selected
                                    if (selected_memory_option_ < 0 && has_memory_options) {
                                        for (size_t m = 0; m < hardware_traits.memory_options.size(); m++) {
                                            if (hardware_traits.memory_options[m].is_default) {
                                                selected_memory_option_ = (int)m;
                                                break;
                                            }
                                        }
                                    }
                                    if (selected_region_option_ < 0 && has_video_standard_configs) {
                                        for (size_t r = 0; r < hardware_traits.video_standard_configs.size(); r++) {
                                            if (hardware_traits.video_standard_configs[r].is_default) {
                                                selected_region_option_ = (int)r;
                                                break;
                                            }
                                        }
                                    }
                                }
                                // Update peripheral state
                                selected_peripherals_[periph_opt.id] = is_enabled;
                            }
                            
                            // Show description as tooltip
                            if (ImGui::IsItemHovered() && periph_opt.description) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s", periph_opt.description);
                                ImGui::EndTooltip();
                            }
                        }
                        ImGui::Unindent();
                    }
                    
                    // Show custom options if available
                    bool has_custom_options = !hardware_traits.custom_options.empty();
                    if (has_custom_options) {
                        // Initialize custom settings map if this system is being selected for the first time
                        if (selected_system_index_ == (int)i && selected_custom_settings_.empty()) {
                            for (const auto& custom_opt : hardware_traits.custom_options) {
                                if (custom_opt.default_index >= 0 && custom_opt.default_index < (int)custom_opt.choices.size()) {
                                    selected_custom_settings_[custom_opt.id] = custom_opt.choices[custom_opt.default_index];
                                }
                            }
                        }
                        
                        for (size_t cust_idx = 0; cust_idx < hardware_traits.custom_options.size(); cust_idx++) {
                            const auto& custom_opt = hardware_traits.custom_options[cust_idx];
                            if (has_memory_options || has_video_standard_configs || has_peripheral_options) ImGui::Spacing();
                            ImGui::TextDisabled("%s:", custom_opt.name);
                            ImGui::Indent();
                            
                            // Get current selection
                            std::string current_value;
                            auto it = selected_custom_settings_.find(custom_opt.id);
                            if (it != selected_custom_settings_.end()) {
                                current_value = it->second;
                            } else if (custom_opt.default_index >= 0 && custom_opt.default_index < (int)custom_opt.choices.size()) {
                                current_value = custom_opt.choices[custom_opt.default_index];
                            }
                            
                            for (size_t choice_idx = 0; choice_idx < custom_opt.choices.size(); choice_idx++) {
                                const char* choice = custom_opt.choices[choice_idx];
                                bool is_selected = (selected_system_index_ == (int)i && current_value == choice);
                                
                                char label[256];
                                snprintf(label, sizeof(label), "%s%s", choice,
                                       ((int)choice_idx == custom_opt.default_index) ? " (default)" : "");
                                
                                if (ImGui::Selectable(label, is_selected)) {
                                    selected_system_index_ = (int)i;
                                    selected_system_name_ = descriptor.short_name;
                                    selected_custom_settings_[custom_opt.id] = choice;
                                    // Set default memory/region if not selected
                                    if (selected_memory_option_ < 0 && has_memory_options) {
                                        for (size_t m = 0; m < hardware_traits.memory_options.size(); m++) {
                                            if (hardware_traits.memory_options[m].is_default) {
                                                selected_memory_option_ = (int)m;
                                                break;
                                            }
                                        }
                                    }
                                    if (selected_region_option_ < 0 && has_video_standard_configs) {
                                        for (size_t r = 0; r < hardware_traits.video_standard_configs.size(); r++) {
                                            if (hardware_traits.video_standard_configs[r].is_default) {
                                                selected_region_option_ = (int)r;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            ImGui::Unindent();
                            
                            // Show description as tooltip on the header
                            if (custom_opt.description && ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s", custom_opt.description);
                                ImGui::EndTooltip();
                            }
                        }
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
                    if (descriptor.supported_formats) {
                        ImGui::Separator();
                        ImGui::Text("Supported formats:");
                        for (const format_descriptor_t* const* fmt = descriptor.supported_formats; *fmt != nullptr; fmt++) {
                            ImGui::BulletText("%s (%s)", (*fmt)->name, (*fmt)->description);
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
