#include "forms/display.hpp"
#include "forms/editor.hpp"
#include "localization.hpp"
namespace paint::forms {
namespace gf = gui_forms;
struct StickyNoteJob {
    StickyNoteSettings settings;
    StickyNoteResult result;
    std::string error;
    std::atomic<bool> cancel{false}, done{false};
};
namespace {
struct StickyDelivery {
    std::weak_ptr<EditorDialog> owner;
    std::shared_ptr<StickyNoteJob> job;
    void operator()() const {
        const std::shared_ptr<EditorDialog> dialog = owner.lock();
        if (dialog) {
            (*dialog).deliver_sticky_note(job);
        }
    }
};
struct StickyWork {
    EditorDialog* dispatcher;
    std::weak_ptr<EditorDialog> owner;
    std::shared_ptr<StickyNoteJob> job;
    void operator()() const {
        try {
            (*job).result = render_sticky_note((*job).settings, &(*job).cancel);
        } catch (const std::exception& error) {
            (*job).error = error.what();
        }
        (*job).done.store(true, std::memory_order_release);
        static_cast<void>((*dispatcher).begin_invoke(StickyDelivery{owner, job}));
    }
};
} // namespace
void EditorDialog::initialize_sticky_note() {
    panel_ = {0, 0, 780, 510};
    const std::shared_ptr<Editor> editor = editor_.lock();
    sticky_settings_ = (*editor).sticky_note_settings;
    sticky_revision_ = (*editor).document.revision;
    sticky_epoch_ = (*editor).document.atlas_epoch;
    label("sticky-text-label", tr("Text"), {20, 44, 350, 22});
    sticky_text_ = gf::make_control<gf::TextBox>(gf::StableId("sticky-text"), sticky_settings_.text);
    (*sticky_text_).set_multiline(true);
    (*sticky_text_).set_word_wrap(true);
    (*sticky_text_).set_maximum_length(8191);
    (*sticky_text_).set_accessible_name(tr("Text"));
    put(sticky_text_, {20, 69, 350, 140});
    const char* names[] = {"Width (px)", "Height (px)", "Font size", "Padding"};
    const double lows[] = {16, 16, 8, 0}, highs[] = {4096, 4096, 256, 2047};
    const int values[] = {sticky_settings_.width, sticky_settings_.height, sticky_settings_.font.size,
                          sticky_settings_.padding};
    for (int index = 0; index < 4; ++index) {
        const double x = 20 + (index % 2) * 185.0, y = 218 + (index / 2) * 64.0;
        label("sticky-label-" + std::to_string(index), tr(names[index]), {x, y, 165, 24});
        const std::shared_ptr<gf::NumericUpDown> value =
            number("sticky-value-" + std::to_string(index), {x, y + 26, 165, 28}, lows[index], highs[index],
                   values[index]);
        (*value).set_accessible_name(tr(names[index]));
        sticky_values_.push_back(value);
    }
    label("sticky-paper-label", tr("Paper texture"), {20, 344, 350, 22});
    sticky_paper_ = gf::make_control<gf::ComboBox>(gf::StableId("sticky-paper"));
    (*sticky_paper_)
        .set_items({tr("Book paper"), tr("Copy paper"), tr("Recycled paper"), tr("Wove paper"),
                    tr("Laid paper"), tr("Bible paper"), tr("Parchment"), tr("Cotton rag"), tr("Kraft paper"),
                    tr("Night paper")});
    (*sticky_paper_).set_selected_index(static_cast<std::size_t>(sticky_settings_.paper));
    (*sticky_paper_).set_accessible_name(tr("Paper texture"));
    put(sticky_paper_, {20, 368, 350, 28});
    label("sticky-paper-color-label", tr("Paper color"), {20, 399, 165, 22});
    label("sticky-ink-color-label", tr("Text color"), {205, 399, 165, 22});
    sticky_paper_color_ = gf::make_control<gf::TextBox>(gf::StableId("sticky-paper-color"),
                                                        to_hex(sticky_settings_.paper_color));
    sticky_ink_color_ =
        gf::make_control<gf::TextBox>(gf::StableId("sticky-ink-color"), to_hex(sticky_settings_.ink_color));
    (*sticky_paper_color_).set_accessible_name(tr("Paper color"));
    (*sticky_ink_color_).set_accessible_name(tr("Text color"));
    put(sticky_paper_color_, {20, 423, 165, 28});
    put(sticky_ink_color_, {205, 423, 165, 28});
    label("sticky-preview-label", tr("Preview"), {400, 44, 360, 22});
    sticky_canvas_ = gf::make_control<gf::RasterCanvas>(gf::StableId("sticky-preview-canvas"));
    (*sticky_canvas_).set_accessible_name(tr("Preview"));
    put(sticky_canvas_, {400, 69, 360, 280});
    sticky_status_ = gf::make_control<gf::Label>(gf::StableId("sticky-status"));
    (*sticky_status_).set_text_wrapping(gf::TextWrapping::word);
    put(sticky_status_, {400, 356, 360, 56});
    sticky_font_ = gf::make_control<gf::ComboBox>(gf::StableId("sticky-font"));
    (*sticky_font_).set_items({tr("Regular"), tr("Bold"), tr("Italic"), tr("Monospace")});
    (*sticky_font_)
        .set_selected_index(sticky_settings_.font.mono     ? 3
                            : sticky_settings_.font.italic ? 2
                            : sticky_settings_.font.bold   ? 1
                                                           : 0);
    (*sticky_font_).set_accessible_name(tr("Font"));
    put(sticky_font_, {400, 423, 175, 28});
    sticky_seed_ = button("sticky-seed", tr("New paper"), {590, 423, 170, 28});
    error_ = gf::make_control<gf::Label>(gf::StableId("dialog-error"));
    put(error_, {20, 453, 740, 22});
    sticky_preview_ = button("sticky-preview", tr("Preview"), {20, 478, 150, 28});
    sticky_place_ = button("sticky-place", tr("Place note"), {520, 478, 130, 28});
    (*sticky_place_).set_enabled(false);
    button("dialog-cancel", tr("Cancel"), {660, 478, 100, 28});
    for (const std::shared_ptr<gf::Button>& control : {sticky_preview_, sticky_place_, sticky_seed_}) {
        subscriptions_.push_back((*control).clicked().subscribe(
            *this,
            gf::Delegate<gf::ButtonBase&>::bind<EditorDialog, &EditorDialog::sticky_note_clicked>(*this)));
    }
    for (const std::shared_ptr<gf::TextBox>& control :
         {sticky_text_, sticky_paper_color_, sticky_ink_color_}) {
        subscriptions_.push_back((*control).text_changed().subscribe(
            *this,
            gf::Delegate<const std::string&>::bind<EditorDialog, &EditorDialog::sticky_note_text>(*this)));
    }
    for (const std::shared_ptr<gf::NumericUpDown>& control : sticky_values_) {
        subscriptions_.push_back((*control).value_changed().subscribe(
            *this, gf::Delegate<double>::bind<EditorDialog, &EditorDialog::sticky_note_number>(*this)));
    }
    for (const std::shared_ptr<gf::ComboBox>& control : {sticky_paper_, sticky_font_}) {
        subscriptions_.push_back((*control).selected_index_changed().subscribe(
            *this,
            gf::Delegate<std::optional<std::size_t>>::bind<EditorDialog, &EditorDialog::sticky_note_choice>(
                *this)));
    }
}
void EditorDialog::stop_sticky_note() {
    if (sticky_job_) {
        (*sticky_job_).cancel.store(true);
    }
    if (sticky_worker_.joinable()) {
        sticky_worker_.join();
    }
    sticky_job_.reset();
}
void EditorDialog::invalidate_sticky_note() {
    if (sticky_job_) {
        (*sticky_job_).cancel.store(true);
    }
    (*sticky_place_).set_enabled(false);
    (*sticky_canvas_).clear_bitmap();
    (*sticky_status_).set_text(tr("Preview"));
    (*error_).set_text("");
}
void EditorDialog::sticky_note_number(double) {
    invalidate_sticky_note();
}
void EditorDialog::sticky_note_choice(std::optional<std::size_t>) {
    invalidate_sticky_note();
}
void EditorDialog::sticky_note_text(const std::string&) {
    invalidate_sticky_note();
}
void EditorDialog::start_sticky_note() {
    stop_sticky_note();
    invalidate_sticky_note();
    try {
        sticky_settings_.text = (*sticky_text_).text();
        sticky_settings_.width = static_cast<int>((*sticky_values_[0]).value());
        sticky_settings_.height = static_cast<int>((*sticky_values_[1]).value());
        sticky_settings_.font = {};
        sticky_settings_.font.size = static_cast<int>((*sticky_values_[2]).value());
        sticky_settings_.padding = static_cast<int>((*sticky_values_[3]).value());
        sticky_settings_.paper = static_cast<int>((*sticky_paper_).selected_index().value_or(0));
        if (sticky_settings_.padding * 2 >= sticky_settings_.width ||
            sticky_settings_.padding * 2 >= sticky_settings_.height) {
            throw std::invalid_argument(tr("Padding must be less than half the note size."));
        }
        if (sticky_settings_.text.size() > 8191) {
            throw std::invalid_argument(tr("Note text is too long (8191 UTF-8 bytes maximum)."));
        }
        const std::size_t font = (*sticky_font_).selected_index().value_or(0);
        sticky_settings_.font.bold = font == 1;
        sticky_settings_.font.italic = font == 2;
        sticky_settings_.font.mono = font == 3;
        if (!from_hex(std::string((*sticky_paper_color_).text()), sticky_settings_.paper_color) ||
            !from_hex(std::string((*sticky_ink_color_).text()), sticky_settings_.ink_color)) {
            throw std::invalid_argument(tr("Invalid color"));
        }
        sticky_job_ = std::make_shared<StickyNoteJob>();
        (*sticky_job_).settings = sticky_settings_;
        (*sticky_preview_).set_enabled(false);
        (*sticky_seed_).set_enabled(false);
        (*sticky_status_).set_text(tr("Preparing preview…"));
        sticky_worker_ = std::thread(
            StickyWork{this, std::static_pointer_cast<EditorDialog>(shared_from_this()), sticky_job_});
    } catch (const std::exception& error) {
        (*error_).set_text(error.what());
        (*sticky_preview_).set_enabled(true);
        (*sticky_seed_).set_enabled(true);
    }
}
void EditorDialog::deliver_sticky_note(const std::shared_ptr<StickyNoteJob>& job) {
    if (!attached_window() || job != sticky_job_ || !(*job).done.load(std::memory_order_acquire)) {
        return;
    }
    if (sticky_worker_.joinable()) {
        sticky_worker_.join();
    }
    (*sticky_preview_).set_enabled(true);
    (*sticky_seed_).set_enabled(true);
    if ((*job).cancel.load()) {
        return;
    }
    if (!(*job).error.empty()) {
        (*error_).set_text((*job).error);
        return;
    }
    IconFrame frame;
    frame.image = (*job).result.image;
    publish_image(render_asset_preview(frame, 360, 280), *sticky_canvas_);
    (*sticky_place_).set_enabled(!(*job).result.text_clipped);
    (*sticky_status_)
        .set_text(
            tr((*job).result.text_clipped
                   ? "Text does not fit. Enlarge the note or reduce the font size."
                   : "Place the note, then Apply on the canvas. Edit note remains available until Apply."));
}
void EditorDialog::sticky_note_clicked(gf::ButtonBase& control) {
    const std::shared_ptr<Editor> editor = editor_.lock();
    if (!editor) {
        return;
    }
    try {
        const std::string id(control.stable_id().value());
        if (id == "sticky-seed") {
            ++sticky_settings_.seed;
            start_sticky_note();
        } else if (id == "sticky-preview") {
            start_sticky_note();
        } else if (id == "sticky-place" && sticky_job_ &&
                   (*sticky_job_).done.load(std::memory_order_acquire) && !(*sticky_job_).cancel.load() &&
                   (*sticky_job_).error.empty() && !(*sticky_job_).result.text_clipped) {
            if ((*editor).document.revision != sticky_revision_ ||
                (*editor).document.atlas_epoch != sticky_epoch_) {
                throw std::runtime_error(tr("The document changed. Open the note again."));
            }
            (*editor).use_sticky_note((*sticky_job_).result.image, (*sticky_job_).settings);
            (*editor).close_editor_dialog();
        }
    } catch (const std::exception& error) {
        (*error_).set_text(error.what());
    }
}
} // namespace paint::forms
