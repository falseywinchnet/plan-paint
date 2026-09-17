#include "application.hpp"
#include "codecs.hpp"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"
#include "paths.hpp"
#include "renderer.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
class UiFixture {
  public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<paint::Application> app;
    explicit UiFixture(bool native = false) {
        if (!native) {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        }
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }
        window = SDL_CreateWindow("Rainstar isolated interaction tests", 1280, 850, SDL_WINDOW_HIDDEN);
        if (!window) {
            throw std::runtime_error(SDL_GetError());
        }
        renderer = SDL_CreateRenderer(window, native ? nullptr : "software");
        if (!renderer) {
            throw std::runtime_error(SDL_GetError());
        }
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = {1280, 850};
        io.DeltaTime = 1.0f / 60;
        io.IniFilename = nullptr;
        static const ImWchar ranges[] = {0x20, 0x024f, 0x2000, 0x2199, 0x25b2, 0x25c0, 0};
        ImFontConfig font_config;
        font_config.FontDataOwnedByAtlas = false;
        ImFont* font =
            (*io.Fonts).AddFontFromMemoryTTF(const_cast<unsigned char*>(paint::embedded_font),
                                             paint::embedded_font_size, 18.0f, &font_config, ranges);
        unsigned char* atlas = nullptr;
        int width = 0, height = 0;
        (*io.Fonts).GetTexDataAsRGBA32(&atlas, &width, &height);
        require((*font).FindGlyphNoFallback(0x25bc) && (*font).FindGlyphNoFallback(0x25b2) &&
                    (*font).FindGlyphNoFallback(0x25b6) && (*font).FindGlyphNoFallback(0x25c0),
                "Portsmouth navigation glyphs are missing from embedded UI font");
        ImGui_ImplSDLRenderer3_Init(renderer);
        app = std::make_unique<paint::Application>(window, renderer, false);
        frame();
        frame();
    }
    ~UiFixture() {
        app.reset();
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    void frame() {
        (*app).prepare_text_font();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();
        (*app).frame();
        ImGui::Render();
    }
    void move(float x, float y) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        frame();
    }
    void click(float x, float y, int button = 0) {
        move(x, y);
        ImGui::GetIO().AddMouseButtonEvent(button, true);
        frame();
        ImGui::GetIO().AddMouseButtonEvent(button, false);
        frame();
    }
    void drag(float x, float y, float end_x, float end_y) {
        move(x, y);
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        frame();
        for (int i = 1; i <= 10; ++i) {
            move(x + (end_x - x) * i / 10, y + (end_y - y) * i / 10);
        }
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        frame();
    }
    void key(ImGuiKey key) {
        ImGui::GetIO().AddKeyEvent(key, true);
        frame();
        ImGui::GetIO().AddKeyEvent(key, false);
        frame();
    }
    void wait_work() {
        std::chrono::steady_clock::time_point limit =
            std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while ((*app).warp_worker.busy() || (*app).stamp_render_pending || (*app).reshape_render_pending ||
               (*app).rotation_render_pending || (*app).rotation_commit_pending) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            require(std::chrono::steady_clock::now() < limit, "asynchronous transform timed out");
        }
    }
};
paint::Image capture_framebuffer(UiFixture& ui, int width, int height, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {static_cast<float>(width), static_cast<float>(height)};
    io.DisplayFramebufferScale = {scale, scale};
    ui.frame();
    ui.frame();
    int pixel_width = static_cast<int>(width * scale);
    int pixel_height = static_cast<int>(height * scale);
    SDL_Texture* target = SDL_CreateTexture(ui.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                            pixel_width, pixel_height);
    require(target != nullptr, SDL_GetError());
    require(SDL_SetRenderTarget(ui.renderer, target), SDL_GetError());
    paint::render_interface(ui.renderer, ImGui::GetDrawData());
    SDL_Surface* surface = SDL_RenderReadPixels(ui.renderer, nullptr);
    require(surface != nullptr, SDL_GetError());
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    require(rgba != nullptr, SDL_GetError());
    paint::Image capture;
    capture.reset((*rgba).w, (*rgba).h);
    const std::uint8_t* pixels = static_cast<const std::uint8_t*>((*rgba).pixels);
    for (int y = 0; y < capture.height; ++y) {
        std::memcpy(capture.pixels.data() + static_cast<std::size_t>(y) * capture.width,
                    pixels + y * (*rgba).pitch, capture.width * 4);
    }
    SDL_DestroySurface(rgba);
    require(SDL_SetRenderTarget(ui.renderer, nullptr), SDL_GetError());
    SDL_DestroyTexture(target);
    return capture;
}
void retina_rendering(UiFixture& ui, const std::string& screenshot = "") {
    paint::Application& app = *ui.app;
    app.document.new_image(960, 1600);
    app.texture_dirty = true;
    app.show_help = false;
    app.show_status = true;
    app.zoom = 1;
    ui.move(-100, -100);
    const int sizes[3][2] = {{1280, 850}, {1160, 600}, {1440, 900}};
    const float densities[] = {2.0f, 1.5f, 1.0f};
    for (int size = 0; size < 3; ++size) {
        int width = sizes[size][0], height = sizes[size][1];
        paint::Image reference = capture_framebuffer(ui, width, height, 1);
        const int points[5][2] = {
            {width - 30, 300}, {80, 400}, {80, height - 16}, {width - 30, 140}, {500, 100}};
        require(!paint::equal(reference.get(width - 30, 300), {240, 240, 240, 255}),
                "reference frame does not fill the window");
        require(reference.get(80, 400).r == 255, "reference canvas is not visible");
        require(reference.get(80, height - 16).r != 255,
                "reference canvas is not clipped above the status bar");
        for (float density : densities) {
            paint::Image capture = capture_framebuffer(ui, width, height, density);
            if (size == 0 && density == 2 && !screenshot.empty()) {
                paint::save_image(capture, screenshot);
            }
            require(capture.width == static_cast<int>(width * density) &&
                        capture.height == static_cast<int>(height * density),
                    "physical framebuffer dimensions do not match density");
            for (int point = 0; point < 5; ++point) {
                int x = points[point][0], y = points[point][1];
                int physical_x = static_cast<int>((x + 0.5f) * density);
                int physical_y = static_cast<int>((y + 0.5f) * density);
                require(paint::equal(reference.get(x, y), capture.get(physical_x, physical_y)),
                        "Retina pixels differ: window coverage, ribbon or canvas clipping is incorrect");
            }
        }
    }
    ImGui::GetIO().DisplaySize = {1280, 850};
    ImGui::GetIO().DisplayFramebufferScale = {1, 1};
}
void drawing_and_controls(UiFixture& ui) {
    paint::Application& app = *ui.app;
    ui.click(273, 75);
    ui.drag(107, 265, 207, 365);
    require(app.document.dirty(), "canvas pointer stroke did not start");
    require(app.document.image.get(100, 100).r == 0, "stroke start pixel missing");
    require(app.document.image.get(200, 200).r == 0, "stroke end pixel missing");
    ui.click(75, 13);
    require(app.document.image.get(100, 100).r == 255, "quick access Undo failed");
    ui.click(102, 13);
    require(app.document.image.get(100, 100).r == 0, "quick access Redo failed");
    ui.key(ImGuiKey_F1);
    require(app.show_help, "F1 did not open help");
    ui.key(ImGuiKey_F1);
    require(!app.show_help, "F1 did not close help");
    ui.click(146, 39);
    require(app.view_tab, "View tab failed");
    ui.click(86, 39);
    require(!app.view_tab, "Home tab failed");
    // The second palette row's first Office blue pastel is #C6D9F1.
    ui.click(921, 93);
    require(paint::equal(app.document.ink.primary, {198, 217, 241, 255}),
            "Office palette hit testing failed");
}
void path_and_selection(UiFixture& ui) {
    paint::Application& app = *ui.app;
    app.execute(paint::Command::New);
    app.choose_tool(paint::Tool::Path);
    app.document.continuous_path = true;
    ui.click(100, 240);
    ui.click(220, 240);
    ui.click(220, 350);
    ui.click(101, 241);
    require(app.document.path.size() == 4, "continuous path terminated at first junction");
    require(app.document.path[0].x == app.document.path[3].x &&
                app.document.path[0].y == app.document.path[3].y,
            "junction did not snap exactly");
    ui.click(340, 370);
    ui.key(ImGuiKey_Escape);
    require(app.document.path.empty(), "Escape did not commit continuous path");
    require(app.document.image.get(93, 75).r != 255, "path was not rasterized");
    app.choose_tool(paint::Tool::Select);
    ui.drag(90, 225, 230, 370);
    require(app.document.selection.active, "rectangle selection was not created");
    int previous_x = app.document.selection.x;
    ui.drag(150, 280, 270, 320);
    require(app.document.selection.x == previous_x + 120, "floating selection did not move");
    ui.key(ImGuiKey_Escape);
    require(!app.document.selection.active, "Escape did not place selection");
    app.execute(paint::Command::Undo);
    require(app.document.image.get(93, 75).r != 255, "selection undo did not restore source");
    app.execute(paint::Command::Redo);
    require(app.document.image.get(213, 115).r != 255, "selection redo lost floating pixels");
}
void stamp_and_reshape(UiFixture& ui) {
    paint::Application& app = *ui.app;
    app.execute(paint::Command::New);
    paint::Ink ink;
    ink.primary = {192, 80, 77, 255};
    ink.secondary = {242, 220, 219, 255};
    paint::draw_shape(app.document.image, paint::Shape::Oval, {50, 50}, {140, 140}, ink, true, true);
    app.texture_dirty = true;
    app.stamp_width = 90;
    app.stamp_height = 90;
    app.choose_tool(paint::Tool::Stamp);
    ui.click(102, 260);
    ui.wait_work();
    require(!app.stamp_preview.pixels.empty(), "stamp was not compiled");
    ui.click(307, 365);
    require(app.document.image.get(300, 200).r == 242, "stamp did not place a copy");
    ui.key(ImGuiKey_R);
    ui.key(ImGuiKey_Equal);
    ui.wait_work();
    require(app.stamp_preview.width > 90, "stamp rotation/scale keys failed");
    app.choose_tool(paint::Tool::Select);
    app.document.select({40, 40, 120, 120});
    app.texture_dirty = true;
    app.choose_tool(paint::Tool::Reshape);
    ui.wait_work();
    require(app.reshape_active && app.reshape_field && app.reshape_mesh.nodes.size() > 4,
            "reshape field/mesh missing");
    paint::Point first = app.reshape_mesh.nodes[0].target;
    bool moved = paint::move_reshape_node(app.reshape_mesh, 0, {first.x - 8, first.y - 5});
    require(moved, "reasonable reshape knob move rejected");
    ++app.mesh_generation;
    app.reshape_render_pending = true;
    ui.wait_work();
    ui.key(ImGuiKey_Escape);
    ui.wait_work();
    require(!app.reshape_active && !app.document.selection.active, "Escape did not commit reshape");
    require(app.error.empty(), app.error.c_str());
}
void text_and_stale_transform(UiFixture& ui) {
    paint::Application& app = *ui.app;
    app.execute(paint::Command::New);
    app.choose_tool(paint::Tool::Text);
    ui.click(120, 260);
    ui.frame();
    ImGui::GetIO().AddInputCharactersUTF8("Hello Portsmouth");
    ui.frame();
    require(std::string(app.text_buffer) == "Hello Portsmouth",
            "text did not receive initial keyboard focus");
    app.text_style.bold = true;
    app.text_style.size = 36;
    ui.frame();
    require(app.text_ui_font && (*app.text_ui_font).FontSize == 36, "live embedded text font did not update");
    app.finish_text();
    ui.frame();
    unsigned marks = 0;
    for (paint::Color pixel : app.document.image.pixels) {
        if (pixel.r < 250) {
            ++marks;
        }
    }
    require(marks > 80, "styled text was not rasterized");
    app.document.select({50, 50, 80, 80});
    app.start_reshape();
    app.command(paint::Command::Undo);
    ui.wait_work();
    require(!app.reshape_active && !app.reshape_field, "completed background compilation survived Undo");
    require(app.error.empty(), app.error.c_str());
}
void recent_files_and_desktop_layouts(UiFixture& ui) {
    paint::Application& app = *ui.app;
    app.zoom = 1;
    ui.click(1258, 837);
    require(app.zoom == 2, "status-bar zoom-in button failed");
    ui.click(1088, 837);
    require(app.zoom == 1, "status-bar zoom-out button failed");
    paint::Image material;
    material.reset(2, 2, {10, 30, 60, 255});
    paint::Image tiled = paint::wallpaper_image(material, 7, 5, paint::WallpaperLayout::Tile);
    require(tiled.width == 7 && tiled.height == 5 && paint::equal(tiled.pixels.back(), material.pixels[0]),
            "tiled wallpaper left an unpainted edge");
    paint::Image centered = paint::wallpaper_image(material, 6, 6, paint::WallpaperLayout::Center);
    require(centered.pixels[0].r == 255 && paint::equal(centered.pixels[14], material.pixels[0]),
            "centered wallpaper did not preserve native size and center");
    paint::Image filled = paint::wallpaper_image(material, 7, 5, paint::WallpaperLayout::Fill);
    require(paint::equal(filled.pixels.front(), material.pixels[0]) &&
                paint::equal(filled.pixels.back(), material.pixels[0]),
            "CONV wallpaper fill did not cover the desktop");
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("rainstar-recent-test-" + std::to_string(SDL_GetTicksNS()));
    std::filesystem::create_directory(directory);
    std::u8string encoded = (directory / "recent.bin").u8string();
    paint::RecentFiles recent;
    recent.storage_path = std::string(encoded.begin(), encoded.end());
    recent.remember("a\\b\nimage.png");
    recent.remember("日本語 image.png");
    recent.remember("a\\b\nimage.png");
    paint::RecentFiles reloaded;
    reloaded.storage_path = recent.storage_path;
    reloaded.load();
    require(reloaded.paths == recent.paths && reloaded.paths.size() == 2,
            "recent files lost Unicode/newlines or retained duplicates");
    for (int index = 0; index < 20; ++index) {
        recent.remember(std::to_string(index));
    }
    require(recent.paths.size() == 12 && recent.paths[0] == "19", "recent files did not bound history");
    app.execute(paint::Command::New);
    encoded = (directory / paint::path_from_utf8("日本語.png")).u8string();
    const std::string picture_path(encoded.begin(), encoded.end());
    app.save_to(picture_path);
    require(app.recent_files.paths.front() == picture_path && app.status.find("日本語") != std::string::npos,
            "saving did not publish the Unicode recent-file entry");
    ui.frame();
    app.document.checkpoint();
    app.document.image.pixels[0] = {20, 40, 60, 255};
    app.recent_to_open = picture_path;
    app.command(paint::Command::OpenRecent);
    require(app.unsaved_dialog && app.deferred_command == paint::Command::OpenRecent,
            "opening recent picture bypassed the unsaved-work prompt");
    app.unsaved_dialog = false;
    app.execute(paint::Command::OpenRecent);
    require(!app.document.dirty() && app.document.filename == picture_path,
            "recent-picture command did not load the selected document");
    std::filesystem::remove_all(directory);
}
void custom_colors_and_cursor(UiFixture& ui) {
    paint::Application& app = *ui.app;
    app.execute(paint::Command::New);
    app.choose_tool(paint::Tool::Pencil);
    ui.move(400, 350);
    require(ImGui::GetMouseCursor() == ImGuiMouseCursor_None,
            "painting cursor did not use its canvas preview");
    ui.move(90, 40);
    for (int frame = 0; frame < 20; ++frame) {
        ui.frame();
        require(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow, "menu cursor request was unstable");
    }
    app.begin_color();
    ui.frame();
    ui.frame();
    app.edit_color({18, 52, 86, 255});
    ImGuiWindow* dialog = ImGui::FindWindowByName("Edit Colors");
    require(dialog != nullptr, "Edit Colors dialog did not open");
    const ImVec2 position = (*dialog).Pos;
    ui.click(position.x + 136, position.y + 289);
    require((app.custom_colors.occupied & 1) && paint::equal(app.custom_colors.colors[0], {18, 52, 86, 255}),
            "Add to custom colors did not retain the edited color");
    app.edit_color({200, 20, 50, 255});
    ui.click(position.x + 26, position.y + 221);
    require(std::string(app.edited_hex) == "#123456", "saved custom swatch did not restore its color");
    ui.click(position.x + 64, position.y + (*dialog).Size.y - 26);
    require(paint::equal(app.document.ink.primary, {18, 52, 86, 255}),
            "Edit Colors OK did not apply the saved color");
    app.begin_color();
    ui.frame();
    app.edit_color({240, 20, 40, 255});
    ui.key(ImGuiKey_Escape);
    require(paint::equal(app.document.ink.primary, {18, 52, 86, 255}), "Cancel changed the drawing color");
    std::filesystem::path file = std::filesystem::temp_directory_path() /
                                 ("rainstar-colors-" + std::to_string(SDL_GetTicksNS()) + ".bin");
    const std::u8string encoded = file.u8string();
    paint::CustomColors saved;
    saved.storage_path = std::string(encoded.begin(), encoded.end());
    saved.store(0, {18, 52, 86, 255});
    saved.store(15, {207, 121, 52, 255});
    paint::CustomColors loaded;
    loaded.storage_path = saved.storage_path;
    loaded.load();
    require(loaded.occupied == 0x8001 && paint::equal(loaded.colors[0], saved.colors[0]) &&
                paint::equal(loaded.colors[15], saved.colors[15]),
            "custom colors did not survive reload");
    std::filesystem::remove(file);
    app.stamp_angle = 90;
    app.stamp_scale = 2;
    app.reset_stamp();
    require(app.document.stamp.pixels.empty() && app.stamp_preview.pixels.empty() && !app.stamp_field &&
                app.stamp_angle == 0 && app.stamp_scale == 1 && app.document.tool == paint::Tool::Stamp,
            "Lift a new stamp did not reset the material, rotation and scale");
}
void arbitrary_rotation(UiFixture& ui) {
    paint::Application& app = *ui.app;
    app.execute(paint::Command::New);
    app.choose_tool(paint::Tool::Select);
    for (int y = 40; y < 80; ++y) {
        for (int x = 40; x < 120; ++x) {
            app.document.image.set(x, y, {static_cast<std::uint8_t>(x), 100, 200, 255});
        }
    }
    app.document.select({40, 40, 80, 40});
    app.texture_dirty = true;
    ui.frame();
    ui.drag(145, 187, 155, 239);
    ui.wait_work();
    require(!app.rotation_active && app.document.selection.active &&
                app.document.selection.image.height > 60 && app.rotation_angle > 30 &&
                app.rotation_angle < 60,
            "selection corner handle did not perform an arbitrary CONV rotation");
    require(app.document.selection.image.pixels.front().a == 0, "rotation lost its transparent corners");
    ui.key(ImGuiKey_Escape);
    require(!app.document.selection.active, "Escape did not place the rotated selection");
    app.command(paint::Command::Undo);
    require(app.document.image.get(40, 40).r == 40, "Undo did not restore the pre-rotation object");
    app.document.new_image(64, 32);
    app.request_rotation(37);
    ui.wait_work();
    require(!app.rotation_active && !app.document.selection.active && app.document.image.width > 64 &&
                app.document.image.height > 32,
            "precise whole-picture rotation did not expand its canvas");
    app.document.new_image();
    app.document.select({40, 40, 80, 40});
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    ui.frame();
    ui.drag(145, 187, 150, 250);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
    ui.wait_work();
    require(std::abs(std::remainder(app.rotation_angle, 15.0)) < 1e-8,
            "Shift did not constrain the rotation handle to 15-degree steps");
    ui.key(ImGuiKey_Escape);
    app.document.select({8, 8, 24, 24});
    app.start_rotation();
    app.command(paint::Command::Undo);
    ui.wait_work();
    require(!app.rotation_active && !app.rotation_field && !app.document.selection.active,
            "obsolete rotation compilation survived Undo");
    require(app.error.empty(), app.error.c_str());
}
} // namespace
int main(int argc, char** argv) {
    try {
        bool native = argc >= 2 && std::string(argv[1]) == "--native-render-test";
        UiFixture ui(native);
        if (native) {
            std::string screenshot = argc >= 3 ? argv[2] : "";
            retina_rendering(ui, screenshot);
            std::cout << "Native framebuffer coverage and clipping passed at 1x, 1.5x and 2x with "
                      << SDL_GetRendererName(ui.renderer) << ".\n";
            return 0;
        }
        drawing_and_controls(ui);
        path_and_selection(ui);
        stamp_and_reshape(ui);
        text_and_stale_transform(ui);
        recent_files_and_desktop_layouts(ui);
        custom_colors_and_cursor(ui);
        arbitrary_rotation(ui);
        retina_rendering(ui);
        std::cout << "Ribbon/canvas, palettes, cursor requests, paths, selections, stamps, free rotation and "
                     "reshape passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
