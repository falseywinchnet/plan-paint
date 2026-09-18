#include "text_session.hpp"
#include <algorithm>
#include <cmath>
namespace paint {
void TextSession::begin(Point origin) {
    clear();
    active = true;
    bounds = {static_cast<int>(std::round(origin.x)), static_cast<int>(std::round(origin.y)), 440, 160};
}
void TextSession::clear() {
    active = false;
    edit = {};
    undo_.clear();
    redo_.clear();
    signature_.clear();
    preview = {};
    layout = {};
}
void TextSession::refresh(Color foreground, Color background) {
    if (!active) {
        return;
    }
    std::string signature =
        edit.content + "\n" + style.face_path + "@" + std::to_string(style.size) + ":" +
        std::to_string(bounds.w) + ":" + std::to_string(bounds.h) + (style.bold ? "b" : "-") +
        (style.italic ? "i" : "-") + (style.underline ? "u" : "-") + (style.strikeout ? "s" : "-") +
        (style.mono ? "m" : "-") + (style.opaque ? "o" : "-") + (style.word_wrap ? "w" : "-") +
        to_hex(foreground) + std::to_string(foreground.a) + ":" + to_hex(background) +
        std::to_string(background.a);
    if (signature == signature_) {
        return;
    }
    layout = layout_text(edit.content, style, bounds.w);
    preview.reset(bounds.w, bounds.h, style.opaque ? background : Color{0, 0, 0, 0});
    render_text(preview, {0, 0}, layout, style, foreground, background);
    signature_ = std::move(signature);
    edit.caret = std::min(edit.caret, edit.content.size());
    edit.anchor = std::min(edit.anchor, edit.content.size());
}
bool TextSession::replace(const std::string& inserted) {
    std::size_t first = std::min(edit.caret, edit.anchor), last = std::max(edit.caret, edit.anchor);
    std::string value = inserted;
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
    std::size_t available = 8191 - (edit.content.size() - (last - first));
    if (value.size() > available) {
        std::size_t length = available;
        while (length > 0 && (static_cast<unsigned char>(value[length]) & 192) == 128) {
            --length;
        }
        value.resize(length);
    }
    if (first == last && value.empty()) {
        return false;
    }
    undo_.push_back(edit);
    if (undo_.size() > 64) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
    edit.content.replace(first, last - first, value);
    edit.caret = edit.anchor = first + value.size();
    return true;
}
void TextSession::history(bool redo) {
    std::vector<TextEditState>& source = redo ? redo_ : undo_;
    std::vector<TextEditState>& destination = redo ? undo_ : redo_;
    if (source.empty()) {
        return;
    }
    destination.push_back(edit);
    edit = std::move(source.back());
    source.pop_back();
}
std::size_t TextSession::caret_at(Point point) const {
    if (layout.carets.empty()) {
        return 0;
    }
    double best = 1e100;
    std::size_t result = 0;
    for (std::size_t i = 0; i <= layout.glyphs.size(); ++i) {
        std::size_t offset = i == layout.glyphs.size() ? layout.carets.size() - 1 : layout.glyphs[i].begin;
        Point caret = layout.carets[offset];
        double row = std::abs(std::floor(point.y / layout.line_height) - caret.y / layout.line_height);
        double distance = row * 100000 + std::abs(point.x - caret.x);
        if (distance < best) {
            best = distance;
            result = offset;
        }
    }
    return result;
}
void TextSession::resize(Rect rectangle) {
    bounds = rectangle;
    bounds.w = std::clamp(bounds.w, 24, 8192);
    bounds.h = std::clamp(bounds.h, 24, std::min(8192, 16000000 / bounds.w));
}
} // namespace paint
