#pragma once

// ============================================================================
// SignalDecoder — base class for GPU-based video signal reconstruction
// ============================================================================
//
// Each concrete decoder encapsulates the shader program, stream texture,
// FBO, and rendering logic for one signal family.  The decoder transforms
// raw stream bytes into a GPU texture containing the reconstructed image.
//
// Shared infrastructure (owned by this base class):
//   - FBO + fullscreen quad (create_fbo / destroy_fbo / resize_fbo)
//   - render_to_texture() template method (subclass only overrides bind_textures)
//   - Shader program + projection uniform location
//   - Optional palette texture (256×1 RGBA, created on demand)
//   - Unified ImGui draw callback
//
// Lifecycle:
//   1. Construct with signal-specific parameters
//   2. create() — compile shader, allocate stream texture + FBO
//   3. Per frame:
//      a. upload()          — upload raw stream bytes to GPU texture
//      b. update_uniforms() — compute scanline map, upload shader uniforms
//      c. render_to_texture() or bind_for_imgui() depending on display mode
//   4. destroy() — release GPU resources
// ============================================================================

#include "gui/gl_api.hpp"
#include "core/signal/sync_types.hpp"

#include <imgui.h>
#include <cstdint>

class SignalDecoder {
public:
    virtual ~SignalDecoder() = default;

    // Unified ImGui draw callback data — POD for ImDrawList::AddCallback.
    struct DecoderCallbackData {
        GLuint shader;
        GLint  loc_proj;
        GLuint tex0;        // Texture for unit 0 (0 = don't bind, ImGui provides)
        GLuint tex1;        // Texture for unit 1 (0 = don't bind)
    };

    // Resource lifecycle
    virtual bool create() = 0;
    virtual void destroy() = 0;

    // Upload raw stream bytes to GPU texture.
    virtual void upload(const uint8_t* data, uint32_t sample_count) = 0;

    // Compute scanline map from sync events and upload shader uniforms.
    virtual void update_uniforms(const SyncEvent* sync, uint32_t sync_count,
                                 int back_porch, int display_width,
                                 uint32_t stream_len) = 0;

    // Render decoded data into an internal FBO (template method).
    // Saves GL state, binds FBO, sets ortho projection, calls bind_textures(),
    // draws fullscreen quad, restores state.  Returns output texture ID.
    // Virtual so subclasses (e.g. VectorStreamDecoder) can replace the
    // fullscreen-quad approach entirely.
    virtual GLuint render_to_texture(int width, int height) {
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

        bind_textures();

        gl_api::glBindVertexArray(quad_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gl_api::glBindVertexArray(0);

        gl_api::glUseProgram(0);
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glViewport(prev_viewport[0], prev_viewport[1],
                   prev_viewport[2], prev_viewport[3]);

        return fbo_tex_;
    }

    // Add an ImGui draw callback that binds the shader + textures.
    void bind_for_imgui(ImDrawList* draw_list) {
        DecoderCallbackData cb = { shader_, loc_proj_, 0, 0 };
        fill_callback_textures(cb);
        draw_list->AddCallback(imgui_bind_callback, &cb, sizeof(cb));
    }

    // True if this decoder requires FBO rendering (render_to_texture path).
    virtual bool requires_fbo() const { return false; }

    // Number of visible scanlines (computed by update_uniforms).
    virtual int display_height() const = 0;

    GLuint program() const { return shader_; }
    GLuint output_texture() const { return fbo_tex_; }
    virtual bool ready() const = 0;

    // Upload palette data (RGBA, up to 256 entries).
    // Default implementation works for any decoder with palette_texture_ set.
    virtual void upload_palette(const uint32_t* palette, int count) {
        if (!palette_texture_ || !palette || count <= 0) return;
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, count, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, palette);
    }

    GLuint palette_texture() const { return palette_texture_; }

protected:
    // --- Shader state (set by concrete create()) ---
    GLuint shader_    = 0;
    GLint  loc_proj_  = -1;

    // --- Optional palette texture ---
    GLuint palette_texture_ = 0;

    // Bind signal-specific textures to texture units.
    // Called inside render_to_texture() after shader + projection are set.
    virtual void bind_textures() = 0;

    // Fill the callback data with textures for the ImGui bind callback.
    // Default: tex0 = 0 (ImGui provides the Image texture), tex1 = 0.
    virtual void fill_callback_textures(DecoderCallbackData& cb) const {
        (void)cb;
    }

    // --- Palette texture helpers ---

    // Create a 256×1 RGBA palette texture (nearest filtering).
    void create_palette_texture() {
        glGenTextures(1, &palette_texture_);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy_palette_texture() {
        if (palette_texture_) {
            glDeleteTextures(1, &palette_texture_);
            palette_texture_ = 0;
        }
    }

    // --- FBO lifecycle ---

    void create_fbo() {
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

    void destroy_fbo() {
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

private:
    // FBO resources
    GLuint fbo_       = 0;
    GLuint fbo_tex_   = 0;
    GLuint quad_vao_  = 0;
    GLuint quad_vbo_  = 0;
    int    fbo_w_     = 0;
    int    fbo_h_     = 0;

    static void imgui_bind_callback(const ImDrawList*, const ImDrawCmd* cmd) {
        auto* d = static_cast<const DecoderCallbackData*>(cmd->UserCallbackData);
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

        if (d->tex0) {
            gl_api::glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d->tex0);
        }
        if (d->tex1) {
            gl_api::glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, d->tex1);
            gl_api::glActiveTexture(GL_TEXTURE0);
        }
    }
};
