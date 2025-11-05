#ifndef CIMGUI_BACKENDS_H
#define CIMGUI_BACKENDS_H

#include <stdbool.h>

// Forward declarations
struct SDL_Window;
union SDL_Event;
struct ImDrawData;

#ifdef __cplusplus
extern "C" {
#endif

// SDL2 backend C wrappers
bool ImGui_ImplSDL2_InitForOpenGL_C(struct SDL_Window* window, void* sdl_gl_context);
void ImGui_ImplSDL2_Shutdown_C(void);
void ImGui_ImplSDL2_NewFrame_C(void);
bool ImGui_ImplSDL2_ProcessEvent_C(const union SDL_Event* event);

// OpenGL3 backend C wrappers
bool ImGui_ImplOpenGL3_Init_C(const char* glsl_version);
void ImGui_ImplOpenGL3_Shutdown_C(void);
void ImGui_ImplOpenGL3_NewFrame_C(void);
void ImGui_ImplOpenGL3_RenderDrawData_C(struct ImDrawData* draw_data);

#ifdef __cplusplus
}
#endif

#endif // CIMGUI_BACKENDS_H
