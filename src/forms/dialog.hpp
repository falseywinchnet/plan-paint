#pragma once
#include "image.hpp"
#include <gui_forms/basic_controls.hpp>
#include <gui_forms/controls/panel/numeric_up_down/numeric_up_down.hpp>
#include <gui_forms/controls/panel/text_box/text_box.hpp>
namespace paint::forms {
class Editor;
class SwatchButton;
class EditorDialog;
enum class EditorDialogKind { color, resize };
class ColorPlane final : public gui_forms::Control {
  public:
    ColorPlane(gui_forms::StableId id, std::weak_ptr<EditorDialog> dialog, bool hue_strip);
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void on_pointer(gui_forms::PointerEvent& event) override;

  private:
    std::weak_ptr<EditorDialog> dialog_;
    bool hue_strip_;
};
class EditorDialog final : public gui_forms::Control {
  public:
    EditorDialog(gui_forms::StableId id, std::weak_ptr<Editor> editor, EditorDialogKind kind, bool secondary);
    static constexpr bool initialize_tree_after_construction = true;
    void initialize_control_tree();
    void arrange(gui_forms::Rect bounds) override;
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void on_pointer(gui_forms::PointerEvent& event) override;
    void on_key_preview(gui_forms::KeyEvent& event) override;
    gui_forms::SemanticDescriptor semantic_descriptor() const override;
    void choose_hsv(double hue, double saturation, double value);
    double hue = 0, saturation = 0, value = 0;

  private:
    std::weak_ptr<Editor> editor_;
    EditorDialogKind kind_;
    bool secondary_ = false, synchronizing_ = false, percent_ = false;
    Color original_, color_;
    int original_width_ = 1, original_height_ = 1, custom_slot_ = 0;
    gui_forms::Rect panel_;
    std::vector<gui_forms::Control::Ptr> controls_;
    std::vector<gui_forms::SubscriptionToken> subscriptions_;
    std::vector<std::shared_ptr<SwatchButton>> custom_;
    std::shared_ptr<SwatchButton> old_, preview_;
    std::shared_ptr<ColorPlane> plane_, hue_strip_;
    std::shared_ptr<gui_forms::NumericUpDown> red_, green_, blue_, alpha_, light_, a_, b_, width_, height_;
    std::shared_ptr<gui_forms::TextBox> hex_;
    std::shared_ptr<gui_forms::CheckBox> lock_, scale_;
    std::shared_ptr<gui_forms::Label> error_;
    std::shared_ptr<gui_forms::Button> primary_tab_, secondary_tab_, pixel_tab_, percent_tab_;
    void put(gui_forms::Control::Ptr control, gui_forms::Rect bounds);
    void label(const std::string& id, const std::string& text, gui_forms::Rect bounds, bool heading = false);
    std::shared_ptr<gui_forms::Button> button(const std::string& id, const std::string& text,
                                              gui_forms::Rect bounds);
    std::shared_ptr<gui_forms::NumericUpDown> number(const std::string& id, gui_forms::Rect bounds,
                                                     double minimum, double maximum, double value,
                                                     int decimals = 0);
    void clicked(gui_forms::ButtonBase& control);
    void rgb_changed(double value);
    void lab_changed(double value);
    void hex_changed(const std::string& value);
    void width_changed(double value);
    void height_changed(double value);
    void set_color(Color color, bool update_hsv = true);
    void accept();
};
} // namespace paint::forms
