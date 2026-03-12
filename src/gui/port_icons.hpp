#pragma once
/**
 * port_icons.hpp — Menu-bar port icon textures
 *
 * Provides small 16×16 RGBA icons for each PortType, loaded as
 * OpenGL textures.  Used by the right-aligned port icon bar in
 * the main menu.
 */

#include <SDL_opengl.h>
#include "core/port.hpp"

namespace PortIcons {

/// Create GL textures for all connector-type icons.  Call once after
/// the OpenGL context has been created.
void init();

/// Destroy all icon textures.  Call before GL context teardown.
void cleanup();

/// Get the GL texture ID for a given connector type.
/// Returns 0 if the type has no icon or init() hasn't been called.
GLuint get_icon(PortType type);

/// Icon display size (logical pixels) for the menu bar.
constexpr int ICON_SIZE = 16;

} // namespace PortIcons
