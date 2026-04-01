#pragma once

// ============================================================================
// VectorSignalDecoder — GPU beam-quad rendering for vector display systems
// ============================================================================
//
// Encapsulates the full GPU pipeline for vector displays (Atari DVG/AVG,
// Vectrex, etc.):
//
//   - Beam shader program (vertex + fragment)
//   - VAO / VBO for beam quad vertices
//   - Phosphor persistence FBO with per-channel decay
//   - CPU-side beam quad expansion from VectorVideoSample stream
//
// Unlike raster decoders, there is no signal texture.  Raw VectorVideoSample
// bytes are stored via upload(), and render_to_texture() builds beam quads
// on the CPU, then renders them into the persistence FBO with additive
// blending on top of the decayed previous frame.
//
// The override of render_to_texture() replaces the base class fullscreen-quad
// approach with the two-pass persistence rendering (decay + additive beams).
// ============================================================================

#include "core/cermu.hpp"
#include "gui/decoder/signal_decoder.hpp"
#include "gui/shader/vector_shader.hpp"

#include <vector>
#include <cstring>
#include <algorithm>

class VectorSignalDecoder : public SignalDecoder {
public:
    /// Construct with hardware coordinate space dimensions (for beam scaling).
    VectorSignalDecoder(int hw_width, int hw_height)
        : hw_width_(hw_width), hw_height_(hw_height) {}

    ~VectorSignalDecoder() override { destroy(); }

    // ---- SignalDecoder interface ----

    bool create() override {
        vector_shader::VectorShaderLocations vlocs{};
        shader_ = vector_shader::create_program(&vlocs);
        if (!shader_) return false;

        loc_proj_ = vlocs.proj_mtx;
        loc_phosphor_ = vlocs.phosphor_color;

        // Create VAO/VBO with BeamVertex attribute layout
        gl_api::glGenVertexArrays(1, &vao_);
        gl_api::glGenBuffers(1, &vbo_);
        gl_api::glBindVertexArray(vao_);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, vbo_);

        // Position (location 0): 2 floats at offset 0
        gl_api::glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            sizeof(vector_shader::BeamVertex), reinterpret_cast<void*>(0));
        gl_api::glEnableVertexAttribArray(0);
        // Intensity (location 1): 1 float at offset 8
        gl_api::glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE,
            sizeof(vector_shader::BeamVertex), reinterpret_cast<void*>(8));
        gl_api::glEnableVertexAttribArray(1);
        // DistFromCenter (location 2): 1 float at offset 12
        gl_api::glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
            sizeof(vector_shader::BeamVertex), reinterpret_cast<void*>(12));
        gl_api::glEnableVertexAttribArray(2);
        // Color (location 3): 3 floats at offset 16
        gl_api::glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE,
            sizeof(vector_shader::BeamVertex), reinterpret_cast<void*>(16));
        gl_api::glEnableVertexAttribArray(3);

        gl_api::glBindVertexArray(0);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Create phosphor persistence FBO
        int init_w = hw_width_  > 0 ? hw_width_  : 1024;
        int init_h = hw_height_ > 0 ? hw_height_ : 1024;
        if (!vector_shader::create_persistence(&persist_, init_w, init_h)) {
            destroy();
            return false;
        }

        log_info("GPU vector display rendering enabled\n");
        return true;
    }

    void destroy() override {
        if (shader_) { gl_api::glDeleteProgram(shader_); shader_ = 0; }
        if (vao_)    { gl_api::glDeleteVertexArrays(1, &vao_); vao_ = 0; }
        if (vbo_)    { gl_api::glDeleteBuffers(1, &vbo_); vbo_ = 0; }
        vector_shader::destroy_persistence(&persist_);
        loc_proj_ = -1;
        loc_phosphor_ = -1;
        beam_buf_.clear();
        sample_buf_.clear();
        sample_count_ = 0;
    }

    void snapshot(const FrameData& fd, int /*fb_width*/) override {
        if (!fd.signal_output || fd.signal_output_len == 0) { has_snapshot_ = false; return; }
        const uint8_t* src = static_cast<const uint8_t*>(fd.signal_output);
        uint32_t n = std::min(fd.signal_output_len, MAX_SIGNAL_SAMPLES);
        size_t bytes = static_cast<size_t>(n) * 8;
        if (sample_buf_.size() < bytes)
            sample_buf_.resize(bytes);
        std::memcpy(sample_buf_.data(), src, bytes);
        sample_count_ = n;
        has_snapshot_ = true;
    }

    bool upload_snapshot() override {
        // Vector has no GPU texture upload — data is consumed by render_to_texture().
        // Just acknowledge the snapshot.
        if (!has_snapshot_) return false;
        has_snapshot_ = false;
        return true;
    }

    /// Set per-frame rendering parameters.
    /// Call this before render_to_texture() each frame.
    void set_frame_params(float frame_dt,
                          float phosphor_r, float phosphor_g, float phosphor_b,
                          const float* color_palette) {
        frame_dt_   = frame_dt;
        phosphor_r_ = phosphor_r;
        phosphor_g_ = phosphor_g;
        phosphor_b_ = phosphor_b;
        color_palette_ = color_palette;
    }

    /// Override: render persistence frame instead of fullscreen quad.
    /// width/height are the display dimensions (not hw dimensions).
    GLuint render_to_texture(int width, int height) override {
        if (!persist_.fbo || !shader_) return 0;

        // Resize persistence FBO to match display dimensions
        if (width > 0 && height > 0)
            vector_shader::resize_persistence(&persist_, width, height);

        // Build beam quads from stored sample data
        if (sample_count_ > 0) {
            float display_w = static_cast<float>(width);
            float display_h = static_cast<float>(height);
            float x_scale = display_w / static_cast<float>(hw_width_);
            float y_scale = display_h / static_cast<float>(hw_height_);
            float beam_w = 3.0f * (std::min(display_w, display_h) / 1024.0f);
            beam_w = std::max(1.5f, std::min(beam_w, 6.0f));
            vector_shader::build_beam_quads(
                sample_buf_.data(), sample_count_,
                beam_w,
                display_w, display_h,
                x_scale, y_scale,
                0.0f, 0.0f,
                color_palette_,
                beam_buf_);
        } else {
            beam_buf_.clear();
        }

        // Render persistence pass: decay previous frame + add new beam quads
        vector_shader::render_persistence_frame(
            &persist_,
            shader_, loc_proj_, loc_phosphor_,
            vao_, vbo_,
            beam_buf_.data(), static_cast<int>(beam_buf_.size()),
            frame_dt_,
            phosphor_r_, phosphor_g_, phosphor_b_);

        return persist_.texture;
    }

    bool requires_fbo() const override { return true; }
    int display_height() const override { return persist_.height; }
    bool ready() const override { return shader_ != 0 && persist_.fbo != 0; }
    GLuint data_texture() const override { return persist_.texture; }

    GLuint output_texture() const { return persist_.texture; }

protected:
    // bind_textures() is never called (render_to_texture is fully overridden)
    void bind_textures() override {}

private:
    int hw_width_  = 0;
    int hw_height_ = 0;

    // Beam shader resources
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLint  loc_phosphor_ = -1;

    // Phosphor persistence
    vector_shader::PhosphorPersistence persist_{};

    // Stored sample data (copied from host snapshot buffer)
    std::vector<uint8_t> sample_buf_;
    uint32_t sample_count_ = 0;

    // Per-frame rendering parameters
    float frame_dt_   = 0.016f;
    float phosphor_r_ = 0.2f;
    float phosphor_g_ = 1.0f;
    float phosphor_b_ = 0.2f;
    const float* color_palette_ = nullptr;

    // CPU-expanded beam quad vertices
    std::vector<vector_shader::BeamVertex> beam_buf_;
};
