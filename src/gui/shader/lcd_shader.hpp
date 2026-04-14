#pragma once

// ============================================================================
// LCD Post-Processing Shader
// ============================================================================
//
// Single-pass LCD simulation applied as a fullscreen post-processing pass.
// Reads the emulated display from a texture and applies:
//   - Pixel grid lines (inter-pixel gaps visible on low-density LCDs)
//   - Subpixel structure (RGB stripe / monochrome)
//   - Backlight bleed and uniformity simulation
//   - Color tinting (e.g. Game Boy DMG green, Game Gear blue-shifted)
//   - Response time / ghosting (temporal blending with previous frame)
//   - Viewing angle contrast falloff (TN panel edge darkening)
//   - Black level elevation (backlit LCDs cannot display true black)
//   - Ambient reflection overlay (reflective / transflective panels)
//
// Uses an FBO to capture the intermediate display output, then renders
// a fullscreen triangle with the LCD shader applied.
//
// Requires: OpenGL 3.0 / GLSL 130
// ============================================================================

#include "core/cermu.hpp"
#include "devices/display/display_device.hpp"
#include "gui/gl_api.hpp"
#include <algorithm>
#include <cstdio>

namespace lcd_shader {

// ============================================================================
// GLSL sources
// ============================================================================

// Vertex shader — fullscreen triangle via gl_VertexID (no VBO needed).
static constexpr const char* vertex_src = R"glsl(
#version 130
out vec2 v_uv;
void main() {
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    v_uv = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)glsl";

// Fragment shader — single-pass LCD simulation.
static constexpr const char* fragment_src = R"glsl(
#version 130
in vec2 v_uv;

// --- Textures ---
uniform sampler2D InputTexture;     // Current frame
uniform sampler2D PrevFrameTexture; // Previous frame (for response time ghosting)

// --- Dimensions ---
uniform vec2  InputSize;            // Emulated framebuffer size in pixels
uniform vec2  OutputSize;           // Display output size in pixels

// --- Pixel structure ---
uniform float PixelGridOpacity;     // Grid line visibility (0=none, 1=full)
uniform float PixelGridWidth;       // Grid line width as fraction of pixel pitch
uniform float SubpixelOpacity;      // Subpixel RGB structure visibility (0=none, 1=full)
uniform int   SubpixelLayout;       // 0=RGB stripe, 1=BGR stripe, 2=monochrome

// --- Response time / ghosting ---
uniform float GhostingStrength;     // Residual previous frame blend (0=crisp, 1=heavy)

// --- Backlight ---
uniform float BacklightBrightness;  // Backlight intensity multiplier
uniform float BacklightBleed;       // Edge light leakage (0=none, 1=heavy)
uniform float BacklightUniformity;  // Evenness (1=perfect, lower=uneven)
uniform float BlackLevel;           // Minimum displayable brightness

// --- Color ---
uniform vec3  ColorTint;            // Panel color bias (default 1,1,1)
uniform float ColorSaturation;      // Saturation multiplier
uniform float Brightness;           // Overall brightness
uniform float Contrast;             // Overall contrast
uniform float Gamma;                // Display gamma (typically 2.2)
uniform vec3  ColorTempTint;        // Color temperature tint (normalized to 6500K)

// --- Optical ---
uniform float ViewingAngleFalloff;  // TN panel edge contrast/color loss
uniform float ReflectionStrength;   // Ambient light reflection on panel surface

// --- Time ---
uniform int   FrameCount;          // Monotonic frame counter

out vec4 Out_Color;

// ── Pixel grid ──────────────────────────────────────────────────────
// Simulates the dark gaps between LCD pixels visible on low-density
// panels (Game Boy, Game Gear, GBA).  At higher zoom levels the grid
// becomes more prominent, matching how real LCD pixels look under
// magnification or on large-pixel handheld displays.
float pixel_grid(vec2 uv, vec2 input_size, float grid_opacity, float grid_width) {
    if (grid_opacity < 0.001) return 1.0;

    vec2 pixel_uv = uv * input_size;
    vec2 frac_uv  = fract(pixel_uv);

    // Grid darkening at pixel boundaries (both horizontal and vertical)
    float half_grid = grid_width * 0.5;
    float gx = smoothstep(0.0, half_grid, frac_uv.x) *
               smoothstep(0.0, half_grid, 1.0 - frac_uv.x);
    float gy = smoothstep(0.0, half_grid, frac_uv.y) *
               smoothstep(0.0, half_grid, 1.0 - frac_uv.y);

    float grid = gx * gy;
    return mix(1.0, grid, grid_opacity);
}

// ── Subpixel structure ──────────────────────────────────────────────
// Models the RGB subpixel layout visible at close range or on large
// pixel-pitch panels.  Each pixel is divided into three vertical
// subpixel columns (R, G, B or B, G, R).  Monochrome panels have
// uniform illumination across the pixel.
vec3 subpixel_mask(vec2 frag_coord, vec2 output_size, vec2 input_size,
                   int layout, float opacity) {
    if (opacity < 0.001) return vec3(1.0);

    // Scale to match emulated pixels on screen
    float pixels_per_output = output_size.x / input_size.x;
    float subpixel_x = frag_coord.x / max(pixels_per_output, 1.0);
    int phase = int(floor(fract(subpixel_x) * 3.0));

    vec3 mask;
    if (layout == 2) {
        // Monochrome — no subpixel coloring
        mask = vec3(1.0);
    } else {
        // RGB or BGR stripe
        float bright = 1.0;
        float dim    = 1.0 - opacity * 0.7;

        if (layout == 1) phase = 2 - phase;  // BGR swap

        if (phase == 0)      mask = vec3(bright, dim, dim);
        else if (phase == 1) mask = vec3(dim, bright, dim);
        else                 mask = vec3(dim, dim, bright);
    }
    return mask;
}

// ── Backlight non-uniformity ────────────────────────────────────────
// Models uneven backlight illumination across the panel.  Real backlit
// LCDs (especially edge-lit) are brighter near the light guide edges
// and dimmer at center, or show corner/edge hotspots.
float backlight_pattern(vec2 uv, float uniformity, float bleed) {
    float base = 1.0;

    // Non-uniform backlight: brighter at edges, dimmer at center
    if (uniformity < 0.999) {
        vec2 center_dist = uv - 0.5;
        float edge_factor = dot(center_dist, center_dist) * 4.0;
        // Invert: edges slightly brighter than center (edge-lit characteristic)
        float nonuniform = 1.0 + (1.0 - uniformity) * edge_factor * 0.3;
        base *= nonuniform;
    }

    // Backlight bleed: bright glow at edges (light escaping the bezel)
    if (bleed > 0.001) {
        float edge_x = min(uv.x, 1.0 - uv.x);
        float edge_y = min(uv.y, 1.0 - uv.y);
        float edge_min = min(edge_x, edge_y);
        float bleed_factor = smoothstep(0.0, 0.15, edge_min);
        base += bleed * (1.0 - bleed_factor) * 0.3;
    }

    return base;
}

// ── Color saturation adjustment ─────────────────────────────────────
vec3 adjust_saturation(vec3 color, float saturation) {
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(luma), color, saturation);
}

void main() {
    vec2 uv = v_uv;

    // Sample current frame
    vec3 color = texture(InputTexture, uv).rgb;

    // Response time ghosting — blend with previous frame
    if (GhostingStrength > 0.001) {
        vec3 prev = texture(PrevFrameTexture, uv).rgb;
        color = mix(color, prev, GhostingStrength);
    }

    // Black level — raise minimum brightness (backlit LCDs can't do true black)
    color = mix(vec3(BlackLevel), vec3(1.0), color);

    // Backlight modulation
    float bl = backlight_pattern(uv, BacklightUniformity, BacklightBleed);
    color *= bl * BacklightBrightness;

    // Color saturation (STN panels have reduced saturation)
    color = adjust_saturation(color, ColorSaturation);

    // Panel color tint (e.g. DMG green, Game Gear blue shift)
    color *= ColorTint;

    // Brightness and contrast
    color = (color - 0.5) * Contrast + 0.5;
    color *= Brightness;

    // Gamma correction
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / Gamma));

    // Color temperature tint
    color *= ColorTempTint;

    // Pixel grid — darken gaps between emulated pixels
    float grid = pixel_grid(uv, InputSize, PixelGridOpacity, PixelGridWidth);
    color *= grid;

    // Subpixel structure — RGB stripe pattern within each pixel
    vec3 sp_mask = subpixel_mask(gl_FragCoord.xy, OutputSize, InputSize,
                                  SubpixelLayout, SubpixelOpacity);
    color *= sp_mask;

    // Viewing angle falloff — TN panels lose contrast/color at edges
    if (ViewingAngleFalloff > 0.001) {
        vec2 vc = uv - 0.5;
        float edge_dist = dot(vc, vc) * 4.0;
        // Desaturate and darken at edges
        float falloff = 1.0 - ViewingAngleFalloff * edge_dist * 0.5;
        color *= max(falloff, 0.5);
        // Slight color shift toward blue at extreme angles (TN characteristic)
        float shift = ViewingAngleFalloff * edge_dist * 0.1;
        color.r -= shift * 0.3;
        color.b += shift * 0.15;
    }

    // Ambient reflection — overlay for reflective/transflective panels
    if (ReflectionStrength > 0.001) {
        // Simulate ambient light wash across panel surface
        // Slightly non-uniform to model curved reflections
        float refl = ReflectionStrength * (0.8 + 0.4 * (1.0 - abs(uv.y - 0.5)));
        color += vec3(refl * 0.08);
    }

    Out_Color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)glsl";

// ============================================================================
// LCD Post-Process state
// ============================================================================

struct LCDPostProcess {
    GLuint fbo         = 0;     ///< Framebuffer object
    GLuint texture     = 0;     ///< Color attachment (RGBA8)
    GLuint shader      = 0;     ///< LCD shader program
    GLuint dummy_vao   = 0;     ///< Empty VAO for gl_VertexID rendering
    GLuint prev_frame_tex = 0;  ///< Previous frame texture (for ghosting)
    int    width       = 0;
    int    height      = 0;

    // Uniform locations
    GLint loc_input_texture      = -1;
    GLint loc_prev_frame_texture = -1;
    GLint loc_input_size         = -1;
    GLint loc_output_size        = -1;
    GLint loc_pixel_grid_opacity = -1;
    GLint loc_pixel_grid_width   = -1;
    GLint loc_subpixel_opacity   = -1;
    GLint loc_subpixel_layout    = -1;
    GLint loc_ghosting_strength  = -1;
    GLint loc_backlight_brightness = -1;
    GLint loc_backlight_bleed    = -1;
    GLint loc_backlight_uniformity = -1;
    GLint loc_black_level        = -1;
    GLint loc_color_tint         = -1;
    GLint loc_color_saturation   = -1;
    GLint loc_brightness         = -1;
    GLint loc_contrast           = -1;
    GLint loc_gamma              = -1;
    GLint loc_color_temp_tint    = -1;
    GLint loc_viewing_angle_falloff = -1;
    GLint loc_reflection_strength = -1;
    GLint loc_frame_count        = -1;

    uint32_t frame_count = 0;  ///< Monotonic frame counter
};

// ============================================================================
// Lifecycle
// ============================================================================

/// Create the LCD post-processing resources (FBO, shader, VAO, prev frame tex).
/// Returns true on success.
inline bool create(LCDPostProcess* p, int w, int h) {
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

    // --- Previous frame texture (for response time ghosting) ---
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
        log_error("lcd_shader: FBO incomplete (0x%x)\n", status);
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
        log_error("lcd_shader: link error: %s\n", log);
        gl_api::glDeleteProgram(p->shader);
        p->shader = 0;
        return false;
    }

    // --- Uniform locations ---
    p->loc_input_texture        = gl_api::glGetUniformLocation(p->shader, "InputTexture");
    p->loc_prev_frame_texture   = gl_api::glGetUniformLocation(p->shader, "PrevFrameTexture");
    p->loc_input_size           = gl_api::glGetUniformLocation(p->shader, "InputSize");
    p->loc_output_size          = gl_api::glGetUniformLocation(p->shader, "OutputSize");
    p->loc_pixel_grid_opacity   = gl_api::glGetUniformLocation(p->shader, "PixelGridOpacity");
    p->loc_pixel_grid_width     = gl_api::glGetUniformLocation(p->shader, "PixelGridWidth");
    p->loc_subpixel_opacity     = gl_api::glGetUniformLocation(p->shader, "SubpixelOpacity");
    p->loc_subpixel_layout      = gl_api::glGetUniformLocation(p->shader, "SubpixelLayout");
    p->loc_ghosting_strength    = gl_api::glGetUniformLocation(p->shader, "GhostingStrength");
    p->loc_backlight_brightness = gl_api::glGetUniformLocation(p->shader, "BacklightBrightness");
    p->loc_backlight_bleed      = gl_api::glGetUniformLocation(p->shader, "BacklightBleed");
    p->loc_backlight_uniformity = gl_api::glGetUniformLocation(p->shader, "BacklightUniformity");
    p->loc_black_level          = gl_api::glGetUniformLocation(p->shader, "BlackLevel");
    p->loc_color_tint           = gl_api::glGetUniformLocation(p->shader, "ColorTint");
    p->loc_color_saturation     = gl_api::glGetUniformLocation(p->shader, "ColorSaturation");
    p->loc_brightness           = gl_api::glGetUniformLocation(p->shader, "Brightness");
    p->loc_contrast             = gl_api::glGetUniformLocation(p->shader, "Contrast");
    p->loc_gamma                = gl_api::glGetUniformLocation(p->shader, "Gamma");
    p->loc_color_temp_tint      = gl_api::glGetUniformLocation(p->shader, "ColorTempTint");
    p->loc_viewing_angle_falloff = gl_api::glGetUniformLocation(p->shader, "ViewingAngleFalloff");
    p->loc_reflection_strength  = gl_api::glGetUniformLocation(p->shader, "ReflectionStrength");
    p->loc_frame_count          = gl_api::glGetUniformLocation(p->shader, "FrameCount");

    // Set texture units
    gl_api::glUseProgram(p->shader);
    gl_api::glUniform1i(p->loc_input_texture, 0);
    gl_api::glUniform1i(p->loc_prev_frame_texture, 1);
    gl_api::glUseProgram(0);

    // --- Empty VAO for gl_VertexID rendering ---
    gl_api::glGenVertexArrays(1, &p->dummy_vao);

    return true;
}

/// Resize the LCD FBO and prev-frame texture if dimensions changed.
inline void resize(LCDPostProcess* p, int w, int h) {
    if (p->width == w && p->height == h) return;
    p->width  = w;
    p->height = h;
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/// Destroy LCD post-processing resources.
inline void destroy(LCDPostProcess* p) {
    if (p->fbo)            { gl_api::glDeleteFramebuffers(1, &p->fbo); p->fbo = 0; }
    if (p->texture)        { glDeleteTextures(1, &p->texture); p->texture = 0; }
    if (p->prev_frame_tex) { glDeleteTextures(1, &p->prev_frame_tex); p->prev_frame_tex = 0; }
    if (p->shader)         { gl_api::glDeleteProgram(p->shader); p->shader = 0; }
    if (p->dummy_vao)      { gl_api::glDeleteVertexArrays(1, &p->dummy_vao); p->dummy_vao = 0; }
}

// ============================================================================
// Render pass
// ============================================================================

/// Render the LCD post-processing pass.
/// Reads from `input_tex`, writes to `p->texture` via the FBO.
/// Copies the result to prev_frame_tex for next frame's ghosting.
///
/// @param p            LCD post-process state
/// @param input_tex    Source texture (the emulated display output)
/// @param input_w      Source texture width (emulated framebuffer)
/// @param input_h      Source texture height (emulated framebuffer)
/// @param output_w     Display output width (screen pixels)
/// @param output_h     Display output height (screen pixels)
/// @param dc           Display characteristics (lcd sub-struct for LCD params)
inline void render(LCDPostProcess* p,
                   GLuint input_tex,
                   float input_w, float input_h,
                   float output_w, float output_h,
                   const DisplayCharacteristics& dc) {
    if (!p->shader || !p->fbo) return;

    const auto& lcd = dc.lcd;

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

    gl_api::glUniform2f(p->loc_input_size, input_w, input_h);
    gl_api::glUniform2f(p->loc_output_size, output_w, output_h);

    // Pixel structure
    gl_api::glUniform1f(p->loc_pixel_grid_opacity, lcd.pixel_grid_opacity);
    gl_api::glUniform1f(p->loc_pixel_grid_width, lcd.pixel_grid_width);
    gl_api::glUniform1f(p->loc_subpixel_opacity, lcd.subpixel_opacity);
    gl_api::glUniform1i(p->loc_subpixel_layout,
                        static_cast<int>(lcd.subpixel_layout));

    // Response time ghosting
    gl_api::glUniform1f(p->loc_ghosting_strength, lcd.ghosting_strength);

    // Backlight
    gl_api::glUniform1f(p->loc_backlight_brightness, lcd.backlight_brightness);
    gl_api::glUniform1f(p->loc_backlight_bleed, lcd.backlight_bleed);
    gl_api::glUniform1f(p->loc_backlight_uniformity, lcd.backlight_uniformity);
    gl_api::glUniform1f(p->loc_black_level, lcd.black_level);

    // Color
    gl_api::glUniform3f(p->loc_color_tint,
                        lcd.color_tint[0], lcd.color_tint[1], lcd.color_tint[2]);
    gl_api::glUniform1f(p->loc_color_saturation, lcd.color_saturation);
    gl_api::glUniform1f(p->loc_brightness, dc.brightness);
    gl_api::glUniform1f(p->loc_contrast, dc.contrast);
    gl_api::glUniform1f(p->loc_gamma, dc.gamma);

    // Optics
    gl_api::glUniform1f(p->loc_viewing_angle_falloff, lcd.viewing_angle_falloff);
    gl_api::glUniform1f(p->loc_reflection_strength, lcd.reflection_strength);

    // Frame count
    gl_api::glUniform1i(p->loc_frame_count, static_cast<GLint>(p->frame_count++));

    // Color temperature tint (reuse CRT shader utility via inline computation)
    {
        float t = dc.color_temp / 100.0f;
        float r, g, b;
        // Red
        r = (t <= 66.0f) ? 1.0f : 1.29293618606f * powf(t - 60.0f, -0.1332047592f);
        // Green
        g = (t <= 66.0f) ? 0.39008157876f * logf(t) - 0.63184144378f
                         : 1.12989086090f * powf(t - 60.0f, -0.0755148492f);
        // Blue
        b = (t >= 66.0f) ? 1.0f : ((t <= 19.0f) ? 0.0f
            : 0.54320678911f * logf(t - 10.0f) - 1.19625408914f);
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        // Normalize to 6500K reference
        float rr, rg, rb;
        float ref_t = 6500.0f / 100.0f;
        rr = 1.0f; // t=65 <= 66
        rg = 0.39008157876f * logf(ref_t) - 0.63184144378f;
        rb = 0.54320678911f * logf(ref_t - 10.0f) - 1.19625408914f;
        rr = std::clamp(rr, 0.0f, 1.0f);
        rg = std::clamp(rg, 0.0f, 1.0f);
        rb = std::clamp(rb, 0.0f, 1.0f);
        float tr = (rr > 0.001f) ? r / rr : 1.0f;
        float tg = (rg > 0.001f) ? g / rg : 1.0f;
        float tb = (rb > 0.001f) ? b / rb : 1.0f;
        gl_api::glUniform3f(p->loc_color_temp_tint, tr, tg, tb);
    }

    // Bind input texture to unit 0
    gl_api::glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_tex);

    // Bind previous frame texture to unit 1
    gl_api::glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);

    // Draw fullscreen triangle
    gl_api::glBindVertexArray(p->dummy_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    gl_api::glBindVertexArray(0);

    // Copy current output to prev_frame_tex for next frame's ghosting
    if (lcd.ghosting_strength > 0.001f) {
        gl_api::glBindFramebuffer(GL_READ_FRAMEBUFFER, p->fbo);
        glBindTexture(GL_TEXTURE_2D, p->prev_frame_tex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, p->width, p->height);
    }

    // Restore previous GL state
    gl_api::glUseProgram(0);
    gl_api::glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    glViewport(prev_viewport[0], prev_viewport[1],
               prev_viewport[2], prev_viewport[3]);
    gl_api::glActiveTexture(GL_TEXTURE0);
}

} // namespace lcd_shader
