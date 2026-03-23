#pragma once

// ============================================================================
// OpenGL API Wrappers
// ============================================================================
//
// Consolidated GL 2.0+ / 3.0 function pointers and helpers.  Windows
// opengl32.dll only exports GL 1.1; all higher functions must be resolved
// at runtime via SDL_GL_GetProcAddress.  Call gl_api::load_gl() once
// after creating the GL context.
//
// This file is the single source of truth for all GL function pointers
// used by the shader and texture management code.  Individual shader
// headers include this file instead of declaring their own pointers.
//
// Requires: SDL2 (for SDL_GL_GetProcAddress), OpenGL 3.0 / GLSL 130
// ============================================================================

#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdio>

// GL 3.0 constants not present in Windows gl.h (only exports GL 1.1)
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace gl_api {

// ============================================================================
// GL 1.3 — multitexture
// ============================================================================
inline void (APIENTRY* glActiveTexture)(GLenum) = nullptr;

// ============================================================================
// GL 2.0 — shader compilation
// ============================================================================
inline GLuint (APIENTRY* glCreateShader)(GLenum) = nullptr;
inline void   (APIENTRY* glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
inline void   (APIENTRY* glCompileShader)(GLuint) = nullptr;
inline void   (APIENTRY* glGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
inline void   (APIENTRY* glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
inline void   (APIENTRY* glDeleteShader)(GLuint) = nullptr;

// ============================================================================
// GL 2.0 — program linking
// ============================================================================
inline GLuint (APIENTRY* glCreateProgram)() = nullptr;
inline void   (APIENTRY* glAttachShader)(GLuint, GLuint) = nullptr;
inline void   (APIENTRY* glBindAttribLocation)(GLuint, GLuint, const GLchar*) = nullptr;
inline void   (APIENTRY* glLinkProgram)(GLuint) = nullptr;
inline void   (APIENTRY* glGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
inline void   (APIENTRY* glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
inline void   (APIENTRY* glDeleteProgram)(GLuint) = nullptr;

// ============================================================================
// GL 2.0 — program usage / uniforms
// ============================================================================
inline void   (APIENTRY* glUseProgram)(GLuint) = nullptr;
inline GLint  (APIENTRY* glGetUniformLocation)(GLuint, const GLchar*) = nullptr;
inline void   (APIENTRY* glUniform1i)(GLint, GLint) = nullptr;
inline void   (APIENTRY* glUniform1iv)(GLint, GLsizei, const GLint*) = nullptr;
inline void   (APIENTRY* glUniform1f)(GLint, GLfloat) = nullptr;
inline void   (APIENTRY* glUniform2f)(GLint, GLfloat, GLfloat) = nullptr;
inline void   (APIENTRY* glUniform3f)(GLint, GLfloat, GLfloat, GLfloat) = nullptr;
inline void   (APIENTRY* glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;

// ============================================================================
// GL 2.0+ — VBO
// ============================================================================
inline void  (APIENTRY* glGenBuffers)(GLsizei, GLuint*) = nullptr;
inline void  (APIENTRY* glBindBuffer)(GLenum, GLuint) = nullptr;
inline void  (APIENTRY* glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
inline void  (APIENTRY* glDeleteBuffers)(GLsizei, const GLuint*) = nullptr;

// ============================================================================
// GL 3.0 — VAO
// ============================================================================
inline void  (APIENTRY* glGenVertexArrays)(GLsizei, GLuint*) = nullptr;
inline void  (APIENTRY* glDeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
inline void  (APIENTRY* glBindVertexArray)(GLuint) = nullptr;
inline void  (APIENTRY* glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
inline void  (APIENTRY* glEnableVertexAttribArray)(GLuint) = nullptr;

// ============================================================================
// GL 3.0 — FBO
// ============================================================================
inline void   (APIENTRY* glGenFramebuffers)(GLsizei, GLuint*) = nullptr;
inline void   (APIENTRY* glDeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
inline void   (APIENTRY* glBindFramebuffer)(GLenum, GLuint) = nullptr;
inline void   (APIENTRY* glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
inline GLenum (APIENTRY* glCheckFramebufferStatus)(GLenum) = nullptr;

// ============================================================================
// Load all GL function pointers
// ============================================================================
//
// Returns true if all critical functions were resolved.
// Must be called after SDL_GL_CreateContext().

inline bool load_gl() {
    #define GL_API_LOAD(fn, gl_name) fn = (decltype(fn))SDL_GL_GetProcAddress(gl_name)

    // GL 1.3
    GL_API_LOAD(glActiveTexture,       "glActiveTexture");

    // GL 2.0 — shader compilation
    GL_API_LOAD(glCreateShader,        "glCreateShader");
    GL_API_LOAD(glShaderSource,        "glShaderSource");
    GL_API_LOAD(glCompileShader,       "glCompileShader");
    GL_API_LOAD(glGetShaderiv,         "glGetShaderiv");
    GL_API_LOAD(glGetShaderInfoLog,    "glGetShaderInfoLog");
    GL_API_LOAD(glDeleteShader,        "glDeleteShader");

    // GL 2.0 — program linking
    GL_API_LOAD(glCreateProgram,       "glCreateProgram");
    GL_API_LOAD(glAttachShader,        "glAttachShader");
    GL_API_LOAD(glBindAttribLocation,  "glBindAttribLocation");
    GL_API_LOAD(glLinkProgram,         "glLinkProgram");
    GL_API_LOAD(glGetProgramiv,        "glGetProgramiv");
    GL_API_LOAD(glGetProgramInfoLog,   "glGetProgramInfoLog");
    GL_API_LOAD(glDeleteProgram,       "glDeleteProgram");

    // GL 2.0 — program usage / uniforms
    GL_API_LOAD(glUseProgram,          "glUseProgram");
    GL_API_LOAD(glGetUniformLocation,  "glGetUniformLocation");
    GL_API_LOAD(glUniform1i,           "glUniform1i");
    GL_API_LOAD(glUniform1iv,          "glUniform1iv");
    GL_API_LOAD(glUniform1f,           "glUniform1f");
    GL_API_LOAD(glUniform2f,           "glUniform2f");
    GL_API_LOAD(glUniform3f,           "glUniform3f");
    GL_API_LOAD(glUniformMatrix4fv,    "glUniformMatrix4fv");

    // VBO
    GL_API_LOAD(glGenBuffers,          "glGenBuffers");
    GL_API_LOAD(glBindBuffer,          "glBindBuffer");
    GL_API_LOAD(glBufferData,          "glBufferData");
    GL_API_LOAD(glDeleteBuffers,       "glDeleteBuffers");

    // VAO
    GL_API_LOAD(glGenVertexArrays,     "glGenVertexArrays");
    GL_API_LOAD(glDeleteVertexArrays,  "glDeleteVertexArrays");
    GL_API_LOAD(glBindVertexArray,     "glBindVertexArray");
    GL_API_LOAD(glVertexAttribPointer, "glVertexAttribPointer");
    GL_API_LOAD(glEnableVertexAttribArray, "glEnableVertexAttribArray");

    // FBO
    GL_API_LOAD(glGenFramebuffers,         "glGenFramebuffers");
    GL_API_LOAD(glDeleteFramebuffers,      "glDeleteFramebuffers");
    GL_API_LOAD(glBindFramebuffer,         "glBindFramebuffer");
    GL_API_LOAD(glFramebufferTexture2D,    "glFramebufferTexture2D");
    GL_API_LOAD(glCheckFramebufferStatus,  "glCheckFramebufferStatus");

    #undef GL_API_LOAD

    bool ok = glCreateShader && glCreateProgram && glUseProgram
           && glActiveTexture && glUniformMatrix4fv && glDeleteProgram
           && glGenBuffers && glBindBuffer && glBufferData
           && glGenVertexArrays && glBindVertexArray
           && glVertexAttribPointer && glEnableVertexAttribArray
           && glUniform1f && glUniform2f && glUniform3f
           && glGenFramebuffers && glBindFramebuffer
           && glFramebufferTexture2D && glCheckFramebufferStatus;
    if (!ok)
        fprintf(stderr, "gl_api: failed to load one or more GL functions\n");
    return ok;
}

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
        fprintf(stderr, "gl_api: compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace gl_api
