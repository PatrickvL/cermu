#pragma once

// ============================================================================
// CRT Post-Processing Shader
// ============================================================================
//
// Single-pass CRT simulation applied as a fullscreen post-processing pass.
// Reads the emulated display from a texture and applies:
//   - Barrel distortion (screen curvature)
//   - Scanline darkening
//   - Shadow mask / aperture grille simulation
//   - Brightness / contrast / gamma correction
//
// Uses an FBO to capture the intermediate display output, then renders
// a fullscreen triangle with the CRT shader applied.
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "core/cermu.hpp"
#include "devices/display/display_device.hpp"
#include "gui/gl_api.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace crt_shader {

// ============================================================================
// GLSL sources
// ============================================================================

// Vertex shader — fullscreen triangle via gl_VertexID (no VBO needed).
// Applies UV rotation for systems with rotated monitors (e.g. arcade portrait).
static constexpr const char* vertex_src = R"glsl(
#version 130
out vec2 v_uv;
uniform int Rotation;  // 0=none, 1=CW90, 2=CW180, 3=CW270
void main() {
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    v_uv = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    // Rotate UV coordinates to match physical monitor orientation
    if (Rotation == 1) {       // CW90  — portrait, top-right becomes top-left
        v_uv = vec2(v_uv.y, 1.0 - v_uv.x);
    } else if (Rotation == 2) { // CW180 — upside-down
        v_uv = vec2(1.0 - v_uv.x, 1.0 - v_uv.y);
    } else if (Rotation == 3) { // CW270 — portrait, top-left becomes top-right
        v_uv = vec2(1.0 - v_uv.y, v_uv.x);
    }
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)glsl";

// Fragment shader — single-pass CRT simulation.
static constexpr const char* fragment_src = R"glsl(
#version 130
in vec2 v_uv;
uniform sampler2D InputTexture;
uniform sampler2D PrevFrameTexture; // Previous frame (for phosphor persistence)
uniform vec2  InputSize;      // emulated framebuffer size in pixels
uniform vec2  OutputSize;     // display output size in pixels
uniform float Curvature;      // 0 = flat, 1 = max barrel distortion
uniform float ScanlineGap;    // 0 = no scanlines, 1 = full black lines
uniform float DotPitch;       // shadow mask dot pitch (mm), 0 = disabled
uniform float Brightness;     // multiplier, 1.0 = neutral
uniform float Contrast;       // multiplier, 1.0 = neutral
uniform float Gamma;          // display gamma, typically 2.2
uniform int   MaskType;       // 0=shadow mask, 1=aperture grille, 2=slot mask, 3=mono, 4=none
uniform vec3  PhosphorTint;   // Phosphor glow color (from PhosphorParams.glow_color)
uniform vec3  ColorTempTint;  // Color temperature tint (normalized to 6500K reference)
uniform float MaskOpacity;    // Shadow mask blend strength (MaskParams.opacity)
uniform float ScanlineStrength; // Scanline darkening intensity (ScanlineParams.strength)
uniform vec2  ConvergenceError; // RGB convergence misalignment in texels (BeamParams)
uniform float VignetteStrength; // Screen-edge darkening (OpticsParams)
uniform vec3  GlassTint;      // CRT glass color filter (OpticsParams)
uniform float TriadSize;       // Mask pattern scale factor (MaskParams.triad_size)
uniform float SlotMaskWidth;   // Aperture open ratio for grille types (MaskParams)
uniform float ScanlinePhase;   // Sub-pixel phase offset per scanline (ScanlineParams)
uniform int   Interlace;       // 1 = interlaced field alternation, 0 = progressive
uniform float NoiseLevel;      // Per-pixel random noise intensity (SignalParams)
uniform float HumBarStrength;  // AC hum bar brightness modulation (SignalParams)
uniform float GhostingStrength; // Composite echo/ghosting (SignalParams)
uniform float ChromaPhaseError; // Chroma hue rotation in degrees (SignalParams)
uniform float SyncStability;   // Horizontal sync jitter (SignalParams, 1.0 = perfect)
uniform int   FrameCount;      // Monotonic frame counter for temporal effects
uniform float Persistence;     // Phosphor decay factor per frame (0=none, ~0.5=moderate)
// TODO: Add uniforms for remaining DisplayCharacteristics fields:
//   PhosphorParams  — float Persistence, float BloomRadius,
//                     float BloomThreshold, int DecayCurve
//   BeamParams      — float BeamWidth, float BeamSoftness, float Pincushion,
//                     float HLinearity, float VLinearity, vec4 CornerPin
//   SignalParams    — float Bandwidth
//   OpticsParams    — float ReflectionStrength, float EdgeGlow
out vec4 Out_Color;

// Fast integer hash for noise generation (Wang hash).
float hash_noise(vec2 co, int frame) {
    uint n = uint(co.x * 1973.0 + co.y * 9277.0 + float(frame) * 26699.0);
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7FFFFFFFu) / float(0x7FFFFFFF);
}

// Barrel distortion for CRT screen curvature.
vec2 barrel_distort(vec2 uv, float k) {
    vec2 cc = uv - 0.5;
    float r2 = dot(cc, cc);
    float f = 1.0 + r2 * k * 2.5;
    return cc * f + 0.5;
}

// Shadow mask pattern (RGB vertical stripes, scaled by TriadSize).
vec3 shadow_mask(vec2 frag_coord, int mask_type, float triad_sz, float slot_w) {
    if (mask_type >= 4) return vec3(1.0);  // none

    // Scale coordinates by triad size
    vec2 fc = frag_coord / max(triad_sz, 0.1);
    int ix = int(fc.x);
    int iy = int(fc.y);

    if (mask_type == 1) {
        // Aperture grille — vertical RGB stripes with configurable open ratio
        float open = clamp(slot_w, 0.1, 1.0);
        int phase = ix % 3;
        float dim = 1.0 - (1.0 - open) * 0.6;  // darker in closed portion
        if (phase == 0) return vec3(1.0, dim, dim);
        if (phase == 1) return vec3(dim, 1.0, dim);
        return vec3(dim, dim, 1.0);
    } else if (mask_type == 2) {
        // Slot mask — alternating offset RGB triads
        int phase = (ix + (iy / 2) * 1) % 3;
        float slot = ((iy % 4) < 3) ? 1.0 : 0.75;
        vec3 rgb;
        if (phase == 0)      rgb = vec3(1.0, 0.5, 0.5);
        else if (phase == 1) rgb = vec3(0.5, 1.0, 0.5);
        else                 rgb = vec3(0.5, 0.5, 1.0);
        return rgb * slot;
    } else if (mask_type == 3) {
        // Monochrome — no color mask
        return vec3(1.0);
    } else {
        // Shadow mask — classic triad pattern with row offset
        int phase = (ix + (iy % 2) * 1) % 3;
        if (phase == 0) return vec3(1.0, 0.6, 0.6);
        if (phase == 1) return vec3(0.6, 1.0, 0.6);
        return vec3(0.6, 0.6, 1.0);
    }
}

void main() {
    // TODO: Apply BeamParams.pincushion (separate from barrel curvature)
    // TODO: Apply BeamParams.h_linearity / v_linearity stretch correction
    // TODO: Apply BeamParams.corner_pin trapezoid warp

    // Apply barrel distortion
    vec2 uv = (Curvature > 0.001) ? barrel_distort(v_uv, Curvature) : v_uv;

    // Black outside the curved screen area
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        Out_Color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // TODO: Apply SignalParams.bandwidth — low-pass filter (horizontal blur proportional
    //       to 1/bandwidth) to simulate analogue bandwidth limiting

    // Sync instability — per-line horizontal jitter (composite/RF artifact)
    if (SyncStability < 0.999) {
        float line = floor(uv.y * InputSize.y);
        float jitter = (hash_noise(vec2(line, 0.0), FrameCount) - 0.5) * (1.0 - SyncStability) * 0.01;
        uv.x += jitter;
    }

    // Sample the input texture with per-channel convergence error (RGB misalignment)
    vec2 texel = 1.0 / InputSize;
    vec2 conv = ConvergenceError * texel;
    vec3 color;
    color.r = texture(InputTexture, uv + vec2(-conv.x, -conv.y)).r;
    color.g = texture(InputTexture, uv).g;
    color.b = texture(InputTexture, uv + vec2( conv.x,  conv.y)).b;

    // Ghosting — composite echo from impedance mismatch / multipath
    if (GhostingStrength > 0.001) {
        vec3 ghost = texture(InputTexture, uv + vec2(3.0 / InputSize.x, 0.0)).rgb;
        color = mix(color, ghost, GhostingStrength);
    }

    // Chroma phase error — hue rotation via YIQ transform
    if (abs(ChromaPhaseError) > 0.01) {
        float y = dot(color, vec3(0.299, 0.587, 0.114));
        float i = dot(color, vec3(0.596, -0.274, -0.322));
        float q = dot(color, vec3(0.211, -0.523,  0.312));
        float angle = radians(ChromaPhaseError);
        float ca = cos(angle), sa = sin(angle);
        float i2 = i * ca - q * sa;
        float q2 = i * sa + q * ca;
        color = vec3(y + 0.956*i2 + 0.621*q2,
                     y - 0.272*i2 - 0.647*q2,
                     y - 1.106*i2 + 1.703*q2);
        color = clamp(color, 0.0, 1.0);
    }

    // TODO: Apply BeamParams.width / softness — gaussian beam profile per scanline

    // Scanline darkening — darken pixels between emulated scan lines
    if (ScanlineGap > 0.001) {
        float scanline_y = uv.y * InputSize.y + ScanlinePhase;
        float scanline_frac = fract(scanline_y);
        // Interlace: darken alternate fields each frame
        if (Interlace > 0) {
            int line = int(floor(scanline_y));
            if ((line + FrameCount) % 2 == 0)
                scanline_frac = 1.0;  // force full darkening on inactive field line
        }
        // Darken near the boundary between scanlines
        float line_dist = abs(scanline_frac - 0.5) * 2.0;  // 0 at center, 1 at edge
        float scanline_mask = 1.0 - ScanlineGap * ScanlineStrength * smoothstep(0.4, 1.0, line_dist);
        color *= scanline_mask;
    }

    // Shadow mask / aperture grille
    if (DotPitch > 0.001) {
        vec3 mask = shadow_mask(gl_FragCoord.xy, MaskType, TriadSize, SlotMaskWidth);
        color *= mix(vec3(1.0), mask, MaskOpacity);
    }

    // Monochrome phosphor — convert to luminance and apply glow color
    if (MaskType == 3) {
        float luma = dot(color, vec3(0.299, 0.587, 0.114));
        color = vec3(luma) * PhosphorTint;
    }

    // Brightness and contrast
    color = (color - 0.5) * Contrast + 0.5;
    color *= Brightness;

    // Gamma correction
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / Gamma));

    // Color temperature tint (pre-computed on CPU, normalized to 6500K)
    color *= ColorTempTint;

    // Vignette — darken screen edges based on distance from center
    if (VignetteStrength > 0.001) {
        vec2 vc = v_uv - 0.5;
        color *= 1.0 - VignetteStrength * dot(vc, vc) * 4.0;
    }

    // Glass tint — CRT faceplate color filter
    color *= GlassTint;

    // Phosphor persistence — blend with decayed previous frame.
    // CRT phosphors continue to glow after the beam passes, creating
    // temporal smoothing that makes sprite flicker invisible on real TVs.
    if (Persistence > 0.001) {
        vec3 prev = texture(PrevFrameTexture, v_uv).rgb;
        color = max(color, prev * Persistence);
    }

    // Analogue noise — per-pixel random intensity variation
    if (NoiseLevel > 0.001) {
        float n = hash_noise(gl_FragCoord.xy, FrameCount) - 0.5;
        color += vec3(n * NoiseLevel);
    }

    // Hum bar — slow-moving horizontal brightness band from AC coupling
    if (HumBarStrength > 0.001) {
        float phase = float(FrameCount) * 0.02 + v_uv.y * 6.2832;
        color *= 1.0 + sin(phase) * HumBarStrength;
    }

    // TODO: Apply OpticsParams.reflection_strength — specular highlight overlay
    // TODO: Apply OpticsParams.edge_glow — bright halo at screen perimeter

    Out_Color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)glsl";

// ============================================================================
// CRT Post-Process state
// ============================================================================

struct CRTPostProcess {
    GLuint fbo      = 0;    ///< Framebuffer object for capturing display output
    GLuint texture  = 0;    ///< Color attachment (RGBA8)
    GLuint prev_frame_tex = 0; ///< Previous frame texture (for phosphor persistence)
    GLuint shader   = 0;    ///< CRT shader program
    GLuint dummy_vao = 0;   ///< Empty VAO for gl_VertexID fullscreen triangle
    int    width    = 0;
    int    height   = 0;

    // Uniform locations
    GLint loc_input_texture  = -1;
    GLint loc_prev_frame_texture = -1;
    GLint loc_input_size     = -1;
    GLint loc_output_size    = -1;
    GLint loc_curvature      = -1;
    GLint loc_scanline_gap   = -1;
    GLint loc_dot_pitch      = -1;
    GLint loc_brightness     = -1;
    GLint loc_contrast       = -1;
    GLint loc_gamma          = -1;
    GLint loc_mask_type      = -1;
    GLint loc_phosphor_tint  = -1;
    GLint loc_color_temp_tint = -1;
    GLint loc_mask_opacity       = -1;
    GLint loc_scanline_strength  = -1;
    GLint loc_convergence_error  = -1;
    GLint loc_vignette_strength  = -1;
    GLint loc_glass_tint         = -1;
    GLint loc_triad_size         = -1;
    GLint loc_slot_mask_width    = -1;
    GLint loc_scanline_phase     = -1;
    GLint loc_interlace          = -1;
    GLint loc_noise_level        = -1;
    GLint loc_hum_bar_strength   = -1;
    GLint loc_ghosting_strength  = -1;
    GLint loc_chroma_phase_error = -1;
    GLint loc_sync_stability     = -1;
    GLint loc_frame_count        = -1;
    GLint loc_rotation           = -1;
    GLint loc_persistence        = -1;
    // TODO: Add uniform locations for remaining fields when implemented:
    //   PhosphorParams:  loc_persistence, loc_bloom_radius,
    //                    loc_bloom_threshold, loc_decay_curve
    //   BeamParams:      loc_beam_width, loc_beam_softness, loc_pincushion,
    //                    loc_h_linearity, loc_v_linearity, loc_corner_pin
    //   OpticsParams:    loc_reflection_strength, loc_edge_glow
    //   SignalParams:    loc_bandwidth

    uint32_t frame_count = 0;  ///< Monotonic frame counter for temporal effects
};

// ============================================================================
// Lifecycle
// ============================================================================

/// Create the CRT post-processing resources (FBO, shader, VAO).
/// Returns true on success.
inline bool create(CRTPostProcess* p, int w, int h) {
    p->width  = w;
    p->height = h;

    // --- FBO texture ---
    glGenTextures(1, &p->texture);
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Previous frame texture (for phosphor persistence) ---
    glGenTextures(1, &p->prev_frame_tex);
    glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- FBO ---
    gl_api::glGenFramebuffers(1, &p->fbo);
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    gl_api::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p->texture, 0);
    GLenum status = gl_api::glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        log_error("crt_shader: FBO incomplete (0x%x)\n", status);
        return false;
    }

    // --- Shader ---
    GLuint vs = gl_api::compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) return false;
    GLuint fs = gl_api::compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) { gl_api::glDeleteShader(vs); return false; }

    p->shader = gl_api::glCreateProgram();
    gl_api::glAttachShader(p->shader, vs);
    gl_api::glAttachShader(p->shader, fs);
    gl_api::glLinkProgram(p->shader);
    gl_api::glDeleteShader(vs);
    gl_api::glDeleteShader(fs);

    GLint link_ok = 0;
    gl_api::glGetProgramiv(p->shader, GL_LINK_STATUS, &link_ok);
    if (link_ok != GL_TRUE) {
        char log[512];
        gl_api::glGetProgramInfoLog(p->shader, sizeof(log), nullptr, log);
        log_error("crt_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(p->shader);
        p->shader = 0;
        return false;
    }

    // --- Uniform locations ---
    p->loc_input_texture = gl_api::glGetUniformLocation(p->shader, "InputTexture");
    p->loc_prev_frame_texture = gl_api::glGetUniformLocation(p->shader, "PrevFrameTexture");
    p->loc_input_size    = gl_api::glGetUniformLocation(p->shader, "InputSize");
    p->loc_output_size   = gl_api::glGetUniformLocation(p->shader, "OutputSize");
    p->loc_curvature     = gl_api::glGetUniformLocation(p->shader, "Curvature");
    p->loc_scanline_gap  = gl_api::glGetUniformLocation(p->shader, "ScanlineGap");
    p->loc_dot_pitch     = gl_api::glGetUniformLocation(p->shader, "DotPitch");
    p->loc_brightness    = gl_api::glGetUniformLocation(p->shader, "Brightness");
    p->loc_contrast      = gl_api::glGetUniformLocation(p->shader, "Contrast");
    p->loc_gamma         = gl_api::glGetUniformLocation(p->shader, "Gamma");
    p->loc_mask_type     = gl_api::glGetUniformLocation(p->shader, "MaskType");
    p->loc_phosphor_tint = gl_api::glGetUniformLocation(p->shader, "PhosphorTint");
    p->loc_color_temp_tint = gl_api::glGetUniformLocation(p->shader, "ColorTempTint");
    p->loc_mask_opacity      = gl_api::glGetUniformLocation(p->shader, "MaskOpacity");
    p->loc_scanline_strength = gl_api::glGetUniformLocation(p->shader, "ScanlineStrength");
    p->loc_convergence_error = gl_api::glGetUniformLocation(p->shader, "ConvergenceError");
    p->loc_vignette_strength = gl_api::glGetUniformLocation(p->shader, "VignetteStrength");
    p->loc_glass_tint        = gl_api::glGetUniformLocation(p->shader, "GlassTint");
    p->loc_triad_size        = gl_api::glGetUniformLocation(p->shader, "TriadSize");
    p->loc_slot_mask_width   = gl_api::glGetUniformLocation(p->shader, "SlotMaskWidth");
    p->loc_scanline_phase    = gl_api::glGetUniformLocation(p->shader, "ScanlinePhase");
    p->loc_interlace         = gl_api::glGetUniformLocation(p->shader, "Interlace");
    p->loc_noise_level       = gl_api::glGetUniformLocation(p->shader, "NoiseLevel");
    p->loc_hum_bar_strength  = gl_api::glGetUniformLocation(p->shader, "HumBarStrength");
    p->loc_ghosting_strength = gl_api::glGetUniformLocation(p->shader, "GhostingStrength");
    p->loc_chroma_phase_error = gl_api::glGetUniformLocation(p->shader, "ChromaPhaseError");
    p->loc_sync_stability    = gl_api::glGetUniformLocation(p->shader, "SyncStability");
    p->loc_frame_count       = gl_api::glGetUniformLocation(p->shader, "FrameCount");
    p->loc_rotation          = gl_api::glGetUniformLocation(p->shader, "Rotation");
    p->loc_persistence       = gl_api::glGetUniformLocation(p->shader, "Persistence");

    // Set texture units (InputTexture=0, PrevFrameTexture=1)
    gl_api::glUseProgram(p->shader);
    gl_api::glUniform1i(p->loc_input_texture, 0);
    gl_api::glUniform1i(p->loc_prev_frame_texture, 1);
    gl_api::glUseProgram(0);

    // --- Empty VAO for gl_VertexID rendering ---
    gl_api::glGenVertexArrays(1, &p->dummy_vao);

    return true;
}

/// Resize the CRT FBO if dimensions changed.
inline void resize(CRTPostProcess* p, int w, int h) {
    if (p->width == w && p->height == h) return;
    p->width  = w;
    p->height = h;
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/// Destroy CRT post-processing resources.
inline void destroy(CRTPostProcess* p) {
    if (p->fbo)            { gl_api::glDeleteFramebuffers(1, &p->fbo); p->fbo = 0; }
    if (p->texture)        { glDeleteTextures(1, &p->texture); p->texture = 0; }
    if (p->prev_frame_tex) { glDeleteTextures(1, &p->prev_frame_tex); p->prev_frame_tex = 0; }
    if (p->shader)         { gl_api::glDeleteProgram(p->shader); p->shader = 0; }
    if (p->dummy_vao)      { gl_api::glDeleteVertexArrays(1, &p->dummy_vao); p->dummy_vao = 0; }
}

/// Map DisplayTechnology enum to mask type int for the shader.
inline int mask_type_from_technology(int technology) {
    // Maps to MaskType uniform: 0=shadow, 1=aperture, 2=slot, 3=mono, 4=none
    switch (technology) {
        case 0:  return 0;  // CRT_Shadow
        case 1:  return 1;  // CRT_Aperture
        case 2:  return 2;  // CRT_SlotMask
        case 3:  return 3;  // CRT_Monochrome
        case 4:  return 4;  // CRT_Vector → no mask
        case 5:  return 4;  // LCD → no mask
        case 6:  return 4;  // LED → no mask
        default: return 4;
    }
}

/// Convert color temperature (Kelvin) to RGB multipliers.
/// Based on Tanner Helland’s approximation of the Planckian locus.
inline void color_temp_to_rgb(float temp_k, float& r, float& g, float& b) {
    float t = temp_k / 100.0f;
    // Red
    if (t <= 66.0f)
        r = 1.0f;
    else
        r = 1.29293618606f * powf(t - 60.0f, -0.1332047592f);
    // Green
    if (t <= 66.0f)
        g = 0.39008157876f * logf(t) - 0.63184144378f;
    else
        g = 1.12989086090f * powf(t - 60.0f, -0.0755148492f);
    // Blue
    if (t >= 66.0f)
        b = 1.0f;
    else if (t <= 19.0f)
        b = 0.0f;
    else
        b = 0.54320678911f * logf(t - 10.0f) - 1.19625408914f;
    // Clamp
    r = r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
    g = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
    b = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
}

/// Compute a color temperature tint normalized to a 6500K reference white.
/// At 6500K the tint is (1,1,1).  Lower temps shift warm, higher shift cool.
inline void color_temp_tint(float temp_k, float& tr, float& tg, float& tb) {
    float r, g, b, rr, rg, rb;
    color_temp_to_rgb(temp_k, r, g, b);
    color_temp_to_rgb(6500.0f, rr, rg, rb);  // reference D65
    tr = (rr > 0.001f) ? r / rr : 1.0f;
    tg = (rg > 0.001f) ? g / rg : 1.0f;
    tb = (rb > 0.001f) ? b / rb : 1.0f;
}

// ============================================================================
// Render pass
// ============================================================================

/// Render the CRT post-processing pass.
/// Reads from `input_tex`, writes to `p->texture` via the FBO.
/// The caller can then display `p->texture` via ImGui::Image().
///
/// @param p            CRT post-process state
/// @param input_tex    Source texture (the emulated display output)
/// @param input_w      Source texture width (emulated framebuffer)
/// @param input_h      Source texture height (emulated framebuffer)
/// @param output_w     Display output width (screen pixels)
/// @param output_h     Display output height (screen pixels)
/// @param dc           Display characteristics (phosphor, beam, mask, etc.)
/// @param rotation     Monitor rotation (DisplayRotation enum value)
inline void render(CRTPostProcess* p,
                   GLuint input_tex,
                   float input_w, float input_h,
                   float output_w, float output_h,
                   const DisplayCharacteristics& dc,
                   int rotation = 0) {
    if (!p->shader || !p->fbo) return;

    // Unpack fields wired to shader uniforms
    float curvature    = dc.optics.curvature;
    float scanline_gap = dc.scanlines.gap;
    float dot_pitch    = dc.dot_pitch;
    float brightness   = dc.brightness;
    float contrast     = dc.contrast;
    float gamma        = dc.gamma;
    int   mask_type    = mask_type_from_technology(static_cast<int>(dc.technology));
    float color_temp_k = dc.color_temp;

    // TODO: Wire remaining DisplayCharacteristics sub-struct fields to uniforms:
    //   dc.beam      — width, softness, pincushion, h/v_linearity, corner_pin
    //   dc.optics    — reflection_strength, edge_glow
    //   dc.signal    — bandwidth

    // Compute phosphor persistence as a per-frame blend factor.
    // Maps the slider (0–100 ms) to a decay factor per frame.
    // Uses τ=3 ms so the usable range is within the first ~20 ms:
    //    0 ms → 0.000  (no persistence)
    //    3 ms → 0.632  (subtle)
    //   10 ms → 0.964  (strong — 3.6% oscillation on sprite flicker)
    //   17 ms → 0.997  (one frame time — nearly invisible)
    //   25 ms → 0.9998 (effectively zero flicker)
    float persistence_ms = dc.phosphor.persistence;
    float decay_factor = 0.0f;
    if (persistence_ms > 0.01f) {
        decay_factor = 1.0f - std::exp(-persistence_ms / 3.0f);
    }

    // Ensure FBO size matches output
    resize(p, static_cast<int>(output_w), static_cast<int>(output_h));

    // Save GL state
    GLint prev_fbo = 0, prev_viewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glGetIntegerv(GL_VIEWPORT, prev_viewport);

    // Bind FBO and set viewport
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    glViewport(0, 0, p->width, p->height);

    // Clear to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind shader and set uniforms
    gl_api::glUseProgram(p->shader);

    // For 90°/270° rotations the vertex shader swaps UV axes, so the
    // fragment shader's scanline/convergence maths must see the post-
    // rotation dimensions (e.g. 224×256 instead of 256×224).
    float eff_input_w = input_w;
    float eff_input_h = input_h;
    if (rotation == 1 || rotation == 3)
        std::swap(eff_input_w, eff_input_h);

    gl_api::glUniform2f(p->loc_input_size, eff_input_w, eff_input_h);
    gl_api::glUniform2f(p->loc_output_size, output_w, output_h);
    gl_api::glUniform1f(p->loc_curvature, curvature);
    gl_api::glUniform1f(p->loc_scanline_gap, scanline_gap);
    gl_api::glUniform1f(p->loc_dot_pitch, dot_pitch);
    gl_api::glUniform1f(p->loc_brightness, brightness);
    gl_api::glUniform1f(p->loc_contrast, contrast);
    gl_api::glUniform1f(p->loc_gamma, gamma);
    gl_api::glUniform1i(p->loc_mask_type, mask_type);
    gl_api::glUniform3f(p->loc_phosphor_tint,
                        dc.phosphor.glow_color[0],
                        dc.phosphor.glow_color[1],
                        dc.phosphor.glow_color[2]);
    gl_api::glUniform1f(p->loc_mask_opacity, dc.mask.opacity);
    gl_api::glUniform1f(p->loc_scanline_strength, dc.scanlines.strength);
    gl_api::glUniform2f(p->loc_convergence_error,
                        dc.beam.convergence_error[0],
                        dc.beam.convergence_error[1]);
    gl_api::glUniform1f(p->loc_vignette_strength, dc.optics.vignette_strength);
    gl_api::glUniform3f(p->loc_glass_tint,
                        dc.optics.glass_tint[0],
                        dc.optics.glass_tint[1],
                        dc.optics.glass_tint[2]);
    gl_api::glUniform1f(p->loc_triad_size, dc.mask.triad_size);
    gl_api::glUniform1f(p->loc_slot_mask_width, dc.mask.slot_mask_width);
    gl_api::glUniform1f(p->loc_scanline_phase, dc.scanlines.phase);
    gl_api::glUniform1i(p->loc_interlace, dc.scanlines.interlace ? 1 : 0);
    gl_api::glUniform1f(p->loc_noise_level, dc.signal.noise_level);
    gl_api::glUniform1f(p->loc_hum_bar_strength, dc.signal.hum_bar_strength);
    gl_api::glUniform1f(p->loc_ghosting_strength, dc.signal.ghosting_strength);
    gl_api::glUniform1f(p->loc_chroma_phase_error, dc.signal.chroma_phase_error);
    gl_api::glUniform1f(p->loc_sync_stability, dc.signal.sync_stability);
    gl_api::glUniform1i(p->loc_frame_count, static_cast<GLint>(p->frame_count++));
    gl_api::glUniform1i(p->loc_rotation, rotation);
    gl_api::glUniform1f(p->loc_persistence, decay_factor);

    // Compute and set color temperature tint
    float ct_r, ct_g, ct_b;
    color_temp_tint(color_temp_k, ct_r, ct_g, ct_b);
    gl_api::glUniform3f(p->loc_color_temp_tint, ct_r, ct_g, ct_b);

    // Bind input texture to unit 0
    gl_api::glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_tex);

    // Bind previous frame texture to unit 1 (for phosphor persistence)
    gl_api::glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);

    // Draw fullscreen triangle
    gl_api::glBindVertexArray(p->dummy_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    gl_api::glBindVertexArray(0);

    // Copy current output to prev_frame_tex for next frame's persistence
    if (decay_factor > 0.001f) {
        gl_api::glBindFramebuffer(GL_READ_FRAMEBUFFER, p->fbo);
        glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, p->width, p->height);
    }

    // Restore state
    gl_api::glUseProgram(0);
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    gl_api::glActiveTexture(GL_TEXTURE0);
}

} // namespace crt_shader
