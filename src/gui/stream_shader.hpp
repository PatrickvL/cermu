#pragma once

// ============================================================================
// GPU Stream Reconstruction Shader
// ============================================================================
//
// Fragment shader for direct 1D-stream → 2D-display reconstruction.
// Replaces the CPU-side reconstruct_to_framebuffer() bridge — the GPU
// reads the raw video stream (per-dot-clock color indices), uses a
// scanline offset map to locate each line's visible pixel data, and
// performs palette lookup in a single draw call.
//
// Textures:
//   unit 0: Stream texture (R8, packed 1D→2D, STREAM_TEX_WIDTH wide)
//   unit 1: Palette texture (RGBA, 256×1)
//
// Uniforms:
//   ProjMtx:        mat4 projection (matches ImGui's layout)
//   ScanlineMap:    int[MAX_SCANLINES] — stream position of first visible
//                   pixel for each scanline (-1 = no data / VBlank)
//   StreamTexWidth: int — width of the packed stream texture
//   DisplayHeight:  int — number of visible scanlines
//   DisplayWidth:   int — visible pixels per scanline
//
// The scanline map is computed on the CPU (trivially cheap: walk the sync
// event list, ~312 iterations).  Everything else runs on the GPU.
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "gui/indexed_shader.hpp"   // GL function pointers, compile_shader()
#include "core/signal/sync_types.hpp"
#include <cstdio>
#include <cstdint>

namespace stream_shader {

// Maximum visible scanlines any system can produce.
// PAL VIC-II: 284, NTSC: 234, PAL TED: 288.  512 gives ample headroom.
inline constexpr int MAX_SCANLINES = 512;

// Width of the packed stream texture.  The 1D stream (up to ~160K samples
// for PAL VIC-II) is stored row-major in a 2D R8 texture this many
// texels wide.  1024 keeps total rows reasonable (~157 for a 160K stream)
// and is universally supported by GL 3.0 drivers.
inline constexpr int STREAM_TEX_WIDTH = 1024;

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

// Fragment shader — reconstructs 2D display from packed 1D stream.
//
// For each output pixel (u, v):
//   1. v → scanline index (0..DisplayHeight-1)
//   2. Look up ScanlineMap[scanline] → stream offset of first visible pixel
//   3. u → pixel-within-line: stream_pos = offset + pixel_x
//   4. Unpack 2D coordinates in stream texture: row = pos / width, col = pos % width
//   5. texelFetch → R8 color index
//   6. texelFetch palette → RGBA color
static constexpr const char* fragment_src = R"glsl(
#version 130

in vec2 Frag_UV;
in vec4 Frag_Color;

uniform sampler2D StreamTex;    // unit 0: R8 packed stream
uniform sampler2D Palette;      // unit 1: 256×1 RGBA

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
        // VBlank or missing line — black
        Out_Color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Compute position in the packed 1D stream
    int stream_pos = offset + pixel_x;
    int tex_row = stream_pos / StreamTexWidth;
    int tex_col = stream_pos - tex_row * StreamTexWidth;  // mod without %

    // Read color index from stream texture (R8 normalized → int)
    float idx_f = texelFetch(StreamTex, ivec2(tex_col, tex_row), 0).r;
    int idx = int(idx_f * 255.0 + 0.5);

    // Palette lookup
    vec4 color = texelFetch(Palette, ivec2(idx, 0), 0);
    Out_Color = Frag_Color * color;
}
)glsl";

// ============================================================================
// Scanline map computation — CPU side
// ============================================================================
//
// Walks the sync event array from a FrameData and fills scanline_offsets
// with the stream position of the first visible pixel on each line.
// Returns the number of visible scanlines found.

// Compute scanline map from sync events.
// offsets[i] = stream position of first visible pixel on scanline i.
// offsets[i] = -1 for VBlank lines or lines beyond the frame.
// Returns the number of visible scanlines filled.
inline int compute_scanline_map(
    int* offsets,
    int max_scanlines,
    const SyncEvent* sync_events,
    uint32_t sync_count,
    int back_porch_pixels,
    uint32_t stream_len)
{
    // Initialize all entries to -1 (no data)
    for (int i = 0; i < max_scanlines; i++)
        offsets[i] = -1;

    int scanline = 0;
    for (uint32_t i = 0; i < sync_count && scanline < max_scanlines; i++) {
        if (sync_events[i].type != SyncType::HSync)
            continue;

        // Skip VBlank lines (VSync flag set)
        if (has_flag(sync_events[i].flags, VideoFlags::VSync))
            continue;

        uint32_t line_start = sync_events[i].stream_pos + back_porch_pixels;
        if (line_start < stream_len) {
            offsets[scanline] = static_cast<int>(line_start);
        }
        scanline++;
    }
    return scanline;
}

// ============================================================================
// Shader program creation
// ============================================================================

struct StreamShaderLocations {
    GLint proj_mtx;
    GLint scanline_map;
    GLint stream_tex_width;
    GLint display_height;
    GLint display_width;
};

// Create the stream reconstruction shader program.
// Returns the program ID (0 on failure).
inline GLuint create_program(StreamShaderLocations* locs) {
    using namespace indexed_shader;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) return 0;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    // Bind attribute locations to match ImGui's vertex layout
    glBindAttribLocation(prog, 0, "Position");
    glBindAttribLocation(prog, 1, "UV");
    glBindAttribLocation(prog, 2, "Color");

    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "stream_shader: link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit bindings
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "StreamTex"), 0);
    glUniform1i(glGetUniformLocation(prog, "Palette"), 1);
    glUseProgram(0);

    if (locs) {
        locs->proj_mtx        = glGetUniformLocation(prog, "ProjMtx");
        locs->scanline_map    = glGetUniformLocation(prog, "ScanlineMap");
        locs->stream_tex_width = glGetUniformLocation(prog, "StreamTexWidth");
        locs->display_height  = glGetUniformLocation(prog, "DisplayHeight");
        locs->display_width   = glGetUniformLocation(prog, "DisplayWidth");
    }

    printf("stream_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

// ============================================================================
// Stream texture management
// ============================================================================

// Create a 2D R8 texture for the packed 1D stream.
// Height is computed from max_stream_len / STREAM_TEX_WIDTH, rounded up.
inline GLuint create_stream_texture(int max_stream_len) {
    int tex_height = (max_stream_len + STREAM_TEX_WIDTH - 1) / STREAM_TEX_WIDTH;
    if (tex_height < 1) tex_height = 1;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, STREAM_TEX_WIDTH, tex_height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    printf("stream_shader: created %dx%d R8 stream texture %u (max %d samples)\n",
           STREAM_TEX_WIDTH, tex_height, tex, max_stream_len);
    return tex;
}

// Upload stream data to the stream texture.
// The 1D stream is packed row-major: first STREAM_TEX_WIDTH bytes go to row 0, etc.
// Only the color_index bytes are uploaded; flags are not needed on the GPU
// (sync classification already happened on the CPU to produce the scanline map).
//
// extract_buf must be at least stream_len bytes — caller provides it to avoid
// per-frame allocation.  The function extracts color_index from each
// CompositeVideoSample (2-byte stride: [color_index, flags]).
inline void upload_stream_texture(GLuint tex, const void* stream_samples,
                                  uint32_t stream_len, uint8_t* extract_buf) {
    if (!tex || !stream_samples || stream_len == 0 || !extract_buf) return;

    // Extract color_index bytes from interleaved [color_index, flags] pairs
    const uint8_t* src = static_cast<const uint8_t*>(stream_samples);
    for (uint32_t i = 0; i < stream_len; i++) {
        extract_buf[i] = src[i * 2];   // CompositeVideoSample.color_index is first byte
    }

    int full_rows = static_cast<int>(stream_len) / STREAM_TEX_WIDTH;
    int remainder = static_cast<int>(stream_len) - full_rows * STREAM_TEX_WIDTH;

    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Upload complete rows
    if (full_rows > 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        STREAM_TEX_WIDTH, full_rows,
                        GL_RED, GL_UNSIGNED_BYTE, extract_buf);
    }

    // Upload partial last row
    if (remainder > 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, full_rows,
                        remainder, 1,
                        GL_RED, GL_UNSIGNED_BYTE,
                        extract_buf + full_rows * STREAM_TEX_WIDTH);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

} // namespace stream_shader
