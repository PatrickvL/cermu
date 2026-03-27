#pragma once

// ============================================================================
// GPU Vector Display Shader
// ============================================================================
//
// Renders vector graphics signals (Atari Asteroids/Tempest/Star Wars,
// Vectrex, etc.) from a VectorVideoSample stream.
//
// Vector displays have no scanlines.  The beam is steered to arbitrary
// X/Y coordinates via DACs; intensity controls brightness.  The signal
// carries a sequence of {x, y, intensity, color_index, flags} samples.
// Consecutive samples with BeamOn produce visible line segments.
//
// Rendering approach (GL 3.0 compatible — no geometry shaders):
//
//   1. CPU extracts line segments from the VectorVideoSample stream:
//      consecutive BeamOn samples form line endpoints.  Each segment
//      is expanded into a screen-space quad (two triangles) with
//      configurable beam width.  Per-vertex color is baked from the
//      sample's color_index via a caller-provided palette.
//
//   2. Quad vertices are uploaded to a VBO and rendered in one draw call
//      with a simple vertex+fragment shader.  The fragment shader applies
//      a gaussian beam profile.  Color comes from per-vertex data.
//
//   3. Phosphor persistence is achieved by rendering the new frame onto
//      the previous frame with alpha decay — the host code is responsible
//      for managing the FBO pair and the decay blend.
//
// Textures:
//   none (vertices carry all data)
//
// Uniforms:
//   ProjMtx:       mat4 — maps vector coordinates to NDC
//   PhosphorColor: vec3 — global phosphor tint multiplied with per-vertex
//                  color.  Set to (1,1,1) for color games (palette carries
//                  the color); set to P31 green for monochrome DVG games.
//
// Attributes (per vertex):
//   Position:       vec2  — screen-space quad vertex position
//   Intensity:      float — beam brightness [0, 1] for this segment
//   DistFromCenter: float — signed distance from beam center line
//                   (normalized: 0 = center, ±1 = edge)
//   Color:          vec3  — per-vertex beam color (from color palette)
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "gui/gl_api.hpp"            // GL function pointers, gl_api::compile_shader()
#include "core/signal/sync_types.hpp"
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

namespace vector_shader {

// ============================================================================
// GLSL sources
// ============================================================================

// Vertex shader — receives pre-expanded quad vertices from the CPU.
// Each vertex has: position (screen-space), intensity, signed
// distance from beam center (gaussian profile), and per-vertex color.
static constexpr const char* vertex_src = R"glsl(
#version 130
uniform mat4 ProjMtx;

in vec2  Position;
in float Intensity;
in float DistFromCenter;
in vec3  Color;

out float v_intensity;
out float v_dist;
out vec3  v_color;

void main() {
    v_intensity = Intensity;
    v_dist      = DistFromCenter;
    v_color     = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);
}
)glsl";

// Fragment shader — gaussian beam profile with per-vertex color.
//
// The beam intensity falls off with distance from the center line
// using a gaussian profile.  Per-vertex color carries the beam's
// RGB value (baked from the phosphor tint or AVG color palette on
// the CPU side).  PhosphorColor uniform provides a global tint
// multiplier — (1,1,1) for color games, P31 green for monochrome.
//
// Intensity can bloom via additive blending in the host code.
static constexpr const char* fragment_src = R"glsl(
#version 130

in float v_intensity;
in float v_dist;
in vec3  v_color;

uniform vec3 PhosphorColor;    // global tint (default: 1,1,1 — pass-through)

out vec4 Out_Color;

void main() {
    // Gaussian falloff: exp(-2.5 * d^2) gives a wider, brighter beam
    // profile matching the look of real vector monitors.
    float d = v_dist;            // [-1, 1] normalized dist from center
    float falloff = exp(-2.5 * d * d);
    float bright = v_intensity * falloff;

    vec3 color = v_color * PhosphorColor;
    Out_Color = vec4(color * bright, bright);
}
)glsl";

// ============================================================================
// Uniform locations
// ============================================================================

struct VectorShaderLocations {
    GLint proj_mtx;
    GLint beam_width;       // not a shader uniform — used by CPU quad expansion
    GLint phosphor_color;
};

// ============================================================================
// Shader program creation
// ============================================================================

inline GLuint create_program(VectorShaderLocations* locs) {

    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) return 0;
    GLuint fs = gl_api::compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { gl_api::glDeleteShader(vs); return 0; }

    GLuint prog = gl_api::glCreateProgram();
    gl_api::glAttachShader(prog, vs);
    gl_api::glAttachShader(prog, fs);

    // Custom attribute layout for vector vertices
    gl_api::glBindAttribLocation(prog, 0, "Position");
    gl_api::glBindAttribLocation(prog, 1, "Intensity");
    gl_api::glBindAttribLocation(prog, 2, "DistFromCenter");
    gl_api::glBindAttribLocation(prog, 3, "Color");

    gl_api::glLinkProgram(prog);
    gl_api::glDeleteShader(vs);
    gl_api::glDeleteShader(fs);

    GLint status = 0;
    gl_api::glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        gl_api::glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "vector_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(prog);
        return 0;
    }

    // Default PhosphorColor to white (pass-through) — per-vertex color
    // carries the actual beam color.  Callers rendering monochrome DVG
    // games can override this to P31 green (0.2, 1.0, 0.4).
    gl_api::glUseProgram(prog);
    gl_api::glUniform3f(gl_api::glGetUniformLocation(prog, "PhosphorColor"),
                1.0f, 1.0f, 1.0f);
    gl_api::glUseProgram(0);

    if (locs) {
        locs->proj_mtx       = gl_api::glGetUniformLocation(prog, "ProjMtx");
        locs->phosphor_color = gl_api::glGetUniformLocation(prog, "PhosphorColor");
    }

    printf("vector_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

// ============================================================================
// Vertex format for CPU-side quad expansion
// ============================================================================

// Per-vertex data for the beam quad mesh.
// Each line segment produces 2 triangles = 6 vertices.
struct BeamVertex {
    float x, y;             // screen-space position
    float intensity;        // beam brightness [0, 1]
    float dist_from_center; // [-1, +1] — used for gaussian falloff
    float r, g, b;          // beam color (from palette or phosphor tint)
};
static_assert(sizeof(BeamVertex) == 28);

// ============================================================================
// VBO / VAO management
// ============================================================================

struct VectorDisplayResources {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shader = 0;
    VectorShaderLocations locs{};
    int    vertex_count = 0;
};

inline bool create_resources(VectorDisplayResources* res) {
    if (!gl_api::glGenVertexArrays || !gl_api::glGenBuffers) return false;

    res->shader = create_program(&res->locs);
    if (!res->shader) return false;

    gl_api::glGenVertexArrays(1, &res->vao);
    gl_api::glGenBuffers(1, &res->vbo);

    gl_api::glBindVertexArray(res->vao);
    gl_api::glBindBuffer(GL_ARRAY_BUFFER, res->vbo);

    // Set up vertex attribute layout: {x, y, intensity, dist_from_center, r, g, b}
    // Position (location 0): 2 floats at offset 0
    gl_api::glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(0));
    gl_api::glEnableVertexAttribArray(0);

    // Intensity (location 1): 1 float at offset 8
    gl_api::glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(8));
    gl_api::glEnableVertexAttribArray(1);

    // DistFromCenter (location 2): 1 float at offset 12
    gl_api::glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(12));
    gl_api::glEnableVertexAttribArray(2);

    // Color (location 3): 3 floats at offset 16
    gl_api::glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(16));
    gl_api::glEnableVertexAttribArray(3);

    gl_api::glBindVertexArray(0);
    gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);

    printf("vector_shader: created VAO %u, VBO %u\n", res->vao, res->vbo);
    return true;
}

inline void destroy_resources(VectorDisplayResources* res) {
    if (res->vao) { gl_api::glDeleteVertexArrays(1, &res->vao); res->vao = 0; }
    if (res->vbo) { gl_api::glDeleteBuffers(1, &res->vbo); res->vbo = 0; }
    if (res->shader) { gl_api::glDeleteProgram(res->shader); res->shader = 0; }
}

// ============================================================================
// CPU-side line segment extraction and quad expansion
// ============================================================================

// Default color palette for AVG STAT[2:0] — nominal P22 tricolor mapping.
// Games may override with their actual color PROM values.  Monochrome
// DVG games should pass a single-color palette (e.g. all entries = white,
// with PhosphorColor set to P31 green).
inline constexpr float DEFAULT_COLOR_PALETTE[8 * 3] = {
    1.0f, 1.0f, 1.0f,   // 0: white
    0.15f, 0.15f, 1.0f, // 1: blue
    0.15f, 1.0f, 0.15f, // 2: green
    0.15f, 1.0f, 1.0f,  // 3: cyan
    1.0f, 0.15f, 0.15f, // 4: red
    1.0f, 0.15f, 1.0f,  // 5: magenta
    1.0f, 1.0f, 0.15f,  // 6: yellow
    1.0f, 1.0f, 1.0f,   // 7: white
};

// Extract line segments from VectorVideoSample stream and expand each
// into a screen-space quad (2 triangles = 6 vertices).
//
// VectorVideoSample layout (8 bytes):
//   int16_t x, y;          — beam position in hardware coordinates
//   uint8_t intensity;     — Z-axis drive level [0, 255]
//   uint8_t color_index;   — AVG STAT[2:0] color select; 0 = white/mono
//   SyncFlag flags;      — BeamOn when drawing
//   uint8_t _pad;
//
// Line segments are formed between consecutive samples where both have
// BeamOn set.  A BeamOff or FrameEnd sample breaks the current segment.
//
// Parameters:
//   signal_output  — raw VectorVideoSample signal output
//   signal_output_len     — number of samples
//   beam_width     — half-width of beam in display pixels
//   display_w/h    — display dimensions in pixels
//   x/y_scale      — hardware units → display pixels
//   x/y_offset     — offset applied after scaling
//   color_palette  — 8×3 float RGB palette for color_index lookup.
//                     If null, all vertices get white (1,1,1) and the
//                     PhosphorColor uniform provides the tint.
//   vertices       — output: caller-provided vector, cleared and filled
inline void build_beam_quads(
    const void* signal_output,
    uint32_t signal_output_len,
    float beam_width,
    float display_w,
    float display_h,
    float x_scale,
    float y_scale,
    float x_offset,
    float y_offset,
    const float* color_palette,
    std::vector<BeamVertex>& vertices)
{
    vertices.clear();
    if (!signal_output || signal_output_len < 2) return;

    // Reserve generous estimate: typical Asteroids frame has ~500 vectors
    vertices.reserve(signal_output_len * 3);

    struct Sample {
        int16_t x, y;
        uint8_t intensity;
        uint8_t color_index;
        uint8_t flags;
        uint8_t _pad;
    };
    static_assert(sizeof(Sample) == 8);

    const auto* samples = static_cast<const Sample*>(signal_output);

    // Track whether we have a valid previous position.  The first sample
    // in the stream establishes prev_x/prev_y but must not draw a line
    // from the initial (0,0) — that would create a spurious random line.
    bool have_prev = false;
    float prev_x = 0, prev_y = 0;

    for (uint32_t i = 0; i < signal_output_len; i++) {
        const auto& s = samples[i];
        bool beam_on = (s.flags & static_cast<uint8_t>(SyncFlag::BeamOn)) != 0;
        bool frame_end = (s.flags & static_cast<uint8_t>(SyncFlag::FrameEnd)) != 0;

        float sx = static_cast<float>(s.x) * x_scale + x_offset;
        float sy = static_cast<float>(s.y) * y_scale + y_offset;
        float si = static_cast<float>(s.intensity) / 255.0f;

        // Look up per-vertex color from palette
        float cr = 1.0f, cg = 1.0f, cb = 1.0f;
        if (color_palette) {
            int ci = s.color_index & 0x07;
            cr = color_palette[ci * 3 + 0];
            cg = color_palette[ci * 3 + 1];
            cb = color_palette[ci * 3 + 2];
        }

        if (beam_on && have_prev) {
            // Line segment from (prev_x, prev_y) to (sx, sy).
            // Each BeamOn sample is the endpoint of a visible vector;
            // the start point is the previous sample's position.
            // Intensity and color come from the current sample — not
            // averaged with the previous, which may be a dark move.
            float dx = sx - prev_x;
            float dy = sy - prev_y;
            float len_sq = dx * dx + dy * dy;

            if (len_sq > 0.000001f) {
                // Perpendicular direction for beam width expansion.
                float inv_len = 1.0f / std::sqrt(len_sq);
                float nx = -dy * inv_len * beam_width;
                float ny =  dx * inv_len * beam_width;

                // Expand into a quad: 2 triangles, 6 vertices
                vertices.push_back({prev_x - nx, prev_y - ny, si, -1.0f, cr, cg, cb});
                vertices.push_back({prev_x + nx, prev_y + ny, si, +1.0f, cr, cg, cb});
                vertices.push_back({sx     - nx, sy     - ny, si, -1.0f, cr, cg, cb});
                vertices.push_back({prev_x + nx, prev_y + ny, si, +1.0f, cr, cg, cb});
                vertices.push_back({sx     + nx, sy     + ny, si, +1.0f, cr, cg, cb});
                vertices.push_back({sx     - nx, sy     - ny, si, -1.0f, cr, cg, cb});
            } else {
                // Zero-length segment: stationary beam → render as a dot.
                // Expand into a beam_width × beam_width quad with gaussian
                // falloff along one axis (close enough for small beam spots).
                float bw = beam_width;
                vertices.push_back({sx - bw, sy - bw, si, -1.0f, cr, cg, cb});
                vertices.push_back({sx - bw, sy + bw, si, +1.0f, cr, cg, cb});
                vertices.push_back({sx + bw, sy - bw, si, -1.0f, cr, cg, cb});
                vertices.push_back({sx - bw, sy + bw, si, +1.0f, cr, cg, cb});
                vertices.push_back({sx + bw, sy + bw, si, +1.0f, cr, cg, cb});
                vertices.push_back({sx + bw, sy - bw, si, -1.0f, cr, cg, cb});
            }
        }

        prev_x = sx;
        prev_y = sy;
        have_prev = !frame_end;  // FrameEnd invalidates prev position
    }
}

// Upload vertices to VBO and render.
// Caller must have already bound the correct shader and set uniforms.
inline void upload_and_draw(VectorDisplayResources* res,
                            const BeamVertex* verts, int count) {
    if (!res || !res->vao || !res->vbo || count <= 0) return;

    gl_api::glBindVertexArray(res->vao);
    gl_api::glBindBuffer(GL_ARRAY_BUFFER, res->vbo);
    gl_api::glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count) * sizeof(BeamVertex),
                 verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, count);
    gl_api::glBindVertexArray(0);
    gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);

    res->vertex_count = count;
}

// ============================================================================
// Phosphor persistence — per-channel two-component decay
// ============================================================================
//
// Real vector CRT phosphors exhibit a two-component exponential decay:
//
//   brightness(t) = A·exp(-t/τ_fast) + B·exp(-t/τ_slow)
//
// The fast component (τ_fast ≈ 1 ms) is the sharp cutoff — peak brightness
// vanishes well within one frame.  The slow component (τ_slow ≈ 15–25 ms,
// B ≈ 10–15% of A) is the lingering phosphor glow that spans 1–2 frames
// at 30–40 fps.  This tail is what gives Asteroids its characteristic warm
// glow on stationary geometry.
//
// Phosphor types by game era:
//
//   P31 (monochrome DVG — Asteroids, Battlezone, etc.):
//     Medium-persistence green, same as oscilloscope CRTs.
//     Decay to 10% of peak: ~1–3 ms.  Long tail: 20–40 ms to
//     visually imperceptible.  All channels decay at the same rate.
//
//   P22 (color AVG — Tempest, Black Widow, Space Duel, etc.):
//     Shadow-mask tricolor.  The three phosphor components decay at
//     different rates:
//       Red:   fastest, τ_slow ≈ 12 ms.  ~1–2 ms to 10%.
//       Green: medium,  τ_slow ≈ 20 ms.  Similar to P31.
//       Blue:  slowest, τ_slow ≈ 30 ms.  ~3–5 ms to 10%, longer tail.
//     A white vector doesn't decay uniformly — blue lingers, giving
//     fading white vectors a faint bluish trail.  Subtle but real.
//
// At a typical 30–40 fps game loop (25–33 ms frame period):
//   - 10% decay happens within one frame — no inter-frame peak accumulation
//   - The slow tail spans 1–2 frames — this is where persistence lives
//   - Stationary vectors appear steady because the tail is repainted
//     each frame before it fully decays

// Per-channel slow-decay time constants (in seconds).
// decay_factor = exp(-frame_dt / τ_slow), applied per channel per frame.
struct PhosphorProfile {
    float tau_r;    // red   slow decay τ (seconds)
    float tau_g;    // green slow decay τ
    float tau_b;    // blue  slow decay τ
};

// P31 monochrome green — all channels same rate.
inline constexpr PhosphorProfile PHOSPHOR_P31 = { 0.020f, 0.020f, 0.020f };

// P22 tricolor — red fastest, blue slowest.
inline constexpr PhosphorProfile PHOSPHOR_P22 = { 0.012f, 0.020f, 0.030f };

/// Compute per-channel decay factors from a phosphor profile and frame dt.
/// Returns vec3 of (decay_r, decay_g, decay_b) where each is exp(-dt/τ).
struct DecayFactors { float r, g, b; };

inline DecayFactors compute_decay(const PhosphorProfile& p, float dt_seconds) {
    return {
        std::exp(-dt_seconds / p.tau_r),
        std::exp(-dt_seconds / p.tau_g),
        std::exp(-dt_seconds / p.tau_b)
    };
}

// ============================================================================
// Decay shader — fullscreen quad that fades the persistence FBO per channel
// ============================================================================

// Vertex shader for fullscreen triangle (no VBO needed — gl_VertexID trick).
// Draws a single triangle that covers the entire screen.
static constexpr const char* decay_vertex_src = R"glsl(
#version 130
out vec2 v_uv;
void main() {
    // Three vertices of a fullscreen triangle: (-1,-1), (3,-1), (-1,3)
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    v_uv = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)glsl";

// Fragment shader — reads previous persistence buffer, multiplies by
// per-channel decay factor.  This implements the slow exponential tail.
static constexpr const char* decay_fragment_src = R"glsl(
#version 130
in vec2 v_uv;
uniform sampler2D PrevFrame;
uniform vec3 Decay;        // per-channel decay factors: exp(-dt/τ)
out vec4 Out_Color;
void main() {
    vec4 prev = texture(PrevFrame, v_uv);
    Out_Color = vec4(prev.rgb * Decay, 1.0);
}
)glsl";

// ============================================================================
// PhosphorPersistence — FBO-based per-channel decay
// ============================================================================

struct PhosphorPersistence {
    GLuint fbo      = 0;    // framebuffer object
    GLuint texture  = 0;    // color attachment (RGBA float recommended, RGBA8 ok)
    GLuint decay_shader = 0;
    GLint  decay_loc_prev  = -1;
    GLint  decay_loc_decay = -1;
    GLuint dummy_vao = 0;   // empty VAO for gl_VertexID fullscreen triangle
    int    width  = 0;
    int    height = 0;
    PhosphorProfile profile = PHOSPHOR_P31;
};

inline bool create_persistence(PhosphorPersistence* p, int w, int h,
                               const PhosphorProfile& profile = PHOSPHOR_P31) {
    p->width   = w;
    p->height  = h;
    p->profile = profile;

    // Create FBO texture
    glGenTextures(1, &p->texture);
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO
    gl_api::glGenFramebuffers(1, &p->fbo);
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    gl_api::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p->texture, 0);
    GLenum status = gl_api::glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "vector_shader: persistence FBO incomplete (0x%x)\n", status);
        return false;
    }

    // Compile decay shader
    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, decay_vertex_src);
    if (!vs) return false;
    GLuint fs = gl_api::compile_shader(GL_FRAGMENT_SHADER, decay_fragment_src);
    if (!fs) { gl_api::glDeleteShader(vs); return false; }

    p->decay_shader = gl_api::glCreateProgram();
    gl_api::glAttachShader(p->decay_shader, vs);
    gl_api::glAttachShader(p->decay_shader, fs);
    gl_api::glLinkProgram(p->decay_shader);
    gl_api::glDeleteShader(vs);
    gl_api::glDeleteShader(fs);

    GLint link_ok = 0;
    gl_api::glGetProgramiv(p->decay_shader, GL_LINK_STATUS, &link_ok);
    if (link_ok != GL_TRUE) {
        char log[512];
        gl_api::glGetProgramInfoLog(p->decay_shader, sizeof(log), nullptr, log);
        fprintf(stderr, "vector_shader: decay shader link error: %s\n", log);
        gl_api::glDeleteProgram(p->decay_shader);
        p->decay_shader = 0;
        return false;
    }

    p->decay_loc_prev  = gl_api::glGetUniformLocation(p->decay_shader, "PrevFrame");
    p->decay_loc_decay = gl_api::glGetUniformLocation(p->decay_shader, "Decay");

    // Set sampler to texture unit 0
    gl_api::glUseProgram(p->decay_shader);
    gl_api::glUniform1i(p->decay_loc_prev, 0);
    gl_api::glUseProgram(0);

    // Empty VAO for fullscreen triangle (gl_VertexID-based)
    gl_api::glGenVertexArrays(1, &p->dummy_vao);

    printf("vector_shader: persistence FBO %u (%dx%d), decay shader %u\n",
           p->fbo, w, h, p->decay_shader);
    return true;
}

inline void destroy_persistence(PhosphorPersistence* p) {
    if (p->fbo)          { gl_api::glDeleteFramebuffers(1, &p->fbo); p->fbo = 0; }
    if (p->texture)      { glDeleteTextures(1, &p->texture); p->texture = 0; }
    if (p->decay_shader) { gl_api::glDeleteProgram(p->decay_shader); p->decay_shader = 0; }
    if (p->dummy_vao)    { gl_api::glDeleteVertexArrays(1, &p->dummy_vao); p->dummy_vao = 0; }
    p->width = p->height = 0;
}

/// Resize the persistence FBO if the display dimensions changed.
inline void resize_persistence(PhosphorPersistence* p, int w, int h) {
    if (p->width == w && p->height == h) return;
    p->width  = w;
    p->height = h;
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ============================================================================
// Persistence rendering pass
// ============================================================================
//
// Call this once per frame BEFORE the ImGui render pass.  It:
//   1. Binds the persistence FBO
//   2. Runs the decay shader (fades previous frame per channel)
//   3. Renders new beam quads on top (additive blend)
//   4. Unbinds the FBO
//
// Afterwards, display p->texture as an ImGui::Image in the screen window.

inline void render_persistence_frame(
    PhosphorPersistence* p,
    GLuint beam_shader,
    GLint  beam_loc_proj,
    GLint  beam_loc_phosphor,
    GLuint beam_vao,
    GLuint beam_vbo,
    const BeamVertex* verts,
    int    vertex_count,
    float  frame_dt_seconds,
    float  phosphor_r, float phosphor_g, float phosphor_b)
{
    if (!p || !p->fbo) return;

    // Save current viewport and FBO state
    GLint prev_viewport[4];
    glGetIntegerv(GL_VIEWPORT, prev_viewport);
    GLint prev_fbo = 0;
    glGetIntegerv(0x8CA6 /* GL_DRAW_FRAMEBUFFER_BINDING */, &prev_fbo);

    // Bind persistence FBO
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    glViewport(0, 0, p->width, p->height);

    // --- Pass 1: Decay previous frame per channel ---
    // No blending — overwrite the FBO with decayed previous content.
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    DecayFactors decay = compute_decay(p->profile, frame_dt_seconds);

    gl_api::glUseProgram(p->decay_shader);
    gl_api::glUniform3f(p->decay_loc_decay, decay.r, decay.g, decay.b);

    // Bind persistence texture as input (reading our own previous content)
    // This is safe because the fullscreen triangle writes every pixel exactly
    // once and there's no overlap between read and write for any given pixel
    // in a single-pass fullscreen quad.  Technically this is a feedback loop
    // in the GL spec (reading an FBO attachment that's also the render target)
    // but all major desktop GL drivers handle single-pass full-overwrites
    // correctly.  If artifacts appear on exotic hardware, ping-pong to a
    // second FBO.
    gl_api::glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p->texture);

    gl_api::glBindVertexArray(p->dummy_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    gl_api::glBindVertexArray(0);

    // --- Pass 2: Render new beam quads additively ---
    if (vertex_count > 0 && verts) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive

        gl_api::glUseProgram(beam_shader);

        // Ortho projection mapping [0, width) × [0, height) to NDC
        const float w = static_cast<float>(p->width);
        const float h = static_cast<float>(p->height);
        const float ortho[4][4] = {
            { 2.0f/w,  0.0f,    0.0f,  0.0f },
            { 0.0f,    2.0f/h,  0.0f,  0.0f },
            { 0.0f,    0.0f,   -1.0f,  0.0f },
            {-1.0f,   -1.0f,    0.0f,  1.0f },
        };
        gl_api::glUniformMatrix4fv(beam_loc_proj, 1, GL_FALSE, &ortho[0][0]);
        gl_api::glUniform3f(beam_loc_phosphor, phosphor_r, phosphor_g, phosphor_b);

        gl_api::glBindVertexArray(beam_vao);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, beam_vbo);
        gl_api::glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertex_count) * static_cast<GLsizeiptr>(sizeof(BeamVertex)),
                     verts, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
        gl_api::glBindVertexArray(0);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Restore previous state
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    gl_api::glUseProgram(0);
}

} // namespace vector_shader
