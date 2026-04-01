#pragma once

// ============================================================================
// GPU Y'PbPr Component Video Stream Shader
// ============================================================================
//
// Y'PbPr (component video) carries luminance and two color-difference
// signals on separate cables.  Luma (Y') travels at full bandwidth;
// Pb and Pr have reduced bandwidth (typically ~half of Y').  The visual
// result is sharper than S-Video, with full-resolution brightness and
// only slightly softened color transitions.
//
// This shader models the bandwidth asymmetry: the center pixel provides
// luma at full resolution, while Pb and Pr are averaged over a 3-pixel
// horizontal window.
//
// Data format:   Same as RGB — RGBA8 stream ({r, g, b, flags})
// Texture units: 0 = SignalTex (RGBA8, no palette)
// Uniforms:      Same set as rgb_signal_shader
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "core/cermu.hpp"
#include "gui/gl_api.hpp"            // GL function pointers, gl_api::compile_shader()
#include "gui/shader/rgb_signal_shader.hpp"    // RGBShaderLocations, vertex_src, constants
#include "gui/shader/signal_shader.hpp"        // SIGNAL_TEX_WIDTH constant
#include <cstdio>

namespace ypbpr_signal_shader {

// Fragment shader — Y'PbPr component video with chroma bandwidth limiting.
//
// For each output pixel:
//   1. Read center pixel's RGB → extract full-resolution luma (Y')
//   2. Sample a 3-pixel horizontal window of RGB values
//   3. Convert each to Y'PbPr, average Pb and Pr across the window
//   4. Recombine Y' (center) + filtered Pb,Pr → output RGB
static constexpr const char* fragment_src = R"glsl(
#version 130

in vec2 Frag_UV;
in vec4 Frag_Color;

uniform sampler2D SignalTex;    // unit 0: RGBA8 packed signal

uniform int ScanlineMap[512];
uniform int SignalTexWidth;
uniform int DisplayHeight;
uniform int DisplayWidth;

out vec4 Out_Color;

// Fetch RGB at a 2D texture position (no division needed)
vec3 rgb_at(int col, int row) {
    return texelFetch(SignalTex, ivec2(col, row), 0).rgb;
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
    int center_row = center_pos / SignalTexWidth;
    int center_col = center_pos - center_row * SignalTexWidth;

    // Center pixel — full-resolution luma (BT.601)
    vec3 center = rgb_at(center_col, center_row);
    float Y = 0.299 * center.r + 0.587 * center.g + 0.114 * center.b;

    // Pb / Pr: average over 3-pixel horizontal window.
    // Models component video's reduced chroma bandwidth (~half of Y').
    // Derive 2D coords from center position — no division.
    float Pb_sum = 0.0;
    float Pr_sum = 0.0;
    for (int dx = -1; dx <= 1; dx++) {
        int delta = clamp(pixel_x + dx, 0, DisplayWidth - 1) - pixel_x;
        int col = center_col + delta;
        int row = center_row;
        if (col < 0) { col += SignalTexWidth; row--; }
        else if (col >= SignalTexWidth) { col -= SignalTexWidth; row++; }
        vec3 c = rgb_at(col, row);
        float cy = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        Pb_sum += 0.564 * (c.b - cy);
        Pr_sum += 0.713 * (c.r - cy);
    }
    float Pb = Pb_sum / 3.0;
    float Pr = Pr_sum / 3.0;

    // Y'PbPr → RGB (BT.601)
    vec3 rgb = vec3(
        Y + 1.402 * Pr,
        Y - 0.344 * Pb - 0.714 * Pr,
        Y + 1.772 * Pb
    );

    Out_Color = Frag_Color * vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)glsl";

// ============================================================================
// Shader program creation
// ============================================================================

// Create the Y'PbPr signal shader program.
// Uses the same vertex shader and uniform layout as rgb_signal_shader.
// Returns the program ID (0 on failure).
inline GLuint create_program(rgb_signal_shader::RGBShaderLocations* locs) {

    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, rgb_signal_shader::vertex_src);
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
        log_error("ypbpr_signal_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit binding — SignalTex on unit 0 (no palette)
    gl_api::glUseProgram(prog);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "SignalTex"), 0);
    gl_api::glUseProgram(0);

    if (locs) {
        locs->proj_mtx         = gl_api::glGetUniformLocation(prog, "ProjMtx");
        locs->scanline_map     = gl_api::glGetUniformLocation(prog, "ScanlineMap");
        locs->signal_tex_width = gl_api::glGetUniformLocation(prog, "SignalTexWidth");
        locs->display_height   = gl_api::glGetUniformLocation(prog, "DisplayHeight");
        locs->display_width    = gl_api::glGetUniformLocation(prog, "DisplayWidth");
    }

    log_info("ypbpr_signal_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

} // namespace ypbpr_signal_shader
