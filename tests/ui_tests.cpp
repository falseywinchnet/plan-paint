#include "application.hpp"
#include <chrono>
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
    UiFixture() {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }
        window = SDL_CreateWindow("Rainstar isolated interaction tests", 1280, 850, SDL_WINDOW_HIDDEN);
        if (!window) {
            throw std::runtime_error(SDL_GetError());
        }
        renderer = SDL_CreateRenderer(window, "software");
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
        app = std::make_unique<paint::Application>(window, renderer);
        frame();
        frame();
    }
    ~UiFixture() {
        app.reset();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    void frame() {
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
        while ((*app).warp_worker.busy() || (*app).stamp_render_pending || (*app).reshape_render_pending) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            require(std::chrono::steady_clock::now() < limit, "asynchronous transform timed out");
        }
    }
};
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
} // namespace
int main() {
    try {
        UiFixture ui;
        drawing_and_controls(ui);
        path_and_selection(ui);
        stamp_and_reshape(ui);
        std::cout
            << "Isolated ribbon/canvas, paths, selections, F1, stamp and reshape interactions passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
