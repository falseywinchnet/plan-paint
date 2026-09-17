#pragma once
#include "atlas.hpp"
#include "raster.hpp"
#include <deque>
namespace paint {
enum class Tool {
    Select,
    Lasso,
    Pencil,
    Fill,
    Text,
    Eraser,
    Picker,
    Magnifier,
    Brush,
    Shape,
    Path,
    Stamp,
    Reshape
};
struct FloatingSelection {
    Image image;
    int x = 0, y = 0;
    bool active = false;
    std::vector<std::uint8_t> coverage;
    std::vector<Point> outline;
};
struct Snapshot {
    Image image;
    AtlasState atlas;
    std::uint64_t revision = 0;
};
struct Document {
    Image image;
    AtlasState atlas;
    std::uint64_t atlas_epoch = 0;
    FloatingSelection selection;
    Image stamp;
    Ink ink;
    Tool tool = Tool::Pencil;
    Shape shape = Shape::Rectangle;
    StampShape stamp_shape = StampShape::Circle;
    bool stamp_transparent = true;
    bool transparent_selection = false;
    bool shape_outline = true, shape_fill = false;
    Brush shape_fill_brush = Brush::Round;
    bool continuous_path = false;
    std::vector<Point> path;
    std::string filename;
    std::uint64_t revision = 0, saved_revision = 0, next_revision = 1;
    std::deque<Snapshot> undo_history, redo_history;
    std::size_t history_bytes = 0;
    Document();
    bool dirty() const;
    void replace_container(ImageContainer replacement, const std::string& path);
    void configure_atlas(const AtlasGrid& grid);
    void leave_atlas();
    void sync_atlas();
    void atlas_select(int index, bool control = false);
    void atlas_step(int direction);
    void make_icon_sizes(const std::vector<int>& sizes, bool cursor);
    void set_hotspot(int x, int y);
    Image output_image() const;
    ImageContainer output_container(bool cursor) const;
    void assign_canvas(Image replacement);
    bool fixed_canvas() const;
    bool has_legacy_xor() const;
    void require_rgba_transform() const;
    void checkpoint();
    void undo();
    void redo();
    void replace(Image replacement, const std::string& path);
    void new_image(int width = 960, int height = 640);
    void paste(const Image& pasted, int x = 0, int y = 0);
    void select(Rect bounds, const std::vector<Point>& lasso = {});
    void commit_selection();
    void delete_selection();
    void select_all();
    void invert_selection();
    void crop();
    void resize(int width, int height, bool scale);
    void rotate(int turns);
    void flip(bool horizontal);
    void invert_colors();
    void commit_path();
    Image visible_image() const;
};
} // namespace paint
