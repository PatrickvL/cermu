#pragma once

// ============================================================================
// IndexedStreamDecoder — GPU palette-indexed rendering as a SignalDecoder
// ============================================================================
//
// For systems whose video chip writes 8-bit palette indices to a CPU
// framebuffer instead of driving per-dot-clock stream samples.  The
// decoder uploads the index buffer as an R8 texture and performs the
// palette lookup in a fragment shader.
//
// Owns: R8 index texture, 256×1 palette texture.
// Inherits: shader, FBO + quad, render_to_texture, ImGui callback from base.
// ============================================================================

#include "gui/decoder/signal_decoder.hpp"
#include "gui/indexed_shader.hpp"

class IndexedStreamDecoder : public SignalDecoder {
public:
    IndexedStreamDecoder(int width, int height)
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

    void upload(const uint8_t* data, uint32_t /*sample_count*/) override {
        if (!index_tex_ || !data) return;
        glBindTexture(GL_TEXTURE_2D, index_tex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                        GL_RED, GL_UNSIGNED_BYTE, data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    // Indexed decoder has no sync events — update_uniforms is a no-op.
    void update_uniforms(const SyncEvent* /*sync*/, uint32_t /*sync_count*/,
                         int /*back_porch*/, int /*display_width*/,
                         uint32_t /*stream_len*/) override {}

    bool requires_fbo() const override { return true; }
    int display_height() const override { return height_; }
    bool ready() const override { return shader_ != 0 && index_tex_ != 0; }

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
    int width_  = 0;
    int height_ = 0;
    GLuint index_tex_ = 0;
};
