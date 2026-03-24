#pragma once

// ============================================================================
// SignalDecoder — base class for GPU-based video signal reconstruction
// ============================================================================
//
// Each concrete decoder encapsulates the shader program, stream texture,
// scanline map computation, and rendering logic for one signal family.
// The decoder does NOT own shared resources like the palette texture or
// signal FBO — those are passed in by the host.
//
// Lifecycle:
//   1. Construct with signal-specific parameters
//   2. create() — compile shader, allocate stream texture
//   3. Per frame:
//      a. upload()          — upload raw stream bytes to GPU texture
//      b. update_uniforms() — compute scanline map, upload shader uniforms
//      c. bind_for_fbo()    — bind shader+textures for FBO rendering
//           or
//         bind_for_imgui()  — add ImGui draw callback for direct rendering
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

    // Bind shader + textures for FBO rendering.
    // The FBO must already be bound by the caller.
    // width/height: FBO dimensions for ortho projection.
    virtual void bind_for_fbo(int width, int height) = 0;

    // Add an ImGui draw callback that binds the shader + textures.
    // Used for non-FBO direct rendering through ImGui's draw list.
    virtual void bind_for_imgui(ImDrawList* draw_list) = 0;

    // Restore default shader state after rendering.
    virtual void unbind() { gl_api::glUseProgram(0); }

    // Number of visible scanlines (computed by update_uniforms).
    virtual int display_height() const = 0;

    // The GL shader program (for callers that need it).
    virtual GLuint program() const = 0;

    // True when create() succeeded and resources are valid.
    virtual bool ready() const = 0;
};
