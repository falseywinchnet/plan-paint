#include "forms/editor.hpp"
#include <algorithm>
#include <cmath>
namespace paint::forms {
namespace gf = gui_forms;
bool Editor::atlas_painting() const {
    return document.atlas.kind != AtlasKind::None && document.atlas.active >= 0;
}
bool Editor::guide_pointer(const gf::PointerEvent& event, Point point) {
    if (document.tool != Tool::Guide || event.button == gf::PointerButton::middle || panning_) {
        return false;
    }
    if (event.action == gf::PointerAction::down) {
        if (event.button == gf::PointerButton::secondary) {
            guide.closed = guide.nodes.size() >= 3;
        } else if (event.button == gf::PointerButton::primary) {
            guide_node_ = -1;
            for (std::size_t index = 0; index < guide.nodes.size(); ++index) {
                if (std::hypot(point.x - guide.nodes[index].x, point.y - guide.nodes[index].y) *
                        canvas().zoom() <=
                    8) {
                    guide_node_ = static_cast<int>(index);
                    break;
                }
            }
            guide_moving_ = guide_node_ < 0 && guide.closed && inside_polygon(guide.nodes, point.x, point.y);
            if (guide_node_ < 0 && !guide_moving_) {
                if (guide.closed) {
                    guide.clear();
                }
                guide.nodes.push_back(point);
                guide_node_ = static_cast<int>(guide.nodes.size()) - 1;
            }
            guide_last_ = point;
            canvas().set_pointer_capture(true);
        }
    } else if ((event.action == gf::PointerAction::move || event.action == gf::PointerAction::up) &&
               canvas().has_pointer_capture()) {
        if (guide_moving_) {
            const Point delta{std::round(point.x - guide_last_.x), std::round(point.y - guide_last_.y)};
            guide.translate(delta);
            guide_last_.x += delta.x;
            guide_last_.y += delta.y;
        } else if (guide_node_ >= 0) {
            guide.nodes[guide_node_] = point;
            guide.selection = {};
        }
        if (event.action == gf::PointerAction::up) {
            guide_node_ = -1;
            guide_moving_ = false;
            canvas().set_pointer_capture(false);
        }
    }
    canvas().invalidate(gf::Dirty::paint);
    return true;
}
void Editor::paint_guide_overlay(gf::Painter& painter) {
    if (!guide.active() && guide.nodes.empty()) {
        return;
    }
    const gf::Color blue = gf::Color::rgba(26, 112, 174, 230), white = gf::Color::rgba(255, 255, 255, 230);
    const std::size_t edges = guide.nodes.size() < 2 ? 0
                              : guide.closed         ? guide.nodes.size()
                                                     : guide.nodes.size() - 1;
    for (std::size_t index = 0; index < edges; ++index) {
        const gf::Point a = screen(guide.nodes[index]),
                        b = screen(guide.nodes[(index + 1) % guide.nodes.size()]);
        painter.draw_line(a, b, white, 3);
        painter.draw_line(a, b, blue, 1);
    }
    if (guide.fill && guide.closed) {
        // Sparse cross-hatching identifies the protected body without hiding it.
        double left = document.image.width, top = document.image.height, right = 0, bottom = 0;
        for (std::size_t index = 0; index < guide.nodes.size(); ++index) {
            left = std::min(left, guide.nodes[index].x);
            top = std::min(top, guide.nodes[index].y);
            right = std::max(right, guide.nodes[index].x);
            bottom = std::max(bottom, guide.nodes[index].y);
        }
        const double step = std::max(1.0, 18 / canvas().zoom());
        for (double y = std::max(0.0, top); y < std::min(static_cast<double>(document.image.height), bottom);
             y += step) {
            for (double x = std::max(0.0, left);
                 x < std::min(static_cast<double>(document.image.width), right); x += step) {
                if (guide.blocked(static_cast<int>(x), static_cast<int>(y)) > 0.5) {
                    const gf::Point p = screen({x, y});
                    painter.draw_line({p.x - 2, p.y + 2}, {p.x + 2, p.y - 2},
                                      gf::Color::rgba(26, 112, 174, 85), 1);
                }
            }
        }
    }
    if (document.tool == Tool::Guide) {
        for (std::size_t index = 0; index < guide.nodes.size(); ++index) {
            const gf::Point point = screen(guide.nodes[index]);
            painter.fill_rounded_rect({point.x - 4, point.y - 4, 8, 8}, 4, blue);
            painter.stroke_rounded_rect({point.x - 4, point.y - 4, 8, 8}, 4, white, 1);
        }
    }
}
void Editor::paint_segment(Point start, Point end) {
    const bool wrap = atlas_painting() && atlas_wrap;
    if (document.tool == Tool::Eraser && atlas_painting() && atlas_preserve_alpha &&
        eraser_mode <= EraserMode::Soft) {
        return;
    }
    if (document.tool == Tool::Fill) {
        stencil_flood(document.image, start_, gesture_ink_, guide, wrap);
        return;
    }
    // Repeat only footprints which intersect this torus cell. The same material
    // coverage object owns overlap so duplicate edge passes do not add coats.
    const double radius = gesture_ink_.size * 0.5 + 2;
    const int min_x =
        wrap ? static_cast<int>(std::ceil((-std::max(start.x, end.x) - radius) / document.image.width)) : 0;
    const int max_x =
        wrap ? static_cast<int>(std::floor((document.image.width - std::min(start.x, end.x) + radius) /
                                           document.image.width))
             : 0;
    const int min_y =
        wrap ? static_cast<int>(std::ceil((-std::max(start.y, end.y) - radius) / document.image.height)) : 0;
    const int max_y =
        wrap ? static_cast<int>(std::floor((document.image.height - std::min(start.y, end.y) + radius) /
                                           document.image.height))
             : 0;
    if (document.tool == Tool::Brush && brush_family == BrushFamily::Additive &&
        gesture_ink_.brush != Brush::Round) {
        dynamic_brush_.segment(document.image, start, end, gesture_ink_, glitter, wrap);
        return;
    }
    if (document.tool == Tool::Brush && brush_family == BrushFamily::Heal) {
        healing_brush_.segment(document.image, start, end, gesture_ink_.size, heal_hardness, heal_correction,
                               wrap);
        return;
    }
    if (document.tool == Tool::Brush && brush_family == BrushFamily::Mix) {
        transform_brush_.segment(document.image, start, end, gesture_ink_.size, mix_effect, effect_strength,
                                 effect_scale, effect_phase, wrap);
        return;
    }
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const Point a{start.x + x * document.image.width, start.y + y * document.image.height};
            const Point b{end.x + x * document.image.width, end.y + y * document.image.height};
            if (document.tool == Tool::Pencil) {
                pixel_line(document.image, a, b, gesture_ink_);
            } else if (document.tool == Tool::Brush) {
                material_.segment(document.image, a, b, gesture_ink_);
            } else if (document.tool == Tool::Eraser) {
                if (eraser_mode == EraserMode::Hard || eraser_mode == EraserMode::Soft) {
                    eraser_.segment(document.image, a, b, gesture_ink_.size,
                                    eraser_soft || eraser_mode == EraserMode::Soft);
                } else {
                    const MixEffect effect = eraser_mode == EraserMode::Blur      ? MixEffect::Blur
                                             : eraser_mode == EraserMode::Sharpen ? MixEffect::Sharpen
                                                                                  : MixEffect::Smudge;
                    transform_brush_.segment(document.image, a, b, gesture_ink_.size, effect, effect_strength,
                                             effect_scale, effect_phase, wrap);
                }
            }
        }
    }
}
} // namespace paint::forms

namespace paint::forms {
void Editor::begin_path_swap(CurveKind kind) {
    guide.clear();
    document.tool = Tool::Path;
    path_swap_kind_ = kind;
    path_swap_segment_ = -1;
    path_swap_handle_ = -1;
    refresh();
}
bool Editor::path_swap_pointer(const gf::PointerEvent& event, Point point) {
    if (!path_swap_kind_ || document.tool != Tool::Path || event.button == gf::PointerButton::middle ||
        panning_) {
        return false;
    }
    if (event.action == gf::PointerAction::down && event.button == gf::PointerButton::secondary) {
        path_swap_kind_.reset();
        path_swap_segment_ = -1;
        path_swap_handle_ = -1;
        canvas().set_pointer_capture(false);
        refresh();
        return true;
    }
    if (event.action == gf::PointerAction::down && event.button == gf::PointerButton::primary) {
        if (path_swap_segment_ >= 0 &&
            static_cast<std::size_t>(path_swap_segment_) < document.path.segments.size()) {
            const CurveGeometry& geometry = document.path.segments[path_swap_segment_].geometry;
            for (int index = 0; index < geometry.handle_count(); ++index) {
                const Point handle = geometry.handle(index);
                if (std::hypot(point.x - handle.x, point.y - handle.y) * canvas().zoom() < 10) {
                    path_swap_handle_ = index;
                    path_swap_checkpoint_ = false;
                    canvas().set_pointer_capture(true);
                    return true;
                }
            }
        }
        const std::vector<PathEdge> edges = document.path_edges();
        double closest = 8 / canvas().zoom();
        int selected = -1;
        for (std::size_t index = 0; index < edges.size(); ++index) {
            std::vector<Point> samples{document.path.nodes[edges[index].first],
                                       document.path.nodes[edges[index].last]};
            for (std::size_t curve = 0; curve < document.path.segments.size(); ++curve) {
                const PathSegment& segment = document.path.segments[curve];
                if (segment.first == edges[index].first && segment.last == edges[index].last) {
                    samples = segment.geometry.samples();
                    break;
                }
            }
            for (std::size_t sample = 1; sample < samples.size(); ++sample) {
                const Point a = samples[sample - 1], b = samples[sample];
                const double dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
                const double t =
                    length > 0 ? std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length, 0.0, 1.0)
                               : 0;
                const double distance = std::hypot(point.x - a.x - t * dx, point.y - a.y - t * dy);
                if (distance < closest) {
                    closest = distance;
                    selected = static_cast<int>(index);
                }
            }
        }
        if (selected >= 0) {
            path_swap_segment_ = document.swap_path_segment(edges[selected], *path_swap_kind_);
            refresh();
        }
    } else if ((event.action == gf::PointerAction::move || event.action == gf::PointerAction::up) &&
               path_swap_handle_ >= 0) {
        if (path_swap_segment_ < 0 ||
            static_cast<std::size_t>(path_swap_segment_) >= document.path.segments.size()) {
            path_swap_handle_ = -1;
            canvas().set_pointer_capture(false);
            return true;
        }
        if (!path_swap_checkpoint_) {
            document.checkpoint();
            path_swap_checkpoint_ = true;
        }
        document.path.segments[path_swap_segment_].geometry.move_handle(path_swap_handle_, point);
        document.sync_path();
        refresh();
        if (event.action == gf::PointerAction::up) {
            path_swap_handle_ = -1;
            canvas().set_pointer_capture(false);
        }
    }
    return true;
}
void Editor::paint_path_swap(gf::Painter& painter) {
    if (!path_swap_kind_ || path_swap_segment_ < 0 ||
        static_cast<std::size_t>(path_swap_segment_) >= document.path.segments.size()) {
        return;
    }
    const CurveGeometry& geometry = document.path.segments[path_swap_segment_].geometry;
    const gf::Color blue = gf::Color::rgba(30, 110, 190), white = gf::Color::rgba(255, 255, 255);
    for (int index = 0; index < geometry.handle_count(); ++index) {
        const gf::Point point = screen(geometry.handle(index));
        if (geometry.kind == CurveKind::Bezier) {
            painter.draw_line(screen(index == 0 ? geometry.start : geometry.end), point, blue, 1);
        }
        painter.fill_rect({point.x - 5, point.y - 5, 10, 10}, white);
        painter.stroke_rect({point.x - 5, point.y - 5, 10, 10}, blue, 2);
    }
}
} // namespace paint::forms
