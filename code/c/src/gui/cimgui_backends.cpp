// Backend wrapper for cimgui with SDL2 and OpenGL3
// This file compiles the necessary ImGui backend implementations for use with cimgui

#include <SDL.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

// Include the actual implementation files
#include "backends/imgui_impl_sdl2.cpp"
#include "backends/imgui_impl_opengl3.cpp"

extern "C" {

// C wrappers for SDL2 backend
bool ImGui_ImplSDL2_InitForOpenGL_C(SDL_Window* window, void* sdl_gl_context) {
    return ImGui_ImplSDL2_InitForOpenGL(window, sdl_gl_context);
}

void ImGui_ImplSDL2_Shutdown_C(void) {
    ImGui_ImplSDL2_Shutdown();
}

void ImGui_ImplSDL2_NewFrame_C(void) {
    ImGui_ImplSDL2_NewFrame();
}

bool ImGui_ImplSDL2_ProcessEvent_C(const SDL_Event* event) {
    return ImGui_ImplSDL2_ProcessEvent(event);
}

// C wrappers for OpenGL3 backend
bool ImGui_ImplOpenGL3_Init_C(const char* glsl_version) {
    return ImGui_ImplOpenGL3_Init(glsl_version);
}

void ImGui_ImplOpenGL3_Shutdown_C(void) {
    ImGui_ImplOpenGL3_Shutdown();
}

void ImGui_ImplOpenGL3_NewFrame_C(void) {
    ImGui_ImplOpenGL3_NewFrame();
}

void ImGui_ImplOpenGL3_RenderDrawData_C(ImDrawData* draw_data) {
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
}

} // extern "C"
