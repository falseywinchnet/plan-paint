#pragma once
#include "atlas.hpp"
#include "curve.hpp"
#include "raster.hpp"
#include <deque>
#include <memory>
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
// Every run shares an immutable session background. Retained node edits replay
// the runs in order, preserving their styles and the artwork underneath.
struct PathRun {
    std::size_t start = 0, count = 0;
    Ink ink;
    Ink alternate;
    bool outline = true, fill = false, continuous = true;
    Brush fill_brush = Brush::Round;
};
struct EditablePath {
    std::vector<Point> nodes;
    std::vector<PathRun> runs;
    std::shared_ptr<const Image> base;
    std::size_t start = 0;
    std::uint64_t session = 0;
    bool extending = false;
    Ink ink;
    Ink alternate;
    bool outline = true, fill = false, continuous = false;
    Brush fill_brush = Brush::Round;
};
struct EditableCurve {
    CurveGeometry geometry;
    std::shared_ptr<const Image> base;
    std::uint64_t session = 0;
    bool line_set = false, secondary = false;
    Ink ink;
};
struct Snapshot {
    Image image;
    AtlasState atlas;
    std::uint64_t revision = 0;
    EditablePath path;
    EditableCurve curve;
    std::size_t bytes() const;
};
struct Document {
    Image image;
    AtlasState atlas;
    std::uint64_t atlas_epoch = 0;
    FloatingSelection selection;
    Image stamp;
    Ink ink, alt_ink;
    Ink primary_ink() const;
    Ink alternate_ink() const;
    Tool tool = Tool::Pencil;
    Shape shape = Shape::Rectangle;
    StampShape stamp_shape = StampShape::Circle;
    bool stamp_transparent = true;
    bool transparent_selection = false;
    bool shape_outline = true, shape_fill = false;
    Brush shape_fill_brush = Brush::Round;
    bool continuous_path = true;
    EditablePath path;
    std::uint64_t next_path_session = 1;
    EditableCurve curve;
    std::uint64_t next_curve_session = 1;
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
    void add_path_node(Point point);
    void end_path_geometry();
    void move_path_node(std::size_t index, Point point);
    void sync_path();
    void restore_path(const EditablePath& previous);
    Image path_image(const Point* next = nullptr) const;
    void begin_curve(CurveKind kind, Point start, bool secondary = false);
    bool establish_curve(Point end);
    void sync_curve();
    void commit_curve();
    void restore_curve(const EditableCurve& previous);
    Image curve_image(const Point* pending_end = nullptr) const;
    Image visible_image() const;
};
} // namespace paint
