#pragma once

// ============================================================================
// DisplayPanel — post-processing stage applied to reconstructed video
// ============================================================================
//
// Sits after the signal decoder (or vector renderer) in the display chain.
// The decoder produces a texture containing the reconstructed screen image;
// the panel applies visual effects (CRT simulation, etc.) and produces a
// final output texture for ImGui display.
//
// Display chain:  VideoPort → DisplayPipeline → SignalDecoder → DisplayPanel
//
// Concrete panels:
//   CRTPanel    — barrel distortion, scanlines, shadow mask, phosphor tint
//   DirectPanel — identity pass-through (LCD / Direct Output)
// ============================================================================

#include "gui/shader/crt_shader.hpp"
#include "devices/display/display_device.hpp"  // DisplayCharacteristics, DisplayTechnology

#include <cstdint>

// ============================================================================
// Base class
// ============================================================================

class DisplayPanel {
public:
    virtual ~DisplayPanel() = default;

    /// Create GPU resources.  Returns true on success.
    virtual bool create(int initial_w, int initial_h) = 0;

    /// Release GPU resources.
    virtual void destroy() = 0;

    /// Apply post-processing to the input texture.
    /// Returns the output texture ID for ImGui display.
    /// For pass-through panels, returns input_tex unchanged.
    virtual GLuint render(GLuint input_tex,
                          float input_w, float input_h,
                          float output_w, float output_h,
                          const DisplayCharacteristics& dc) = 0;

    /// True when create() succeeded.
    virtual bool ready() const = 0;
};


// ============================================================================
// CRTPanel — full CRT post-processing
// ============================================================================

class CRTPanel : public DisplayPanel {
public:
    /// Construct with a non-owning reference to an existing CRTPostProcess.
    /// The caller is responsible for the CRTPostProcess lifecycle.
    /// Future: CRTPanel owns its own state and creates it in create().
    explicit CRTPanel(crt_shader::CRTPostProcess* state) : state_(state) {}

    bool create(int /*initial_w*/, int /*initial_h*/) override {
        return state_ && state_->shader != 0;
    }

    void destroy() override {
        // Non-owning — caller destroys the CRTPostProcess.
        state_ = nullptr;
    }

    GLuint render(GLuint input_tex,
                  float input_w, float input_h,
                  float output_w, float output_h,
                  const DisplayCharacteristics& dc) override {
        if (!state_ || !state_->shader) return input_tex;
        int mask = crt_shader::mask_type_from_technology(
            static_cast<int>(dc.technology));
        float pr, pg, pb;
        phosphor_tint_rgb(dc.phosphor, pr, pg, pb);
        crt_shader::render(state_, input_tex,
                           input_w, input_h,
                           output_w, output_h,
                           dc.curvature, dc.scanline_gap, dc.dot_pitch_mm,
                           dc.brightness, dc.contrast, dc.gamma, mask,
                           pr, pg, pb);
        return state_->texture;
    }

    bool ready() const override { return state_ && state_->shader != 0; }

private:
    crt_shader::CRTPostProcess* state_;  // Non-owning
};


// ============================================================================
// DirectPanel — identity pass-through (no post-processing)
// ============================================================================

class DirectPanel : public DisplayPanel {
public:
    bool create(int /*initial_w*/, int /*initial_h*/) override {
        ready_ = true;
        return true;
    }

    void destroy() override { ready_ = false; }

    GLuint render(GLuint input_tex,
                  float /*input_w*/, float /*input_h*/,
                  float /*output_w*/, float /*output_h*/,
                  const DisplayCharacteristics& /*dc*/) override {
        return input_tex;
    }

    bool ready() const override { return ready_; }

private:
    bool ready_ = false;
};
