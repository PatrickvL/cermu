#pragma once

// ============================================================================
// GPU Indexed Palette Shader
// ============================================================================
//
// Fragment shader for palette-indexed rendering.  Video chips write 8-bit
// palette indices to an R8 texture; this shader performs the palette lookup
// on the GPU, eliminating per-pixel CPU work and reducing the texture
// upload from 4 bytes/pixel (RGBA) to 1 byte/pixel.
//
// The vertex shader matches ImGui's vertex layout so the shader can be
// injected via ImDrawList::AddCallback without disturbing ImGui's
// rendering pipeline.
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "core/cermu.hpp"
#include "gui/gl_api.hpp"

namespace indexed_shader {

// ============================================================================
// GLSL sources
// ============================================================================

// Vertex shader — identical to ImGui's OpenGL3 backend vertex shader.
// Must use the same attribute locations (0=Position, 1=UV, 2=Color)
// and ProjMtx uniform so ImGui's draw commands work unchanged.
static constexpr const char* vertex_src = R"glsl(
#version 130
uniform mat4 ProjMtx;
in vec2 Position;
in vec2 UV;
in vec4 Color;
out vec2 Frag_UV;
out vec4 Frag_Color;
void main() {
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
}
)glsl";

// Fragment shader — samples the R8 index texture (slot 0, bound by ImGui)
// and performs a palette lookup from the palette texture (slot 1).
// The palette texture is a 256×1 RGBA texture; texelFetch reads the
// exact texel without filtering.
static constexpr const char* fragment_src = R"glsl(
#version 130
in vec2 Frag_UV;
in vec4 Frag_Color;
uniform sampler2D Texture;
uniform sampler2D Palette;
out vec4 Out_Color;
void main() {
    // R8 texture stores normalized [0,1]; convert to integer index [0,255].
    float idx_f = texture(Texture, Frag_UV.st).r;
    int idx = int(idx_f * 255.0 + 0.5);
    // Palette lookup — texelFetch avoids filtering artifacts.
    vec4 color = texelFetch(Palette, ivec2(idx, 0), 0);
    Out_Color = Frag_Color * color;
}
)glsl";

// Create the indexed palette shader program.
// Returns the program ID (0 on failure).
// On success, writes uniform locations to the output parameters.
inline GLuint create_program(GLint* out_loc_proj, GLint* out_loc_palette) {

    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) return 0;
    GLuint fs = gl_api::compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { gl_api::glDeleteShader(vs); return 0; }

    GLuint prog = gl_api::glCreateProgram();
    gl_api::glAttachShader(prog, vs);
    gl_api::glAttachShader(prog, fs);

    // Bind attribute locations to match ImGui's vertex layout.
    gl_api::glBindAttribLocation(prog, 0, "Position");
    gl_api::glBindAttribLocation(prog, 1, "UV");
    gl_api::glBindAttribLocation(prog, 2, "Color");

    gl_api::glLinkProgram(prog);
    gl_api::glDeleteShader(vs);
    gl_api::glDeleteShader(fs);

    GLint status = 0;
    gl_api::glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        gl_api::glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        log_error("indexed_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit bindings (Texture=0 set by ImGui, Palette=1 set by us).
    gl_api::glUseProgram(prog);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "Texture"), 0);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "Palette"), 1);
    gl_api::glUseProgram(0);

    if (out_loc_proj)    *out_loc_proj    = gl_api::glGetUniformLocation(prog, "ProjMtx");
    if (out_loc_palette) *out_loc_palette = gl_api::glGetUniformLocation(prog, "Palette");

    log_info("indexed_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

} // namespace indexed_shader
