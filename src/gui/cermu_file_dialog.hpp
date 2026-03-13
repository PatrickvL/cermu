#pragma once

/**
 * CermuFileDialog — Subclass of IGFD::FileDialog with cermu UI tweaks.
 *
 * Cermu wraps the file dialog in its own ImGui::Begin/End using the
 * NoDialog flag, giving us a title-bar X button for free.  This
 * subclass hides the redundant Cancel button by overriding the
 * virtual m_DrawCancelButton().
 *
 * It also overrides m_SelectableItem() so that archive/container
 * entries (presented as directories by VfsFileSystem) can be both
 * selected (single-click → name goes into the filename field) and
 * browsed into (double-click → navigates inside).  This lets the
 * receiving code decide how to handle a container path without
 * modifying the ImGuiFileDialog submodule.
 *
 * This avoids forking ImGuiFileDialog for a trivial UI preference.
 */

#include <ImGuiFileDialog.h>
#include "gui/vfs_file_system.hpp"

#include <cstdarg>

namespace cermu {

class CermuFileDialog : public IGFD::FileDialog {
protected:
    /// Hide the Cancel button — close is handled via the title bar X button.
    /// Still honour needToExitDialog (Escape key, programmatic close).
    bool m_DrawCancelButton() override {
        if (m_FileDialogInternal.needToExitDialog) {
            m_FileDialogInternal.isOk = false;
            return true;
        }
        return false;
    }

    /// Override selectable-item click behavior for container/archive
    /// directories.  Stock IGFD navigates into directories on any click;
    /// we intercept browsable containers so that:
    ///   - single-click  → selects the entry (filename field, OK enabled)
    ///   - double-click  → navigates into the container (stock behavior)
    /// Regular directories and files keep stock IGFD behavior.
    void m_SelectableItem(int vRowIdx,
                          std::shared_ptr<IGFD::FileInfos> vInfos,
                          bool vSelected,
                          const char* vFmt, ...) override {
        if (!vInfos.use_count()) return;

        auto& fdi = m_FileDialogInternal.fileManager;

        static ImGuiSelectableFlags flags =
            ImGuiSelectableFlags_AllowDoubleClick |
            ImGuiSelectableFlags_SpanAllColumns |
            static_cast<ImGuiSelectableFlags>(1 << 24);  // SpanAvailWidth (imgui internal)

        va_list args;
        va_start(args, vFmt);
        vsnprintf(fdi.variadicBuffer, MAX_FILE_DIALOG_NAME_BUFFER, vFmt, args);
        va_end(args);

        if (m_Selectable(vRowIdx, fdi.variadicBuffer, vSelected, flags,
                         ImVec2(-1.0f, 0.0f))) {
            if (vInfos->fileType.isDir()) {
                // --- cermu: selectable container/archive directories ---
                // Browsable containers (archives, D64, T64, …) presented as
                // directories can be selected without browsing in.
                if (vInfos->fileNameExt != ".." &&
                    vInfos->fileNameExt != "." &&
                    VfsFileSystem::is_browsable(vInfos->fileNameExt)) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        fdi.pathClicked = fdi.SelectDirectory(vInfos);
                    } else {
                        fdi.SelectOrDeselectFileName(m_FileDialogInternal,
                                                     vInfos);
                    }
                }
                // --- stock IGFD behavior for regular directories ---
                // With NavEnableKeyboard, single-click navigates into
                // directories (IGFD's "little fix for mouse behavior in
                // nav system").  Guard with !pathClicked so the double-
                // click event that may fire in the same frame can't
                // overwrite a successful navigation with a failed one.
                else if (ImGui::GetIO().ConfigFlags &
                         ImGuiConfigFlags_NavEnableKeyboard) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        if (!fdi.pathClicked)
                            fdi.pathClicked = fdi.SelectDirectory(vInfos);
                    } else if (fdi.dLGDirectoryMode) {
                        fdi.SelectOrDeselectFileName(m_FileDialogInternal,
                                                     vInfos);
                    } else {
                        if (!fdi.pathClicked)
                            fdi.pathClicked = fdi.SelectDirectory(vInfos);
                    }
                } else {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        fdi.pathClicked = fdi.SelectDirectory(vInfos);
                    } else if (fdi.dLGDirectoryMode) {
                        fdi.SelectOrDeselectFileName(m_FileDialogInternal,
                                                     vInfos);
                    }
                }
            } else {
                fdi.SelectOrDeselectFileName(m_FileDialogInternal, vInfos);
                if (ImGui::IsMouseDoubleClicked(0)) {
                    m_FileDialogInternal.isOk = true;
                }
            }
        }
    }
};

/// Convenience accessor — drop-in replacement for ImGuiFileDialog::Instance().
inline CermuFileDialog* FileDialogInstance() {
    static CermuFileDialog instance;
    return &instance;
}

}  // namespace cermu
