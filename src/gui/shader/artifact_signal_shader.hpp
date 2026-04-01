#pragma once

// ============================================================================
// GPU NTSC Composite Artifact Coloring Shader
// ============================================================================
//
// Simulates the NTSC composite video encode→decode pipeline inside the
// fragment shader.  High-frequency luminance patterns create spurious
// chrominance when decoded, producing the characteristic "artifact colors"
// used by Apple II hi-res, CGA composite mode, and some C64 software.
//
// Algorithm (per output pixel):
//   1. Sample a window of palette colors around the pixel
//   2. Convert each to YIQ and encode as a composite signal:
//        composite = Y + I·cos(ωt) + Q·sin(ωt)
//      where ω = PhaseIncrement radians per pixel
//   3. Decode: demodulate I and Q from the composite signal via
//      windowed correlation, low-pass Y
//   4. Convert YIQ back to RGB
//
// The interplay between the color carrier frequency and the pixel clock
// determines the artifact pattern.  PhaseIncrement = π gives the classic
// Apple II 4-color artifact scheme (2 pixels per color cycle).
//
// Data format:   Same as Composite — palette-indexed RG8 stream
// Texture units: 0 = SignalTex (RG8), 1 = Palette (RGBA 256×1)
// Uniforms:      Base set from signal_shader + PhaseIncrement (float)
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "core/cermu.hpp"
#include "gui/gl_api.hpp"            // GL function pointers, gl_api::compile_shader()
#include "gui/shader/signal_shader.hpp"    // SignalShaderLocations, vertex_src, constants
#include <cstdio>

namespace artifact_signal_shader {

// Fragment shader — NTSC composite artifact coloring.
static constexpr const char* fragment_src = R"glsl(
#version 130

in vec2 Frag_UV;
in vec4 Frag_Color;

uniform sampler2D SignalTex;    // unit 0: RG8 packed signal
uniform sampler2D Palette;      // unit 1: 256×1 RGBA

uniform int ScanlineMap[512];
uniform int SignalTexWidth;
uniform int DisplayHeight;
uniform int DisplayWidth;
uniform float PhaseIncrement;   // radians per pixel (2π × carrier/dotclock)

out vec4 Out_Color;

// Fetch palette color at a 2D texture position (no division needed)
vec3 palette_at(int col, int row) {
    float idx_f = texelFetch(SignalTex, ivec2(col, row), 0).r;
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
    int center_row = center_pos / SignalTexWidth;
    int center_col = center_pos - center_row * SignalTexWidth;

    // Encode/decode window — 9-tap filter (radius 4)
    const int R = 4;

    // Accumulate decoded Y, I, Q via windowed correlation
    float y_out = 0.0;
    float i_out = 0.0;
    float q_out = 0.0;
    float w_sum = 0.0;

    for (int dx = -R; dx <= R; dx++) {
        int px = clamp(pixel_x + dx, 0, DisplayWidth - 1);

        // Derive 2D coords from center position — no division
        int delta = px - pixel_x;
        int col = center_col + delta;
        int row = center_row;
        if (col < 0) { col += SignalTexWidth; row--; }
        else if (col >= SignalTexWidth) { col -= SignalTexWidth; row++; }
        vec3 c = palette_at(col, row);

        // RGB → YIQ
        float y = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        float i = 0.596 * c.r - 0.275 * c.g - 0.321 * c.b;
        float q = 0.212 * c.r - 0.523 * c.g + 0.311 * c.b;

        // Encode: modulate chroma onto carrier
        float phase = float(px) * PhaseIncrement;
        float composite = y + i * cos(phase) + q * sin(phase);

        // Quadratic window (approximates Hann, cheaper than trig)
        float t = float(dx) / float(R + 1);
        float w = 1.0 - t * t;

        // Decode: low-pass luma, demodulate chroma
        y_out += composite * w;
        i_out += composite * cos(phase) * 2.0 * w;
        q_out += composite * sin(phase) * 2.0 * w;

        w_sum += w;
    }

    y_out /= w_sum;
    i_out /= w_sum;
    q_out /= w_sum;

    // YIQ → RGB
    vec3 rgb = vec3(
        y_out + 0.956 * i_out + 0.621 * q_out,
        y_out - 0.272 * i_out - 0.647 * q_out,
        y_out - 1.107 * i_out + 1.704 * q_out
    );

    Out_Color = Frag_Color * vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)glsl";

// ============================================================================
// Shader program creation
// ============================================================================

// Create the NTSC artifact signal shader program.
// Returns the program ID (0 on failure).
// out_phase_loc receives the PhaseIncrement uniform location.
inline GLuint create_program(signal_shader::SignalShaderLocations* locs,
                             GLint* out_phase_loc) {

    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, signal_shader::vertex_src);
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
        log_error("artifact_signal_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit bindings — same as composite signal shader
    gl_api::glUseProgram(prog);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "SignalTex"), 0);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "Palette"), 1);
    gl_api::glUseProgram(0);

    if (locs) {
        locs->proj_mtx        = gl_api::glGetUniformLocation(prog, "ProjMtx");
        locs->scanline_map    = gl_api::glGetUniformLocation(prog, "ScanlineMap");
        locs->signal_tex_width = gl_api::glGetUniformLocation(prog, "SignalTexWidth");
        locs->display_height  = gl_api::glGetUniformLocation(prog, "DisplayHeight");
        locs->display_width   = gl_api::glGetUniformLocation(prog, "DisplayWidth");
    }

    if (out_phase_loc)
        *out_phase_loc = gl_api::glGetUniformLocation(prog, "PhaseIncrement");

    log_info("artifact_signal_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

} // namespace artifact_signal_shader
