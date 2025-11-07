#pragma once

// Dear ImGui Backend Headers for SDL2 + OpenGL3
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

// Backend initialization and cleanup functions
extern "C" {
    bool imgui_backends_init(void* window, const char* glsl_version);
    void imgui_backends_shutdown();
    void imgui_backends_new_frame();
}