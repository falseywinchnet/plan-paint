#include "renderer.hpp"
#include "imgui_impl_sdlrenderer3.h"
#include <stdexcept>

namespace paint {
void render_interface(SDL_Renderer* renderer, ImDrawData* draw_data) {
    // SDL geometry stays in logical coordinates. Set the renderer scale each frame so
    // its vertices and the backend's clip rectangles agree, including after moving
    // the window between displays. The backend detects this scale and avoids doubling it.
    if (!SDL_SetRenderScale(renderer, (*draw_data).FramebufferScale.x, (*draw_data).FramebufferScale.y)) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(draw_data, renderer);
}
} // namespace paint
