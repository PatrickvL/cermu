#include "system_selection_dialog.h"
#include "../core/emulated_system.h"
#include "../core/system_registry.h"
#include "imgui.h"
#include <stdio.h>

SystemSelectionDialog::SystemSelectionDialog()
    : is_open_(false)
    , selected_system_index_(-1)
    , selected_system_name_(nullptr)
    , selection_confirmed_(false)
{
}

void SystemSelectionDialog::open() {
    is_open_ = true;
    selected_system_index_ = -1;
    selected_system_name_ = nullptr;
    selection_confirmed_ = false;
}

void SystemSelectionDialog::close() {
    is_open_ = false;
}

void SystemSelectionDialog::reset() {
    selected_system_index_ = -1;
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
        
        ImGui::Text("Choose a system to emulate:");
        ImGui::Separator();
        ImGui::Spacing();
        
        // System list with selectables
        ImGui::BeginChild("SystemList", ImVec2(0, -40), true);
        for (size_t i = 0; i < systems.size(); i++) {
            const auto& [descriptor, factory] = systems[i];
            
            bool is_selected = (selected_system_index_ == (int)i);
            
            // Selectable for system
            if (ImGui::Selectable(descriptor.name, is_selected, 0, ImVec2(0, 0))) {
                selected_system_index_ = (int)i;
                selected_system_name_ = descriptor.short_name;
            }
            
            // Double-click to select and confirm
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                selection_confirmed_ = true;
                close();
                ImGui::CloseCurrentPopup();
            }
            
            // Show description as tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", descriptor.description);
                
                // Show supported file extensions
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
