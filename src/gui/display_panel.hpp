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
    CRTPanel() = default;
    ~CRTPanel() override { destroy(); }

    // Non-copyable, non-movable (owns GPU resources)
    CRTPanel(const CRTPanel&) = delete;
    CRTPanel& operator=(const CRTPanel&) = delete;

    bool create(int initial_w, int initial_h) override {
        return crt_shader::create(&state_, initial_w, initial_h);
    }

    void destroy() override {
        crt_shader::destroy(&state_);
    }

    GLuint render(GLuint input_tex,
                  float input_w, float input_h,
                  float output_w, float output_h,
                  const DisplayCharacteristics& dc) override {
        if (!state_.shader) return input_tex;
        crt_shader::render(&state_, input_tex,
                           input_w, input_h,
                           output_w, output_h,
                           dc);
        return state_.texture;
    }

    bool ready() const override { return state_.shader != 0; }

private:
    crt_shader::CRTPostProcess state_{};
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
