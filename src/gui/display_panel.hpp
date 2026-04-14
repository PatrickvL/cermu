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
//   LCDPanel    — pixel grid, subpixels, ghosting, backlight effects
//   DirectPanel — identity pass-through (no post-processing)
// ============================================================================

#include "gui/shader/crt_shader.hpp"
#include "gui/shader/lcd_shader.hpp"
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
    /// @param rotation  DisplayRotation enum value (0=none, 1=CW90, 2=CW180, 3=CW270)
    virtual GLuint render(GLuint input_tex,
                          float input_w, float input_h,
                          float output_w, float output_h,
                          const DisplayCharacteristics& dc,
                          int rotation = 0) = 0;

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
                  const DisplayCharacteristics& dc,
                  int rotation = 0) override {
        if (!state_.shader) return input_tex;
        crt_shader::render(&state_, input_tex,
                           input_w, input_h,
                           output_w, output_h,
                           dc, rotation);
        return state_.texture;
    }

    bool ready() const override { return state_.shader != 0; }

private:
    crt_shader::CRTPostProcess state_{};
};


// ============================================================================
// LCDPanel — LCD post-processing (pixel grid, ghosting, backlight)
// ============================================================================

class LCDPanel : public DisplayPanel {
public:
    LCDPanel() = default;
    ~LCDPanel() override { destroy(); }

    // Non-copyable, non-movable (owns GPU resources)
    LCDPanel(const LCDPanel&) = delete;
    LCDPanel& operator=(const LCDPanel&) = delete;

    bool create(int initial_w, int initial_h) override {
        return lcd_shader::create(&state_, initial_w, initial_h);
    }

    void destroy() override {
        lcd_shader::destroy(&state_);
    }

    GLuint render(GLuint input_tex,
                  float input_w, float input_h,
                  float output_w, float output_h,
                  const DisplayCharacteristics& dc,
                  int /*rotation*/ = 0) override {
        if (!state_.shader) return input_tex;
        lcd_shader::render(&state_, input_tex,
                           input_w, input_h,
                           output_w, output_h,
                           dc);
        return state_.texture;
    }

    bool ready() const override { return state_.shader != 0; }

private:
    lcd_shader::LCDPostProcess state_{};
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
                  const DisplayCharacteristics& /*dc*/,
                  int /*rotation*/ = 0) override {
        return input_tex;
    }

    bool ready() const override { return ready_; }

private:
    bool ready_ = false;
};
