#pragma once

// ============================================================================
// IndexedSignalDecoder — GPU palette-indexed rendering as a SignalDecoder
// ============================================================================
//
// For systems whose video chip writes 8-bit palette indices to a CPU
// framebuffer instead of driving per-dot-clock signal samples.  The
// decoder uploads the index buffer as an R8 texture and performs the
// palette lookup in a fragment shader.
//
// Owns: R8 index texture, 256×1 palette texture.
// Inherits: shader, FBO + quad, render_to_texture, ImGui callback from base.
// ============================================================================

#include "gui/decoder/signal_decoder.hpp"
#include "gui/shader/indexed_shader.hpp"

#include <cstring>
#include <memory>

class IndexedSignalDecoder : public SignalDecoder {
public:
    IndexedSignalDecoder(int width, int height)
        : width_(width), height_(height) {}

    bool create() override {
        // Index texture (R8, nearest filtering for pixel-perfect indices)
        glGenTextures(1, &index_tex_);
        glBindTexture(GL_TEXTURE_2D, index_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0,
                     GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        create_palette_texture();

        shader_ = indexed_shader::create_program(&loc_proj_, nullptr);
        if (!shader_) {
            destroy();
            return false;
        }

        create_fbo();
        return true;
    }

    void destroy() override {
        destroy_fbo();
        if (shader_)    { gl_api::glDeleteProgram(shader_); shader_ = 0; }
        if (index_tex_) { glDeleteTextures(1, &index_tex_); index_tex_ = 0; }
        destroy_palette_texture();
    }

    // Indexed decoder: snapshot copies from the live index framebuffer.
    void snapshot(const FrameData& /*fd*/, int /*fb_width*/) override {
        // Indexed path doesn't use FrameData streams — use snapshot_index().
    }

    void snapshot_index(const uint8_t* index_buf, int w, int h) {
        if (!index_buf || !index_snapshot_) return;
        std::memcpy(index_snapshot_.get(), index_buf,
                    static_cast<size_t>(w) * h);
        has_snapshot_ = true;
    }

    bool upload_snapshot() override {
        if (!has_snapshot_) return false;
        has_snapshot_ = false;
        upload_index_texture(index_snapshot_.get());
        return true;
    }

    /// Allocate the index buffers (live + snapshot).  Returns the live
    /// buffer pointer to hand to the system via set_index_buffer().
    uint8_t* allocate_index_buffers() {
        size_t bytes = static_cast<size_t>(width_) * height_;
        index_framebuffer_ = std::make_unique<uint8_t[]>(bytes);
        index_snapshot_    = std::make_unique<uint8_t[]>(bytes);
        std::memset(index_framebuffer_.get(), 0, bytes);
        std::memset(index_snapshot_.get(), 0, bytes);
        return index_framebuffer_.get();
    }

    /// Non-owning pointer to the live index buffer (emu thread writes here).
    uint8_t* index_framebuffer() const { return index_framebuffer_.get(); }

    bool requires_fbo() const override { return true; }
    int display_height() const override { return height_; }
    bool ready() const override { return shader_ != 0 && index_tex_ != 0; }
    GLuint data_texture() const override { return index_tex_; }

    GLuint index_texture() const { return index_tex_; }

protected:
    void bind_textures() override {
        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, index_tex_);
        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        gl_api::glActiveTexture(GL_TEXTURE0);
    }

    void fill_callback_textures(DecoderCallbackData& cb) const override {
        // TU0 = ImGui provides index texture, TU1 = palette
        cb.tex1 = palette_texture_;
    }

private:
    void upload_index_texture(const uint8_t* data) {
        if (!index_tex_ || !data) return;
        glBindTexture(GL_TEXTURE_2D, index_tex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                        GL_RED, GL_UNSIGNED_BYTE, data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    int width_  = 0;
    int height_ = 0;
    GLuint index_tex_ = 0;

    // Owned index buffers (live + snapshot for double-buffering)
    std::unique_ptr<uint8_t[]> index_framebuffer_;
    std::unique_ptr<uint8_t[]> index_snapshot_;
};
