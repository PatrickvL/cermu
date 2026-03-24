#pragma once

// ============================================================================
// GPU RGB Stream Reconstruction Shader
// ============================================================================
//
// Fragment shader for direct 1D-stream → 2D-display reconstruction of
// RGB video signals (Amiga Denise, Atari ST shifter, later consoles).
//
// Unlike the composite stream shader which reads an R8 palette index
// and performs a palette lookup, the RGB shader reads 3-byte {r, g, b}
// values directly from the stream and outputs them as fragment colors.
// No palette texture is needed.
//
// Stream texture layout:
//   RGBA8 texture packed 1D→2D (STREAM_TEX_WIDTH wide).
//   Each texel stores {r, g, b, 0} — the 4th byte is unused padding
//   (RGBVideoSample is {r, g, b, flags} and we discard flags on upload).
//
// Uniforms:
//   ProjMtx:        mat4 projection (matches ImGui's layout)
//   ScanlineMap:    int[MAX_SCANLINES] — stream position of first visible
//                   pixel for each scanline (-1 = no data / VBlank)
//   StreamTexWidth: int — width of the packed stream texture
//   DisplayHeight:  int — number of visible scanlines
//   DisplayWidth:   int — visible pixels per scanline
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "gui/gl_api.hpp"            // GL function pointers, gl_api::compile_shader()
#include "gui/shader/stream_shader.hpp"    // MAX_SCANLINES, STREAM_TEX_WIDTH, compute_scanline_map()
#include <cstdio>
#include <cstdint>

namespace rgb_stream_shader {

// ============================================================================
// GLSL sources
// ============================================================================

// Vertex shader — identical to ImGui's OpenGL3 backend vertex shader.
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

// Fragment shader — reconstructs 2D display from packed 1D RGB stream.
//
// Same scanline-map approach as the composite stream shader, but the
// stream texture is RGBA8 (not R8) and contains raw {r, g, b} values.
// No palette lookup is performed.
static constexpr const char* fragment_src = R"glsl(
#version 130

in vec2 Frag_UV;
in vec4 Frag_Color;

uniform sampler2D StreamTex;    // unit 0: RGBA8 packed stream ({r, g, b, 0})

uniform int ScanlineMap[512];   // stream offset per visible scanline
uniform int StreamTexWidth;     // width of packed stream texture
uniform int DisplayHeight;      // number of visible scanlines
uniform int DisplayWidth;       // visible pixels per scanline

out vec4 Out_Color;

void main() {
    // Map UV to display pixel coordinates
    int scanline = int(Frag_UV.y * float(DisplayHeight));
    int pixel_x  = int(Frag_UV.x * float(DisplayWidth));

    // Clamp to valid range
    scanline = clamp(scanline, 0, DisplayHeight - 1);
    pixel_x  = clamp(pixel_x,  0, DisplayWidth  - 1);

    // Look up stream offset for this scanline
    int offset = ScanlineMap[scanline];
    if (offset < 0) {
        Out_Color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Compute position in the packed 1D stream
    int stream_pos = offset + pixel_x;
    int tex_row = stream_pos / StreamTexWidth;
    int tex_col = stream_pos - tex_row * StreamTexWidth;

    // Read RGB directly from stream texture
    vec4 rgb = texelFetch(StreamTex, ivec2(tex_col, tex_row), 0);
    Out_Color = Frag_Color * vec4(rgb.rgb, 1.0);
}
)glsl";

// ============================================================================
// Uniform locations
// ============================================================================

struct RGBShaderLocations {
    GLint proj_mtx;
    GLint scanline_map;
    GLint stream_tex_width;
    GLint display_height;
    GLint display_width;
};

// ============================================================================
// Shader program creation
// ============================================================================

// Create the RGB stream reconstruction shader program.
// Returns the program ID (0 on failure).
inline GLuint create_program(RGBShaderLocations* locs) {

    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, vertex_src);
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
        fprintf(stderr, "rgb_stream_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit binding — StreamTex on unit 0 (no Palette needed)
    gl_api::glUseProgram(prog);
    gl_api::glUniform1i(gl_api::glGetUniformLocation(prog, "StreamTex"), 0);
    gl_api::glUseProgram(0);

    if (locs) {
        locs->proj_mtx         = gl_api::glGetUniformLocation(prog, "ProjMtx");
        locs->scanline_map     = gl_api::glGetUniformLocation(prog, "ScanlineMap");
        locs->stream_tex_width = gl_api::glGetUniformLocation(prog, "StreamTexWidth");
        locs->display_height   = gl_api::glGetUniformLocation(prog, "DisplayHeight");
        locs->display_width    = gl_api::glGetUniformLocation(prog, "DisplayWidth");
    }

    printf("rgb_stream_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

// ============================================================================
// Stream texture management
// ============================================================================

// Create a 2D RGBA8 texture for the packed 1D RGB stream.
// Same dimensions as the composite stream texture, but RGBA8 format.
inline GLuint create_stream_texture(int max_stream_len) {
    int tex_height = (max_stream_len + stream_shader::STREAM_TEX_WIDTH - 1)
                   / stream_shader::STREAM_TEX_WIDTH;
    if (tex_height < 1) tex_height = 1;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, stream_shader::STREAM_TEX_WIDTH,
                 tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    printf("rgb_stream_shader: created %dx%d RGBA8 stream texture %u (max %d samples)\n",
           stream_shader::STREAM_TEX_WIDTH, tex_height, tex, max_stream_len);
    return tex;
}

// Upload raw 4-byte RGB samples to the RGBA8 stream texture.
// Same 1D→2D packing as the composite stream texture, but RGBA8 format.
inline void upload_stream_texture(GLuint tex, const uint8_t* raw_samples,
                                  uint32_t stream_len) {
    if (!tex || !raw_samples || stream_len == 0) return;

    int full_rows = static_cast<int>(stream_len) / stream_shader::STREAM_TEX_WIDTH;
    int remainder = static_cast<int>(stream_len) - full_rows * stream_shader::STREAM_TEX_WIDTH;

    glBindTexture(GL_TEXTURE_2D, tex);

    if (full_rows > 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        stream_shader::STREAM_TEX_WIDTH, full_rows,
                        GL_RGBA, GL_UNSIGNED_BYTE, raw_samples);
    }
    if (remainder > 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, full_rows,
                        remainder, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        raw_samples + full_rows * stream_shader::STREAM_TEX_WIDTH * 4);
    }
}

} // namespace rgb_stream_shader
