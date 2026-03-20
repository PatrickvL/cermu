#pragma once

// ============================================================================
// GPU Vector Display Shader
// ============================================================================
//
// Renders vector graphics signals (Atari Asteroids/Tempest/Star Wars,
// Vectrex, etc.) from a VectorVideoSample stream.
//
// Vector displays have no scanlines.  The beam is steered to arbitrary
// X/Y coordinates via DACs; intensity controls brightness.  The stream
// carries a sequence of {x, y, intensity, flags} samples.  Consecutive
// samples with BeamOn produce visible line segments.
//
// Rendering approach (GL 3.0 compatible — no geometry shaders):
//
//   1. CPU extracts line segments from the VectorVideoSample stream:
//      consecutive BeamOn samples form line endpoints.  Each segment
//      is expanded into a screen-space quad (two triangles) with
//      configurable beam width.
//
//   2. Quad vertices are uploaded to a VBO and rendered in one draw call
//      with a simple vertex+fragment shader.  The fragment shader applies
//      a gaussian beam profile and phosphor tinting.
//
//   3. Phosphor persistence is achieved by rendering the new frame onto
//      the previous frame with alpha decay — the host code is responsible
//      for managing the FBO pair and the decay blend.
//
// Textures:
//   none (vertices carry all data)
//
// Uniforms:
//   ProjMtx:    mat4 — maps vector coordinates to NDC
//   BeamWidth:  float — screen-space half-width of the beam, in pixels
//   PhosphorR, PhosphorG, PhosphorB: float — phosphor tint color
//
// Attributes (per vertex):
//   Position:   vec2 — screen-space quad vertex position
//   Intensity:  float — beam brightness [0, 1] for this segment
//   DistFromCenter: float — signed distance from beam center line
//                    (normalized: 0 = center, ±1 = edge)
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "gui/indexed_shader.hpp"     // GL function pointers, compile_shader()
#include "core/signal/sync_types.hpp"
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

namespace vector_shader {

// ============================================================================
// Additional GL function pointers needed for VBO/VAO rendering
// ============================================================================

inline void  (APIENTRY* glGenBuffers)(GLsizei, GLuint*)        = nullptr;
inline void  (APIENTRY* glBindBuffer)(GLenum, GLuint)          = nullptr;
inline void  (APIENTRY* glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
inline void  (APIENTRY* glGenVertexArrays)(GLsizei, GLuint*)   = nullptr;
inline void  (APIENTRY* glDeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
inline void  (APIENTRY* glBindVertexArray)(GLuint)              = nullptr;
inline void  (APIENTRY* glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
inline void  (APIENTRY* glEnableVertexAttribArray)(GLuint)      = nullptr;
inline void  (APIENTRY* glDeleteBuffers)(GLsizei, const GLuint*) = nullptr;
inline void  (APIENTRY* glUniform1f)(GLint, GLfloat)            = nullptr;
inline void  (APIENTRY* glUniform3f)(GLint, GLfloat, GLfloat, GLfloat) = nullptr;

// Load vector-specific GL functions.  Call after indexed_shader::load_gl().
inline bool load_gl() {
    #define VGL_LOAD(fn, name) fn = (decltype(fn))SDL_GL_GetProcAddress(name)
    VGL_LOAD(glGenBuffers,              "glGenBuffers");
    VGL_LOAD(glBindBuffer,              "glBindBuffer");
    VGL_LOAD(glBufferData,              "glBufferData");
    VGL_LOAD(glGenVertexArrays,         "glGenVertexArrays");
    VGL_LOAD(glDeleteVertexArrays,      "glDeleteVertexArrays");
    VGL_LOAD(glBindVertexArray,         "glBindVertexArray");
    VGL_LOAD(glVertexAttribPointer,     "glVertexAttribPointer");
    VGL_LOAD(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    VGL_LOAD(glDeleteBuffers,           "glDeleteBuffers");
    VGL_LOAD(glUniform1f,              "glUniform1f");
    VGL_LOAD(glUniform3f,              "glUniform3f");
    #undef VGL_LOAD

    bool ok = glGenBuffers && glBindBuffer && glBufferData
           && glGenVertexArrays && glBindVertexArray
           && glVertexAttribPointer && glEnableVertexAttribArray
           && glUniform1f && glUniform3f;
    if (!ok)
        fprintf(stderr, "vector_shader: failed to load one or more GL functions\n");
    return ok;
}

// GL constants not always available in legacy headers
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

// ============================================================================
// GLSL sources
// ============================================================================

// Vertex shader — receives pre-expanded quad vertices from the CPU.
// Each vertex has: position (screen-space), intensity, and signed
// distance from beam center (for gaussian profile in fragment shader).
static constexpr const char* vertex_src = R"glsl(
#version 130
uniform mat4 ProjMtx;

in vec2  Position;
in float Intensity;
in float DistFromCenter;

out float v_intensity;
out float v_dist;

void main() {
    v_intensity = Intensity;
    v_dist      = DistFromCenter;
    gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);
}
)glsl";

// Fragment shader — gaussian beam profile with phosphor tinting.
//
// The beam intensity falls off with distance from the center line
// using a gaussian profile.  The phosphor color (default: P31 green)
// tints the output.  Intensity is clamped but can bloom via additive
// blending in the host code.
static constexpr const char* fragment_src = R"glsl(
#version 130

in float v_intensity;
in float v_dist;

uniform vec3 PhosphorColor;    // phosphor tint (default: warm green P31)

out vec4 Out_Color;

void main() {
    // Gaussian falloff: exp(-2.5 * d^2) gives a wider, brighter beam
    // profile matching the look of real vector monitors.
    float d = v_dist;            // [-1, 1] normalized dist from center
    float falloff = exp(-2.5 * d * d);
    float bright = v_intensity * falloff;

    Out_Color = vec4(PhosphorColor * bright, bright);
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
    using namespace indexed_shader;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) return 0;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    // Custom attribute layout for vector vertices
    glBindAttribLocation(prog, 0, "Position");
    glBindAttribLocation(prog, 1, "Intensity");
    glBindAttribLocation(prog, 2, "DistFromCenter");

    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "vector_shader: link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }

    // Set default phosphor color (P31 warm green — typical Atari vector monitor)
    glUseProgram(prog);
    glUniform3f(glGetUniformLocation(prog, "PhosphorColor"),
                0.2f, 1.0f, 0.4f);
    glUseProgram(0);

    if (locs) {
        locs->proj_mtx       = glGetUniformLocation(prog, "ProjMtx");
        locs->phosphor_color = glGetUniformLocation(prog, "PhosphorColor");
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
};
static_assert(sizeof(BeamVertex) == 16);

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
    if (!glGenVertexArrays || !glGenBuffers) return false;

    res->shader = create_program(&res->locs);
    if (!res->shader) return false;

    glGenVertexArrays(1, &res->vao);
    glGenBuffers(1, &res->vbo);

    glBindVertexArray(res->vao);
    glBindBuffer(GL_ARRAY_BUFFER, res->vbo);

    // Set up vertex attribute layout: {x, y, intensity, dist_from_center}
    // Position (location 0): 2 floats at offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Intensity (location 1): 1 float at offset 8
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(8));
    glEnableVertexAttribArray(1);

    // DistFromCenter (location 2): 1 float at offset 12
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(BeamVertex),
                          reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    printf("vector_shader: created VAO %u, VBO %u\n", res->vao, res->vbo);
    return true;
}

inline void destroy_resources(VectorDisplayResources* res) {
    if (res->vao) { glDeleteVertexArrays(1, &res->vao); res->vao = 0; }
    if (res->vbo) { glDeleteBuffers(1, &res->vbo); res->vbo = 0; }
    if (res->shader) { indexed_shader::glDeleteProgram(res->shader); res->shader = 0; }
}

// ============================================================================
// CPU-side line segment extraction and quad expansion
// ============================================================================

// Extract line segments from VectorVideoSample stream and expand each
// into a screen-space quad (2 triangles = 6 vertices).
//
// VectorVideoSample layout (8 bytes):
//   int16_t x, y;        — beam position in hardware coordinates
//   uint8_t intensity;   — brightness [0, 255]
//   uint8_t _pad;
//   VideoFlags flags;    — BeamOn when drawing
//   uint8_t _pad2;
//
// Line segments are formed between consecutive samples where both have
// BeamOn set.  A BeamOff or FrameEnd sample breaks the current segment.
//
// Parameters:
//   stream       — raw VectorVideoSample stream
//   stream_len   — number of samples
//   beam_width   — half-width of beam in display pixels
//   display_w    — display width in pixels (for coordinate normalization)
//   display_h    — display height in pixels
//   vertices     — output: caller-provided vector, cleared and filled
//
// Coordinate mapping:
//   VectorVideoSample.x/y are signed 16-bit hardware coordinates.
//   The DVG (Digital Vector Generator) uses a 1024×1024 coordinate space.
//   This function normalizes to [0, display_w) × [0, display_h).
//   The caller can override the mapping by adjusting x_scale/y_scale.
inline void build_beam_quads(
    const void* stream,
    uint32_t stream_len,
    float beam_width,
    float display_w,
    float display_h,
    float x_scale,          // hardware units → display pixels (e.g. display_w / 1024)
    float y_scale,          // hardware units → display pixels (e.g. display_h / 1024)
    float x_offset,         // offset applied after scaling
    float y_offset,
    std::vector<BeamVertex>& vertices)
{
    vertices.clear();
    if (!stream || stream_len < 2) return;

    // Reserve generous estimate: typical Asteroids frame has ~500 vectors
    vertices.reserve(stream_len * 3);

    struct Sample {
        int16_t x, y;
        uint8_t intensity;
        uint8_t _pad;
        uint8_t flags;
        uint8_t _pad2;
    };
    static_assert(sizeof(Sample) == 8);

    const auto* samples = static_cast<const Sample*>(stream);

    bool prev_beam_on = false;
    float prev_x = 0, prev_y = 0;
    float prev_intensity = 0;

    for (uint32_t i = 0; i < stream_len; i++) {
        const auto& s = samples[i];
        bool beam_on = (s.flags & static_cast<uint8_t>(VideoFlags::BeamOn)) != 0;

        float sx = static_cast<float>(s.x) * x_scale + x_offset;
        float sy = static_cast<float>(s.y) * y_scale + y_offset;
        float si = static_cast<float>(s.intensity) / 255.0f;

        if (beam_on && prev_beam_on) {
            // Line segment from (prev_x, prev_y) to (sx, sy)
            float dx = sx - prev_x;
            float dy = sy - prev_y;
            float len_sq = dx * dx + dy * dy;

            if (len_sq > 0.000001f) {
                // Perpendicular direction for beam width expansion.
                // One division by len instead of sqrt + two divides.
                float inv_len = 1.0f / std::sqrt(len_sq);
                float nx = -dy * inv_len * beam_width;
                float ny =  dx * inv_len * beam_width;

                // Average intensity for the segment
                float avg_i = (prev_intensity + si) * 0.5f;

                // Expand into a quad: 2 triangles, 6 vertices
                // Vertices along the perpendicular: +1 = top edge, -1 = bottom edge
                // Triangle 1: (a-n, a+n, b-n)
                vertices.push_back({prev_x - nx, prev_y - ny, avg_i, -1.0f});
                vertices.push_back({prev_x + nx, prev_y + ny, avg_i, +1.0f});
                vertices.push_back({sx     - nx, sy     - ny, avg_i, -1.0f});
                // Triangle 2: (a+n, b+n, b-n)
                vertices.push_back({prev_x + nx, prev_y + ny, avg_i, +1.0f});
                vertices.push_back({sx     + nx, sy     + ny, avg_i, +1.0f});
                vertices.push_back({sx     - nx, sy     - ny, avg_i, -1.0f});
            } else {
                // Degenerate segment (point) — draw a small dot
                float avg_i = (prev_intensity + si) * 0.5f;
                float hw = beam_width;
                vertices.push_back({sx - hw, sy - hw, avg_i, -1.0f});
                vertices.push_back({sx + hw, sy - hw, avg_i, +1.0f});
                vertices.push_back({sx - hw, sy + hw, avg_i, -1.0f});
                vertices.push_back({sx + hw, sy - hw, avg_i, +1.0f});
                vertices.push_back({sx + hw, sy + hw, avg_i, +1.0f});
                vertices.push_back({sx - hw, sy + hw, avg_i, -1.0f});
            }
        }

        prev_beam_on   = beam_on;
        prev_x         = sx;
        prev_y         = sy;
        prev_intensity = si;
    }
}

// Upload vertices to VBO and render.
// Caller must have already bound the correct shader and set uniforms.
inline void upload_and_draw(VectorDisplayResources* res,
                            const BeamVertex* verts, int count) {
    if (!res || !res->vao || !res->vbo || count <= 0) return;

    glBindVertexArray(res->vao);
    glBindBuffer(GL_ARRAY_BUFFER, res->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count) * sizeof(BeamVertex),
                 verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    res->vertex_count = count;
}

// ============================================================================
// Phosphor persistence
// ============================================================================
//
// For phosphor decay, the host code should:
//
//   1. Bind the persistence FBO
//   2. Draw a full-screen quad with the previous frame, alpha = (1 - decay_rate)
//   3. Enable additive blending: glBlendFunc(GL_ONE, GL_ONE)
//   4. Call upload_and_draw() to render new vectors on top
//   5. Disable blending / unbind FBO
//   6. Display the persistence FBO texture to screen
//
// Typical decay_rate: 0.15–0.25 per frame (P31 phosphor ≈ 40µs decay,
// but at 60 Hz frame rate this is essentially "full persistence" so
// the visual decay is an artistic choice for how quickly ghost images fade).
//
// FBO creation/management is left to the integration code in session_gui.cpp
// since it involves GL resources beyond shader scope.

} // namespace vector_shader
