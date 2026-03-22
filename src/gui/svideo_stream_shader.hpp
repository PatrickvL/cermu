#pragma once

// ============================================================================
// GPU S-Video Stream Reconstruction Shader
// ============================================================================
//
// S-Video (Separate Video) carries luma (Y) and chroma (C) on independent
// wires, eliminating cross-color interference (dot crawl) inherent in
// composite video.  The visual result is full-bandwidth luminance with
// softer, bandwidth-limited chrominance transitions.
//
// This shader models that characteristic: for each output pixel it takes
// the center pixel's luma at full resolution, while averaging the chroma
// (U/V) over a small horizontal window.  The result is perceptibly sharper
// than composite on patterned backgrounds, with smoother color edges.
//
// Data format:   Same as Composite — palette-indexed RG8 stream
// Texture units: 0 = StreamTex (RG8), 1 = Palette (RGBA 256×1)
// Uniforms:      Same set as stream_shader (ProjMtx, ScanlineMap, etc.)
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "gui/gl_api.hpp"            // GL function pointers, gl_api::compile_shader()
#include "gui/stream_shader.hpp"    // StreamShaderLocations, vertex_src, constants
#include <cstdio>

namespace svideo_stream_shader {

// Fragment shader — palette lookup + luma/chroma separation.
//
// For each output pixel:
//   1. Look up the center pixel's palette color → extract full-res luma (Y)
//   2. Sample a 5-pixel horizontal window of palette colors
//   3. Convert each to YUV, average U and V across the window
//   4. Recombine Y (center) + filtered U,V → output RGB
//
// The 5-pixel chroma window models S-Video's ~0.5 MHz chroma bandwidth
// versus ~3+ MHz luma bandwidth (≈6:1 ratio at typical dot clocks).
static constexpr const char* fragment_src = R"glsl(
#version 130

in vec2 Frag_UV;
in vec4 Frag_Color;

uniform sampler2D StreamTex;    // unit 0: RG8 packed stream
uniform sampler2D Palette;      // unit 1: 256×1 RGBA

uniform int ScanlineMap[512];
uniform int StreamTexWidth;
uniform int DisplayHeight;
uniform int DisplayWidth;

out vec4 Out_Color;

// Fetch palette color at a 2D texture position (no division needed)
vec3 palette_at(int col, int row) {
    float idx_f = texelFetch(StreamTex, ivec2(col, row), 0).r;
    int idx = int(idx_f * 255.0 + 0.5);
    return texelFetch(Palette, ivec2(idx, 0), 0).rgb;
}

void main() {
    int scanline = clamp(int(Frag_UV.y * float(DisplayHeight)), 0, DisplayHeight - 1);
    int pixel_x  = clamp(int(Frag_UV.x * float(DisplayWidth)),  0, DisplayWidth  - 1);

    int offset = ScanlineMap[scanline];
    if (offset < 0) {
        Out_Color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Compute center pixel's 2D texture position (single division)
    int center_pos = offset + pixel_x;
    int center_row = center_pos / StreamTexWidth;
    int center_col = center_pos - center_row * StreamTexWidth;

    // Center pixel — full-resolution luma
    vec3 center = palette_at(center_col, center_row);
    float Y = 0.299 * center.r + 0.587 * center.g + 0.114 * center.b;

    // Chroma: average U/V over a 5-pixel horizontal window.
    // All window samples share the same scanline offset, so derive 2D
    // coords from the center position via column offset — no division.
    float U_sum = 0.0;
    float V_sum = 0.0;
    for (int dx = -2; dx <= 2; dx++) {
        int delta = clamp(pixel_x + dx, 0, DisplayWidth - 1) - pixel_x;
        int col = center_col + delta;
        int row = center_row;
        if (col < 0) { col += StreamTexWidth; row--; }
        else if (col >= StreamTexWidth) { col -= StreamTexWidth; row++; }
        vec3 c = palette_at(col, row);
        float cy = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        U_sum += c.b - cy;   // U ∝ B-Y
        V_sum += c.r - cy;   // V ∝ R-Y
    }
    float U = U_sum / 5.0;
    float V = V_sum / 5.0;

    // YUV → RGB (BT.601)
    vec3 rgb = vec3(
        Y + 1.140 * V,
        Y - 0.395 * U - 0.581 * V,
        Y + 2.032 * U
    );

    Out_Color = Frag_Color * vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)glsl";

// ============================================================================
// Shader program creation
// ============================================================================

// Create the S-Video stream shader program.
// Uses the same vertex shader and uniform layout as stream_shader.
// Returns the program ID (0 on failure).
inline GLuint create_program(stream_shader::StreamShaderLocations* locs) {

    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, stream_shader::vertex_src);
    if (!vs) return 0;
    GLuint fs = gl_api::compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { gl_api::glDeleteShader(vs); return 0; }

    GLuint prog = gl_api::glCreateProgram();
    gl_api::glAttachShader(prog, vs);
    gl_api::glAttachShader(prog, fs);

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
        fprintf(stderr, "svideo_stream_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit bindings — same as composite stream shader
    gl_api::glUseProgram(prog);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "StreamTex"), 0);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "Palette"), 1);
    gl_api::glUseProgram(0);

    if (locs) {
        locs->proj_mtx        = gl_api::glGetUniformLocation(prog, "ProjMtx");
        locs->scanline_map    = gl_api::glGetUniformLocation(prog, "ScanlineMap");
        locs->stream_tex_width = gl_api::glGetUniformLocation(prog, "StreamTexWidth");
        locs->display_height  = gl_api::glGetUniformLocation(prog, "DisplayHeight");
        locs->display_width   = gl_api::glGetUniformLocation(prog, "DisplayWidth");
    }

    printf("svideo_stream_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

} // namespace svideo_stream_shader
