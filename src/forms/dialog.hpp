#pragma once
#include "carpet.hpp"
#include "color_tools.hpp"
#include "image.hpp"
#include <gui_forms/basic_controls.hpp>
#include <gui_forms/canvas.hpp>
#include <gui_forms/controls/panel/combo_box/combo_box.hpp>
#include <gui_forms/controls/panel/numeric_up_down/numeric_up_down.hpp>
#include <gui_forms/controls/panel/text_box/text_box.hpp>
#include <gui_forms/controls/range_control/track_bar/track_bar.hpp>
#include <memory>
#include <thread>
namespace paint::forms {
class Editor;
class SwatchButton;
class EditorDialog;
struct CarpetRenderJob;
struct DitherRenderJob;
enum class EditorDialogKind {
    dither,
    tool_size,
    carpet,
    about,
    color,
    resize,
    atlas_grid,
    icon_sizes,
    cursor_sizes,
    hotspot,
    atlas_gallery,
    properties,
    settings,
    print_preview,
    linux_print,
    linux_page_setup
};
class ColorPlane final : public gui_forms::Control {
  public:
    ColorPlane(gui_forms::StableId id, std::weak_ptr<EditorDialog> dialog, bool hue_strip);
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void on_pointer(gui_forms::PointerEvent& event) override;
    void on_detaching_from_window(gui_forms::Window& window) noexcept override;

  private:
    std::weak_ptr<EditorDialog> dialog_;
    bool hue_strip_;
    gui_forms::ImageId mosaic_image_;
    const ColorMosaic* cached_mosaic_ = nullptr;
    int mosaic_width_ = 0, mosaic_height_ = 0;
    std::vector<std::uint8_t> mosaic_cells_;
    std::size_t selected_cell_ = 96;
    std::vector<std::vector<Point>> selected_contours_;
    void paint_mosaic(gui_forms::Painter& painter, EditorDialog& dialog, gui_forms::Rect bounds);
};
class EditorDialog final : public gui_forms::Control {
  public:
    EditorDialog(gui_forms::StableId id, std::weak_ptr<Editor> editor, EditorDialogKind kind, bool secondary);
    ~EditorDialog() override;
    void on_attached_to_window() override;
    void on_detaching_from_window(gui_forms::Window& window) noexcept override;
    void deliver_carpet();
    void deliver_dither();
    static constexpr bool initialize_tree_after_construction = true;
    void initialize_control_tree();
    void arrange(gui_forms::Rect bounds) override;
    void on_paint(gui_forms::Painter& painter, gui_forms::Rect damage) override;
    void on_pointer(gui_forms::PointerEvent& event) override;
    void on_key_preview(gui_forms::KeyEvent& event) override;
    gui_forms::SemanticDescriptor semantic_descriptor() const override;
    void choose_hsv(double hue, double saturation, double value);
    double hue = 0, saturation = 0, value = 0;
    Color plane_color(double h, double s, double level) const;
    const ColorMosaic& color_mosaic(double hue) const;
    PickerSpace picker_space = PickerSpace::RGB;
    bool mosaic = false;

  private:
    void initialize_dither();
    void render_dither();
    void stop_dither();
    void accept_dither();
    void dither_count_changed(double);
    void dither_pattern_changed(std::optional<std::size_t>);
    std::shared_ptr<DitherRenderJob> dither_job_;
    std::thread dither_worker_;
    std::shared_ptr<gui_forms::NumericUpDown> dither_count_;
    std::shared_ptr<gui_forms::ComboBox> dither_pattern_;
    std::shared_ptr<gui_forms::RasterCanvas> dither_preview_;
    std::shared_ptr<gui_forms::Label> dither_status_;
    std::shared_ptr<gui_forms::ComboBox> shape_gesture_;
    CarpetParameters carpet_;
    Image carpet_image_;
    std::shared_ptr<CarpetRenderJob> carpet_job_;
    std::thread carpet_worker_;
    std::shared_ptr<gui_forms::RasterCanvas> carpet_preview_;
    std::shared_ptr<gui_forms::ComboBox> carpet_presets_;
    std::shared_ptr<gui_forms::CheckBox> carpet_hillshade_;
    std::shared_ptr<gui_forms::TextBox> carpet_color_;
    std::vector<std::shared_ptr<gui_forms::TrackBar>> carpet_sliders_;
    std::vector<std::shared_ptr<gui_forms::Label>> carpet_values_;
    void initialize_carpet();
    void sync_carpet();
    void carpet_changed(double value);
    void carpet_color_changed(const std::string& value);
    void carpet_preset_changed(std::optional<std::size_t> index);
    void render_carpet_preview();
    void stop_carpet();
    std::weak_ptr<Editor> editor_;
    mutable std::array<std::unique_ptr<ColorMosaic>, 48> mosaics_;
    std::shared_ptr<gui_forms::ComboBox> background_, alpha_background_;
    std::shared_ptr<gui_forms::CheckBox> mosaic_;
    void color_space_changed(PickerSpace space);
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
    std::shared_ptr<gui_forms::NumericUpDown> red_, green_, blue_, alpha_, light_, a_, b_, width_, height_,
        skew_horizontal_, skew_vertical_;
    std::shared_ptr<gui_forms::TextBox> hex_, alpha_background_color_;
    std::shared_ptr<gui_forms::ComboBox> printer_;
    std::shared_ptr<gui_forms::CheckBox> lock_, scale_, rotate_view_;
    std::shared_ptr<gui_forms::Label> error_;
    std::shared_ptr<gui_forms::Button> primary_tab_, secondary_tab_, pixel_tab_, percent_tab_, rgb_tab_,
        okhsl_tab_;
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
    std::vector<std::shared_ptr<gui_forms::NumericUpDown>> atlas_numbers_;
    std::vector<std::shared_ptr<gui_forms::CheckBox>> icon_sizes_;
    std::string title() const;
    void atlas_changed(double value);
    void accept();
};
} // namespace paint::forms
