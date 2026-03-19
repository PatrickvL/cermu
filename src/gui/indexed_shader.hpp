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

#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdio>

// GL 3.0 constants not present in Windows gl.h (only exports GL 1.1)
#ifndef GL_R8
#define GL_R8 0x8229
#endif

namespace indexed_shader {

// ============================================================================
// GL 2.0+ function pointers — loaded at runtime via SDL_GL_GetProcAddress
// ============================================================================
// Windows opengl32.dll only exports GL 1.1; all higher functions must be
// resolved at runtime.  Call load_gl() once after creating the GL context.

// GL 1.3
inline void (APIENTRY* glActiveTexture)(GLenum) = nullptr;

// GL 2.0 — shader compilation
inline GLuint (APIENTRY* glCreateShader)(GLenum) = nullptr;
inline void   (APIENTRY* glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
inline void   (APIENTRY* glCompileShader)(GLuint) = nullptr;
inline void   (APIENTRY* glGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
inline void   (APIENTRY* glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
inline void   (APIENTRY* glDeleteShader)(GLuint) = nullptr;

// GL 2.0 — program linking
inline GLuint (APIENTRY* glCreateProgram)() = nullptr;
inline void   (APIENTRY* glAttachShader)(GLuint, GLuint) = nullptr;
inline void   (APIENTRY* glBindAttribLocation)(GLuint, GLuint, const GLchar*) = nullptr;
inline void   (APIENTRY* glLinkProgram)(GLuint) = nullptr;
inline void   (APIENTRY* glGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
inline void   (APIENTRY* glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
inline void   (APIENTRY* glDeleteProgram)(GLuint) = nullptr;

// GL 2.0 — program usage / uniforms
inline void   (APIENTRY* glUseProgram)(GLuint) = nullptr;
inline GLint  (APIENTRY* glGetUniformLocation)(GLuint, const GLchar*) = nullptr;
inline void   (APIENTRY* glUniform1i)(GLint, GLint) = nullptr;
inline void   (APIENTRY* glUniform1iv)(GLint, GLsizei, const GLint*) = nullptr;
inline void   (APIENTRY* glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;

// Load all GL function pointers.  Returns true if all critical functions
// were resolved.  Must be called after SDL_GL_CreateContext().
inline bool load_gl() {
    #define ISGL_LOAD(name) name = (decltype(name))SDL_GL_GetProcAddress("gl" #name + 2)
    // +2 skips the "gl" prefix already in the stringified name — giving us
    // the canonical GL function name.  E.g. "glCreateShader" + 2 won't work;
    // we need the real GL name.  Fix: use the actual GL entry point string.
    #undef ISGL_LOAD

    #define ISGL_LOAD(fn, gl_name) fn = (decltype(fn))SDL_GL_GetProcAddress(gl_name)
    ISGL_LOAD(glActiveTexture,       "glActiveTexture");
    ISGL_LOAD(glCreateShader,        "glCreateShader");
    ISGL_LOAD(glShaderSource,        "glShaderSource");
    ISGL_LOAD(glCompileShader,       "glCompileShader");
    ISGL_LOAD(glGetShaderiv,         "glGetShaderiv");
    ISGL_LOAD(glGetShaderInfoLog,    "glGetShaderInfoLog");
    ISGL_LOAD(glDeleteShader,        "glDeleteShader");
    ISGL_LOAD(glCreateProgram,       "glCreateProgram");
    ISGL_LOAD(glAttachShader,        "glAttachShader");
    ISGL_LOAD(glBindAttribLocation,  "glBindAttribLocation");
    ISGL_LOAD(glLinkProgram,         "glLinkProgram");
    ISGL_LOAD(glGetProgramiv,        "glGetProgramiv");
    ISGL_LOAD(glGetProgramInfoLog,   "glGetProgramInfoLog");
    ISGL_LOAD(glDeleteProgram,       "glDeleteProgram");
    ISGL_LOAD(glUseProgram,          "glUseProgram");
    ISGL_LOAD(glGetUniformLocation,  "glGetUniformLocation");
    ISGL_LOAD(glUniform1i,           "glUniform1i");
    ISGL_LOAD(glUniform1iv,          "glUniform1iv");
    ISGL_LOAD(glUniformMatrix4fv,    "glUniformMatrix4fv");
    #undef ISGL_LOAD

    bool ok = glCreateShader && glCreateProgram && glUseProgram
           && glActiveTexture && glUniformMatrix4fv && glDeleteProgram;
    if (!ok)
        fprintf(stderr, "indexed_shader: failed to load one or more GL functions\n");
    return ok;
}

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

// ============================================================================
// Shader compilation helpers
// ============================================================================

// Compile a shader stage and return its ID (0 on failure).
inline GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "indexed_shader: compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Create the indexed palette shader program.
// Returns the program ID (0 on failure).
// On success, writes uniform locations to the output parameters.
inline GLuint create_program(GLint* out_loc_proj, GLint* out_loc_palette) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) return 0;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    // Bind attribute locations to match ImGui's vertex layout.
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
        fprintf(stderr, "indexed_shader: link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }

    // Set texture unit bindings (Texture=0 set by ImGui, Palette=1 set by us).
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "Texture"), 0);
    glUniform1i(glGetUniformLocation(prog, "Palette"), 1);
    glUseProgram(0);

    if (out_loc_proj)    *out_loc_proj    = glGetUniformLocation(prog, "ProjMtx");
    if (out_loc_palette) *out_loc_palette = glGetUniformLocation(prog, "Palette");

    printf("indexed_shader: program %u compiled and linked successfully\n", prog);
    return prog;
}

} // namespace indexed_shader
