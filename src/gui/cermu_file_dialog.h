#pragma once

/**
 * CermuFileDialog — Subclass of IGFD::FileDialog with cermu UI tweaks.
 *
 * Cermu wraps the file dialog in its own ImGui::Begin/End using the
 * NoDialog flag, giving us a title-bar X button for free.  This
 * subclass hides the redundant Cancel button by overriding the
 * virtual m_DrawCancelButton().
 *
 * This avoids forking ImGuiFileDialog for a trivial UI preference.
 */

#include "ImGuiFileDialog.h"

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
};

/// Convenience accessor — drop-in replacement for ImGuiFileDialog::Instance().
inline CermuFileDialog* FileDialogInstance() {
    static CermuFileDialog instance;
    return &instance;
}

}  // namespace cermu
