#pragma once
#include "document.hpp"
#include "imgui.h"
#include "platform.hpp"
#include "text.hpp"
#include "warp_session.hpp"
namespace paint {
enum class Command {
    New,
    Open,
    Save,
    SaveAs,
    Print,
    PageSetup,
    Quit,
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    PasteFrom,
    SelectAll,
    InvertSelection,
    Crop,
    Resize,
    RotateRight,
    RotateLeft,
    Rotate180,
    FlipHorizontal,
    FlipVertical,
    Invert,
    Delete,
    Properties,
    About
};
struct Application {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* canvas_texture = nullptr;
    SDL_Texture* stamp_texture = nullptr;
    Document document;
    FileDialog dialog;
    bool running = true, show_help = false, view_tab = false, text_tab = false;
    bool show_grid = false, show_rulers = false, show_status = true, full_screen = false;
    bool texture_dirty = true, preview_active = false, dragging = false, moving_selection = false;
    bool color_dialog = false, resize_dialog = false, properties_dialog = false, about_dialog = false;
    bool text_active = false, unsaved_dialog = false;
    Command deferred_command = Command::New;
    bool deferred_after_save = false;
    bool primary_slot = true;
    bool right_gesture = false;
    Image preview, gesture_base, stamp_preview;
    Point down, last, hover, selection_start;
    std::vector<Point> lasso;
    std::vector<Point> curve_points;
    float zoom = 1.0f;
    int stamp_width = 80, stamp_height = 80;
    double stamp_scale = 1.0, stamp_angle = 0.0;
    int resize_width = 960, resize_height = 640;
    bool resize_percent = true, resize_lock = true, resize_scale = true;
    int resize_original_width = 960, resize_original_height = 640;
    int resize_handle = -1;
    int canvas_handle = -1;
    Rect handle_original;
    Rect handle_preview;
    Image selection_original;
    std::string error, status = "For Help, press F1";
    std::string screenshot_path;
    int screenshot_frame = 0;
    int rendered_frames = 0;
    char text_buffer[8192] = {};
    TextStyle text_style;
    Point text_origin;
    Lab edited_lab;
    float edited_rgb[3] = {};
    char edited_hex[16] = {};
    int jpeg_quality = 95;
    WarpWorker warp_worker;
    std::shared_ptr<const ConvWarpField> reshape_field;
    std::shared_ptr<const ConvWarpField> stamp_field;
    ReshapeMesh reshape_mesh;
    Point reshape_origin;
    bool reshape_active = false, reshape_render_pending = false, reshape_commit_pending = false;
    bool stamp_render_pending = false, transform_selection = false;
    bool transform_active = false;
    int active_mesh_node = -1;
    double mesh_spacing = 60.0;
    std::uint64_t mesh_generation = 0, stamp_generation = 0;
    double skew_horizontal = 0.0, skew_vertical = 0.0;
    void start_reshape();
    void finish_reshape();
    void poll_warp();
    void request_skew(int width, int height);
    explicit Application(SDL_Window* input_window, SDL_Renderer* input_renderer);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    void event(const SDL_Event& event);
    void frame();
    void ribbon(float width);
    void canvas(float width, float height);
    void help(float x, float y, float width, float height);
    void dialogs();
    void keyboard();
    void command(Command command);
    void execute(Command command);
    void choose_tool(Tool tool);
    void begin_color();
    void refresh_texture(const Image& image);
    void begin_gesture(Point point, bool right);
    void update_gesture(Point point);
    void end_gesture(Point point);
    void finish_text();
    void finish_curve();
    void regenerate_stamp();
    void file_results();
    void save_to(const std::string& path);
    void report(const std::exception& exception);
};
ImU32 packed(Color color);
void classic_icon(ImDrawList& draw, int icon, ImVec2 position, float size,
                  ImU32 color = IM_COL32(40, 70, 100, 255));
bool ribbon_button(const char* id, const char* label, int icon, ImVec2 position, ImVec2 size,
                   bool selected = false, const char* tooltip = nullptr);
} // namespace paint
