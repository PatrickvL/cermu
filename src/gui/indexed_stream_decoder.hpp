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
// Owns: shader program, R8 index texture, 256×1 palette texture, FBO + quad.
// ============================================================================

#include "gui/signal_decoder.hpp"
#include "gui/indexed_shader.hpp"

#include <imgui.h>

// ImGui draw callback data — must be POD for ImDrawList::AddCallback.
struct IndexedCallbackData {
    GLuint shader;
    GLint  loc_proj;
    GLuint palette_tex;
};

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

        // Palette texture (256×1 RGBA, nearest filtering)
        glGenTextures(1, &palette_texture_);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Shader
        shader_ = indexed_shader::create_program(&loc_proj_, nullptr);
        if (!shader_) {
            destroy();
            return false;
        }

        // FBO + quad
        create_fbo_resources();
        return true;
    }

    void destroy() override {
        destroy_fbo_resources();
        if (shader_)      { gl_api::glDeleteProgram(shader_); shader_ = 0; }
        if (index_tex_)   { glDeleteTextures(1, &index_tex_); index_tex_ = 0; }
        if (palette_texture_) { glDeleteTextures(1, &palette_texture_); palette_texture_ = 0; }
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

    // Upload palette data to the owned palette texture.
    void upload_palette(const uint32_t* palette, int count) override {
        if (!palette_texture_ || !palette || count <= 0) return;
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, count, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, palette);
    }
    GLuint palette_texture() const override { return palette_texture_; }

    GLuint render_to_texture(int width, int height) override {
        if (!fbo_ || !fbo_tex_) return 0;
        resize_fbo(width, height);

        GLint prev_fbo = 0, prev_viewport[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glGetIntegerv(GL_VIEWPORT, prev_viewport);

        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        gl_api::glUseProgram(shader_);
        float L = 0, R = static_cast<float>(width);
        float T = static_cast<float>(height), B = 0;
        const float ortho[4][4] = {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };
        gl_api::glUniformMatrix4fv(loc_proj_, 1, GL_FALSE, &ortho[0][0]);

        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, index_tex_);
        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        gl_api::glActiveTexture(GL_TEXTURE0);

        gl_api::glBindVertexArray(quad_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gl_api::glBindVertexArray(0);

        gl_api::glUseProgram(0);
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glViewport(prev_viewport[0], prev_viewport[1],
                   prev_viewport[2], prev_viewport[3]);

        return fbo_tex_;
    }

    void bind_for_imgui(ImDrawList* draw_list) override {
        IndexedCallbackData cb = { shader_, loc_proj_, palette_texture_ };
        draw_list->AddCallback(imgui_bind_callback, &cb, sizeof(cb));
    }

    bool requires_fbo() const override { return true; }
    int display_height() const override { return height_; }
    GLuint program() const override { return shader_; }
    GLuint output_texture() const override { return fbo_tex_; }
    bool ready() const override { return shader_ != 0 && index_tex_ != 0; }

    GLuint index_texture() const { return index_tex_; }

private:
    int width_  = 0;
    int height_ = 0;

    GLuint shader_      = 0;
    GLuint index_tex_   = 0;
    GLuint palette_texture_ = 0;  // Owned
    GLint  loc_proj_    = -1;

    // FBO resources (owned)
    GLuint fbo_       = 0;
    GLuint fbo_tex_   = 0;
    GLuint quad_vao_  = 0;
    GLuint quad_vbo_  = 0;
    int    fbo_w_     = 0;
    int    fbo_h_     = 0;

    void create_fbo_resources() {
        glGenTextures(1, &fbo_tex_);
        glBindTexture(GL_TEXTURE_2D, fbo_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        gl_api::glGenFramebuffers(1, &fbo_);
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        gl_api::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, fbo_tex_, 0);
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, 0);

        struct QuadVertex { float x, y, u, v; uint32_t col; };
        QuadVertex quad[6] = {
            {0, 0, 0, 0, 0xFFFFFFFF}, {1, 0, 1, 0, 0xFFFFFFFF},
            {1, 1, 1, 1, 0xFFFFFFFF}, {0, 0, 0, 0, 0xFFFFFFFF},
            {1, 1, 1, 1, 0xFFFFFFFF}, {0, 1, 0, 1, 0xFFFFFFFF},
        };
        gl_api::glGenVertexArrays(1, &quad_vao_);
        gl_api::glGenBuffers(1, &quad_vbo_);
        gl_api::glBindVertexArray(quad_vao_);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        gl_api::glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
        gl_api::glEnableVertexAttribArray(0);
        gl_api::glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                                      sizeof(QuadVertex), (void*)0);
        gl_api::glEnableVertexAttribArray(1);
        gl_api::glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                                      sizeof(QuadVertex), (void*)(2 * sizeof(float)));
        gl_api::glEnableVertexAttribArray(2);
        gl_api::glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                                      sizeof(QuadVertex), (void*)(4 * sizeof(float)));
        gl_api::glBindVertexArray(0);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);
        fbo_w_ = 0;
        fbo_h_ = 0;
    }

    void destroy_fbo_resources() {
        if (fbo_)      { gl_api::glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
        if (fbo_tex_)  { glDeleteTextures(1, &fbo_tex_); fbo_tex_ = 0; }
        if (quad_vao_) { gl_api::glDeleteVertexArrays(1, &quad_vao_); quad_vao_ = 0; }
        if (quad_vbo_) { gl_api::glDeleteBuffers(1, &quad_vbo_); quad_vbo_ = 0; }
        fbo_w_ = fbo_h_ = 0;
    }

    void resize_fbo(int w, int h) {
        if (w == fbo_w_ && h == fbo_h_) return;
        fbo_w_ = w;
        fbo_h_ = h;
        glBindTexture(GL_TEXTURE_2D, fbo_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        struct QuadVertex { float x, y, u, v; uint32_t col; };
        QuadVertex quad[6] = {
            {0, 0,         0, 0, 0xFFFFFFFF}, {(float)w, 0,         1, 0, 0xFFFFFFFF},
            {(float)w, (float)h, 1, 1, 0xFFFFFFFF}, {0, 0,         0, 0, 0xFFFFFFFF},
            {(float)w, (float)h, 1, 1, 0xFFFFFFFF}, {0, (float)h,  0, 1, 0xFFFFFFFF},
        };
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        gl_api::glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    static void imgui_bind_callback(const ImDrawList*, const ImDrawCmd* cmd) {
        auto* d = static_cast<const IndexedCallbackData*>(cmd->UserCallbackData);
        gl_api::glUseProgram(d->shader);

        ImDrawData* draw_data = ImGui::GetDrawData();
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        if (R == L || B == T) return;
        const float ortho[4][4] = {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };
        gl_api::glUniformMatrix4fv(d->loc_proj, 1, GL_FALSE, &ortho[0][0]);

        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, d->palette_tex);
        gl_api::glActiveTexture(GL_TEXTURE0);
    }
};
