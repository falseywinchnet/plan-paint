#pragma once
#include "document.hpp"
#include "material.hpp"
#include <gui_forms/application.hpp>
#include <gui_forms/basic_controls.hpp>
#include <gui_forms/canvas.hpp>
#include <gui_forms/controls/menu_strip/menu_strip.hpp>
#include <gui_forms/controls/panel/combo_box/combo_box.hpp>
#include <gui_forms/delegate.hpp>
#include <gui_forms/host.hpp>
namespace paint::forms {
class Editor;
class PaintCanvas final : public gui_forms::RasterCanvas {
  public:
    PaintCanvas(gui_forms::StableId id, std::weak_ptr<Editor> editor);
    void on_pointer(gui_forms::PointerEvent& event) override;
    void on_paint_overlay(gui_forms::Painter& painter, gui_forms::Rect damage) override;

  private:
    std::weak_ptr<Editor> editor_;
};
class Editor final : public gui_forms::Control {
  public:
    explicit Editor(gui_forms::StableId id);
    static constexpr bool initialize_tree_after_construction = true;
    void initialize_control_tree();
    Document document;
    void arrange(gui_forms::Rect bounds) override;
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void paint_canvas_overlay(gui_forms::Painter& painter, gui_forms::Rect damage);
    void on_key_preview(gui_forms::KeyEvent& event) override;
    void ready(gui_forms::Window& window, gui_forms::ApplicationWindowHandle handle);
    void closing(gui_forms::HostCloseRequest& request);
    void execute(const std::string& command);
    void choose_tool(Tool tool);
    void refresh();
    void open_file(const std::string& path);
    bool save(bool save_as);
    gui_forms::RasterCanvas& canvas();
    void pointer(const gui_forms::PointerEvent& event);

  private:
    std::shared_ptr<gui_forms::RasterCanvas> canvas_;
    std::shared_ptr<gui_forms::Label> status_;
    std::shared_ptr<gui_forms::MenuStrip> menu_;
    std::shared_ptr<gui_forms::ComboBox> brush_, shape_, pattern_, size_;
    std::vector<std::shared_ptr<gui_forms::Button>> buttons_;
    std::vector<std::shared_ptr<gui_forms::Command>> commands_;
    std::vector<gui_forms::SubscriptionToken> subscriptions_;
    gui_forms::ApplicationWindowHandle handle_;
    std::uint64_t next_dialog_id_ = 1;
    Point start_, last_, current_, selection_offset_, handle_offset_;
    gui_drawing::PointF pan_origin_;
    gui_forms::Point pan_start_;
    std::vector<Point> lasso_;
    Image preview_;
    EraserStroke eraser_;
    MaterialStroke material_;
    bool shift_ = false;
    bool synchronizing_controls_ = false;
    Ink gesture_ink_;
    bool dragging_ = false, moving_selection_ = false, preview_active_ = false, panning_ = false;
    bool handle_checkpoint_ = false;
    int curve_handle_ = -1;
    void update_status();
    void button_clicked(gui_forms::ButtonBase& button);
    void command_invoked(const gui_forms::CommandInvocation& invocation);
    void brush_changed(std::optional<std::size_t> index);
    void shape_changed(std::optional<std::size_t> index);
    void pattern_changed(std::optional<std::size_t> index);
    void size_changed(std::optional<std::size_t> index);
    void add_button(const std::string& id, const std::string& text, gui_forms::Rect bounds);
    gui_forms::MenuItemSpec menu_item(const std::string& id, const std::string& text);
    void begin(Point point, bool secondary);
    void move(Point point);
    void end(Point point);
    void release_gesture();
    void finish_controls();
    void zoom(double factor, gui_forms::Point anchor);
    void copy();
    void paste();
    void edit_color(bool secondary);
    bool can_replace();
    gui_forms::HostServices& services();
    void error(const std::string& message);
    gui_forms::HostDialogResult dialog(gui_forms::HostDialogRequestPayload payload);
    gui_forms::Point screen(Point point) const;
};
} // namespace paint::forms
