#include "localization.hpp"
#include "forms/display.hpp"
#include "forms/editor.hpp"
#include <algorithm>
#include <stdexcept>
namespace paint::forms {
static void publish_pixels(const Image& source, gui_forms::RasterCanvas& canvas, Rect damage) {
    const int width = source.width, height = source.height;
    std::shared_ptr<gui_drawing::Bitmap> bitmap = canvas.bitmap();
    bool replacement = !bitmap || (*bitmap).width() != static_cast<unsigned>(width) ||
                       (*bitmap).height() != static_cast<unsigned>(height);
    if (replacement) {
        bitmap = std::make_shared<gui_drawing::Bitmap>(width, height);
    }
    if (replacement || damage.w <= 0 || damage.h <= 0) {
        damage = {0, 0, width, height};
    }
    int right = std::clamp(damage.x + damage.w, 0, width);
    int bottom = std::clamp(damage.y + damage.h, 0, height);
    damage.x = std::clamp(damage.x, 0, width);
    damage.y = std::clamp(damage.y, 0, height);
    damage.w = right - damage.x;
    damage.h = bottom - damage.y;
    if (damage.w <= 0 || damage.h <= 0) {
        return;
    }
    gui_drawing::BitmapEditView edit = (*bitmap).begin_edit({damage.x, damage.y, damage.w, damage.h});
    for (int y = 0; y < damage.h; ++y) {
        std::byte* row = edit.writable_data + static_cast<std::size_t>(y) * edit.row_bytes;
        for (int x = 0; x < damage.w; ++x) {
            const int sx = x + damage.x;
            const int sy = y + damage.y;
            Color color = source.pixels[static_cast<std::size_t>(sy) * source.width + sx];
            // Rounded premultiplication p = (channel * alpha + 127) / 255.
            row[x * 4] = static_cast<std::byte>((color.b * color.a + 127U) / 255U);
            row[x * 4 + 1] = static_cast<std::byte>((color.g * color.a + 127U) / 255U);
            row[x * 4 + 2] = static_cast<std::byte>((color.r * color.a + 127U) / 255U);
            row[x * 4 + 3] = static_cast<std::byte>(color.a);
        }
    }
    static_cast<void>((*bitmap).commit_edit(edit.token));
    if (replacement) {
        canvas.set_bitmap(bitmap);
        if (canvas.last_resource_error() != gui_forms::ImageResourceError::none) {
            throw std::runtime_error(tr("The image exceeds the window's display resource budget"));
        }
    } else if (!canvas.synchronize_bitmap()) {
        throw std::runtime_error(tr("Canvas resource synchronization failed"));
    }
}
void publish_image(const Image& source, gui_forms::RasterCanvas& canvas, Rect damage) {
    publish_pixels(source, canvas, damage);
}
void publish_image(const Image& source, PaintCanvas& canvas, Rect damage) {
    publish_pixels(source, canvas, damage);
    canvas.publish_source(source, damage);
}
} // namespace paint::forms
