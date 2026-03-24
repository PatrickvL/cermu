#pragma once

// ============================================================================
// SignalDecoder — base class for GPU-based video signal reconstruction
// ============================================================================
//
// Each concrete decoder encapsulates the shader program, stream texture,
// FBO, and rendering logic for one signal family.  The decoder transforms
// raw stream bytes into a GPU texture containing the reconstructed image.
//
// Two rendering paths:
//   requires_fbo() == true:
//     render_to_texture() — renders into an internal FBO and returns the
//     output texture ID.  Used when CRT post-processing or other effects
//     need a texture as input.
//
//   requires_fbo() == false:
//     bind_for_imgui() — adds an ImGui draw callback that binds the
//     shader + textures for inline rendering through ImGui's draw list.
//     The ImGui::Image call uses the stream texture directly.
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

#include <cstdint>

struct ImDrawList;

class SignalDecoder {
public:
    virtual ~SignalDecoder() = default;

    // Resource lifecycle
    virtual bool create() = 0;
    virtual void destroy() = 0;

    // Upload raw stream bytes to GPU texture.
    // data: raw sample bytes (format depends on signal type).
    // sample_count: number of samples.
    virtual void upload(const uint8_t* data, uint32_t sample_count) = 0;

    // Compute scanline map from sync events and upload shader uniforms.
    // Called after upload(), outside any mutex.
    virtual void update_uniforms(const SyncEvent* sync, uint32_t sync_count,
                                 int back_porch, int display_width,
                                 uint32_t stream_len) = 0;

    // Render decoded data into an internal FBO.
    // Returns the output texture ID containing the reconstructed image.
    // width/height: desired output dimensions (FBO resized if needed).
    virtual GLuint render_to_texture(int width, int height) = 0;

    // Add an ImGui draw callback that binds the shader + textures.
    // Used for non-FBO direct rendering through ImGui's draw list.
    // Only meaningful when requires_fbo() returns false.
    virtual void bind_for_imgui(ImDrawList* draw_list) = 0;

    // True if this decoder requires FBO rendering (render_to_texture path).
    // False if it can render inline through ImGui's draw list.
    // Decoders like Vector always require an FBO; stream decoders may
    // use the inline path when no CRT post-processing is active.
    virtual bool requires_fbo() const { return false; }

    // Number of visible scanlines (computed by update_uniforms).
    virtual int display_height() const = 0;

    // The GL shader program (for callers that need it).
    virtual GLuint program() const = 0;

    // The output texture of the internal FBO (valid after render_to_texture).
    virtual GLuint output_texture() const = 0;

    // True when create() succeeded and resources are valid.
    virtual bool ready() const = 0;

    // Upload palette data (RGBA, up to 256 entries).
    // Meaningful for indexed and composite decoders; default no-op for others.
    virtual void upload_palette(const uint32_t* /*palette*/, int /*count*/) {}

    // Palette texture (256×1 RGBA) owned or referenced by this decoder.
    // Returns 0 if the decoder doesn't use a palette.
    virtual GLuint palette_texture() const { return 0; }
};
