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
//      a. snapshot()         — emu thread copies stream data into decoder
//      b. upload_snapshot()  — GUI thread uploads to GPU + computes uniforms
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

    // ------------------------------------------------------------------
    // Snapshot / upload pipeline
    // ------------------------------------------------------------------
    // snapshot() — called by emu thread under fb_mutex_.
    // Copies stream/sync/metadata from FrameData into decoder-owned buffers.
    virtual void snapshot(const FrameData& fd, int fb_width) = 0;

    // upload_snapshot() — called by GUI thread under fb_mutex_.
    // Uploads internal snapshot data to GPU textures and computes uniforms.
    // Returns true if data was uploaded (i.e. snapshot was non-empty).
    virtual bool upload_snapshot() = 0;

    // Whether this decoder has pending snapshot data.
    bool has_snapshot() const { return has_snapshot_; }

    // Render decoded data into an internal FBO (template method).
    // Saves GL state, binds FBO, sets ortho projection, calls bind_textures(),
    // draws fullscreen quad, restores state.  Returns output texture ID.
    // Virtual so subclasses (e.g. VectorSignalDecoder) can replace the
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
        upload_ortho_projection(loc_proj_, 0, static_cast<float>(width),
                                static_cast<float>(height), 0);

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
    // NOTE: the 3-argument AddCallback copies `cb` by value (sized copy).
    void bind_for_imgui(ImDrawList* draw_list) {
        DecoderCallbackData cb = { shader_, loc_proj_, 0, 0 };
        fill_callback_textures(cb);
        draw_list->AddCallback(imgui_bind_callback, &cb, sizeof(cb));
    }

    // Whether this decoder ONLY works via the FBO path (render_to_texture).
    // Indexed and vector decoders return true — they always render to FBO.
    // Scanline decoders (Composite/RGB) return false — they support both
    // the inline ImGui path and the FBO path (for CRT post-processing).
    // Note: returning false does NOT mean the decoder lacks an FBO;
    // Composite/RGB still create one for optional CRT post-processing.
    virtual bool requires_fbo() const { return false; }

    // Number of visible scanlines (computed by update_uniforms).
    virtual int display_height() const = 0;

    GLuint program() const { return shader_; }
    GLuint output_texture() const { return fbo_tex_; }
    virtual bool ready() const = 0;

    /// Primary data texture — the texture the shader samples from TU0.
    /// For stream decoders this is the stream texture; for indexed, the
    /// index texture.  Used as the ImGui::Image texture ID on the inline
    /// (non-FBO) display path.
    virtual GLuint data_texture() const = 0;

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

    // --- Snapshot pending flag ---
    bool has_snapshot_ = false;

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

        QuadVertex quad[6] = {
            {0, 0,         0, 0, 0xFFFFFFFF}, {(float)w, 0,         1, 0, 0xFFFFFFFF},
            {(float)w, (float)h, 1, 1, 0xFFFFFFFF}, {0, 0,         0, 0, 0xFFFFFFFF},
            {(float)w, (float)h, 1, 1, 0xFFFFFFFF}, {0, (float)h,  0, 1, 0xFFFFFFFF},
        };
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        gl_api::glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // --- Ortho projection helper (shared by render_to_texture and ImGui callback) ---

    static void upload_ortho_projection(GLint loc, float L, float R, float T, float B) {
        const float ortho[4][4] = {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };
        gl_api::glUniformMatrix4fv(loc, 1, GL_FALSE, &ortho[0][0]);
    }

private:
    // Fullscreen quad vertex — used by create_fbo() and resize_fbo().
    struct QuadVertex { float x, y, u, v; uint32_t col; };

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
        upload_ortho_projection(d->loc_proj, L, R, T, B);

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
