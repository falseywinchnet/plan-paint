#include "application.hpp"
#include "codecs.hpp"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL_main.h>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <numbers>

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_Window* window = SDL_CreateWindow("Untitled - Rainstar Paint", 1280, 850,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::cerr << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 1160, 480);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    const ImWchar interface_ranges[] = {0x20, 0x024F, 0x2000, 0x2199, 0x25B2, 0x25C0, 0};
    (*io.Fonts).AddFontFromMemoryTTF(const_cast<unsigned char*>(paint::embedded_font),
                                     paint::embedded_font_size, 18.0f, &font_config, interface_ranges);
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.ChildRounding = 0;
    style.FrameRounding = 0;
    style.PopupRounding = 0;
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;
    style.PopupBorderSize = 1;
    style.ScrollbarSize = 16;
    style.ScrollbarRounding = 0;
    style.GrabRounding = 0;
    style.Colors[ImGuiCol_Text] = ImVec4(0.10f, 0.13f, 0.17f, 1);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.96f, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(0.96f, 0.97f, 0.98f, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.87f, 0.93f, 0.99f, 1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.86f, 0.96f, 1);
    style.Colors[ImGuiCol_Header] = ImVec4(0.84f, 0.91f, 0.97f, 1);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.87f, 0.93f, 0.99f, 1);
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    int exit_code = 0;
    try {
        paint::Application app(window, renderer);
        bool trace = false;
        bool demo_reshape = false;
        for (int i = 1; i < argc; ++i) {
            std::string argument = argv[i];
            if (argument == "--trace") {
                trace = true;
            } else if (argument == "--screenshot" && i + 1 < argc) {
                app.screenshot_path = argv[++i];
                app.screenshot_frame = 8;
            } else if (argument == "--demo-reshape") {
                demo_reshape = true;
            } else if (argument == "--view-tab") {
                app.view_tab = true;
            } else if (argument == "--help-sidebar") {
                app.show_help = true;
            } else if (argument == "--demo") {
                app.document.new_image(960, 640);
                paint::Ink ink;
                ink.primary = {79, 129, 189, 255};
                ink.secondary = {219, 229, 241, 255};
                ink.size = 3;
                paint::draw_shape(app.document.image, paint::Shape::RoundedRectangle, {75, 85}, {390, 325},
                                  ink, true, true);
                ink.primary = {155, 187, 89, 255};
                ink.secondary = {235, 241, 222, 255};
                paint::draw_shape(app.document.image, paint::Shape::Oval, {445, 120}, {745, 420}, ink, true,
                                  true);
                ink.primary = {192, 80, 77, 255};
                ink.secondary = {242, 220, 219, 255};
                paint::draw_shape(app.document.image, paint::Shape::Star5, {235, 310}, {535, 570}, ink, true,
                                  true);
                paint::TextStyle text_style;
                text_style.size = 30;
                paint::draw_text(app.document.image, {80, 25}, "A little room for a big imagination.",
                                 text_style, {31, 73, 125, 255}, {}, "");
                app.texture_dirty = true;
            } else if (!argument.starts_with("--")) {
                app.document.replace(paint::load_image(argument), argument);
            }
        }
        if (demo_reshape) {
            std::vector<paint::Point> outline;
            for (int i = 0; i < 24; ++i) {
                double angle = i * 2.0 * std::numbers::pi / 24;
                outline.push_back({595.0 + 154 * std::cos(angle), 270.0 + 154 * std::sin(angle)});
            }
            app.document.select({440, 115, 310, 310}, outline);
            app.start_reshape();
            app.document.tool = paint::Tool::Reshape;
            std::size_t rightmost = 0;
            for (std::size_t i = 1; i < app.reshape_mesh.nodes.size(); ++i) {
                if (app.reshape_mesh.nodes[i].target.x > app.reshape_mesh.nodes[rightmost].target.x) {
                    rightmost = i;
                }
            }
            paint::Point target = app.reshape_mesh.nodes[rightmost].target;
            if (paint::move_reshape_node(app.reshape_mesh, rightmost, {target.x + 85, target.y - 25})) {
                ++app.mesh_generation;
                app.reshape_render_pending = true;
            }
        }
        while (app.running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                if (trace &&
                    (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                     event.type == SDL_EVENT_MOUSE_MOTION)) {
                    std::cerr << "input " << event.type << " " << event.button.x << "," << event.button.y
                              << "\n";
                }
                ImGui_ImplSDL3_ProcessEvent(&event);
                app.event(event);
            }
            app.prepare_text_font();
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            app.frame();
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            ++app.rendered_frames;
            if (!app.screenshot_path.empty() && app.rendered_frames >= app.screenshot_frame &&
                (!demo_reshape || (!app.warp_worker.busy() && !app.reshape_render_pending))) {
                SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
                if (surface) {
                    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
                    if (rgba) {
                        paint::Image image;
                        image.reset((*rgba).w, (*rgba).h);
                        const std::uint8_t* pixels = static_cast<const std::uint8_t*>((*rgba).pixels);
                        for (int y = 0; y < image.height; ++y) {
                            std::memcpy(image.pixels.data() + static_cast<std::size_t>(y) * image.width,
                                        pixels + y * (*rgba).pitch, image.width * 4);
                        }
                        paint::save_image(image, app.screenshot_path);
                        SDL_DestroySurface(rgba);
                    }
                    SDL_DestroySurface(surface);
                }
                app.running = false;
            }
            SDL_RenderPresent(renderer);
            SDL_Delay(8);
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        exit_code = 1;
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
