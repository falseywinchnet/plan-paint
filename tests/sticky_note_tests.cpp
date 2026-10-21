#include "sticky_note.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
bool same(const paint::Image& a, const paint::Image& b) {
    if (a.width != b.width || a.height != b.height) {
        return false;
    }
    for (std::size_t i = 0; i < a.pixels.size(); ++i) {
        if (!paint::equal(a.pixels[i], b.pixels[i])) {
            return false;
        }
    }
    return true;
}
void paper_and_text() {
    paint::StickyNoteSettings settings;
    settings.width = 96;
    settings.height = 80;
    settings.padding = 8;
    settings.font.size = 16;
    for (int paper = 0; paper < 10; ++paper) {
        settings.paper = paper;
        const paint::StickyNoteResult blank = paint::render_sticky_note(settings);
        require(blank.image.width == 96 && blank.image.height == 80 && !blank.text_clipped,
                "paper dimensions or blank text extent changed");
        require(same(blank.image, paint::render_sticky_note(settings).image),
                "paper seed is not deterministic");
        ++settings.seed;
        require(!same(blank.image, paint::render_sticky_note(settings).image),
                "paper seed does not vary texture");
        --settings.seed;
        for (paint::Color color : blank.image.pixels) {
            require(color.a == 255, "paper must remain opaque");
        }
        settings.text = "Plan\nPaint";
        const paint::StickyNoteResult text = paint::render_sticky_note(settings);
        require(!text.text_clipped && !same(blank.image, text.image), "fitting note text did not render");
        for (int y = 0; y < 80; ++y) {
            for (int x = 0; x < 96; ++x) {
                if (x < 8 || x >= 88 || y < 8 || y >= 72) {
                    require(paint::equal(blank.image.get(x, y), text.image.get(x, y)),
                            "text escaped padding");
                }
            }
        }
        settings.text.clear();
    }
    settings.text = "A long note with enough words to overflow this small sheet of paper.";
    require(paint::render_sticky_note(settings).text_clipped, "overflow must be reported before placement");
    settings.height = 320;
    require(!paint::render_sticky_note(settings).text_clipped, "word wrapping must use the enlarged note");
    settings.text = "W";
    settings.width = 16;
    settings.height = 32;
    settings.padding = 7;
    require(paint::render_sticky_note(settings).text_clipped, "a single wide glyph must report overflow");
}
void invalid_and_cancelled() {
    for (int invalid = 0; invalid < 7; ++invalid) {
        paint::StickyNoteSettings settings;
        if (invalid == 0) {
            settings.width = 4097;
        }
        if (invalid == 1) {
            settings.height = 15;
        }
        if (invalid == 2) {
            settings.padding = 96;
        }
        if (invalid == 3) {
            settings.font.size = 257;
        }
        if (invalid == 4) {
            settings.paper = 10;
        }
        if (invalid == 5) {
            settings.paper_color.a = 254;
        }
        if (invalid == 6) {
            settings.text.assign(8192, 'x');
        }
        bool rejected = false;
        try {
            static_cast<void>(paint::render_sticky_note(settings));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "invalid note settings were accepted");
    }
    const std::atomic<bool> cancel{true};
    bool cancelled = false;
    try {
        static_cast<void>(paint::render_sticky_note({}, &cancel));
    } catch (const std::runtime_error&) {
        cancelled = true;
    }
    require(cancelled, "cancelled note generation must not publish an image");
}
} // namespace
int main() {
    try {
        paper_and_text();
        invalid_and_cancelled();
        std::cout << "Sticky paper, text, overflow and cancellation contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
