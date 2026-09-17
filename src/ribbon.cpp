#include "application.hpp"
#include "gui_scope.hpp"
#include <algorithm>
#include <cmath>
namespace paint {
void classic_icon(ImDrawList& draw, int icon, ImVec2 position, float size, ImU32 color) {
    float x = position.x, y = position.y, s = size;
    ImU32 blue = IM_COL32(60, 135, 203, 255), gold = IM_COL32(233, 174, 55, 255),
          edge = IM_COL32(76, 88, 101, 255);
    if (icon >= 100 && icon < 123) {
        Shape shape = static_cast<Shape>(icon - 100);
        std::vector<Point> points = shape_points(shape, {x + 2, y + 2}, {x + s - 2, y + s - 2});
        for (std::size_t i = 1; i < points.size(); ++i) {
            draw.AddLine(ImVec2(static_cast<float>(points[i - 1].x), static_cast<float>(points[i - 1].y)),
                         ImVec2(static_cast<float>(points[i].x), static_cast<float>(points[i].y)), color,
                         1.1f);
        }
        if (points.size() > 2) {
            draw.AddLine(ImVec2(static_cast<float>(points.back().x), static_cast<float>(points.back().y)),
                         ImVec2(static_cast<float>(points.front().x), static_cast<float>(points.front().y)),
                         color, 1.1f);
        }
        return;
    }
    switch (icon) {
    case 0: // clipboard
        draw.AddRectFilled(ImVec2(x + s * .2f, y + s * .1f), ImVec2(x + s * .8f, y + s * .94f), gold, 1);
        draw.AddRect(ImVec2(x + s * .2f, y + s * .1f), ImVec2(x + s * .8f, y + s * .94f), edge);
        draw.AddRectFilled(ImVec2(x + s * .38f, y), ImVec2(x + s * .63f, y + s * .2f),
                           IM_COL32(190, 198, 203, 255), 2);
        draw.AddRectFilled(ImVec2(x + s * .4f, y + s * .32f), ImVec2(x + s * .98f, y + s * .98f),
                           IM_COL32(255, 255, 255, 255));
        draw.AddRect(ImVec2(x + s * .4f, y + s * .32f), ImVec2(x + s * .98f, y + s * .98f), blue);
        break;
    case 1: // scissors
        draw.AddCircle(ImVec2(x + s * .2f, y + s * .75f), s * .13f, blue, 10, 1.7f);
        draw.AddCircle(ImVec2(x + s * .52f, y + s * .82f), s * .13f, blue, 10, 1.7f);
        draw.AddLine(ImVec2(x + s * .28f, y + s * .67f), ImVec2(x + s * .84f, y + s * .08f), edge, 1.5f);
        draw.AddLine(ImVec2(x + s * .5f, y + s * .69f), ImVec2(x + s * .24f, y + s * .08f), edge, 1.5f);
        break;
    case 2: // copy
        draw.AddRectFilled(ImVec2(x + s * .1f, y + s * .1f), ImVec2(x + s * .68f, y + s * .73f),
                           IM_COL32(255, 255, 255, 255));
        draw.AddRect(ImVec2(x + s * .1f, y + s * .1f), ImVec2(x + s * .68f, y + s * .73f), blue);
        draw.AddRectFilled(ImVec2(x + s * .34f, y + s * .3f), ImVec2(x + s * .91f, y + s * .96f),
                           IM_COL32(255, 255, 255, 255));
        draw.AddRect(ImVec2(x + s * .34f, y + s * .3f), ImVec2(x + s * .91f, y + s * .96f), blue);
        break;
    case 3: // select
        for (int i = 0; i < 4; ++i) {
            float t = s * (.1f + i * .22f);
            draw.AddLine(ImVec2(x + t, y + s * .15f), ImVec2(x + t + s * .1f, y + s * .15f), blue);
            draw.AddLine(ImVec2(x + t, y + s * .85f), ImVec2(x + t + s * .1f, y + s * .85f), blue);
            draw.AddLine(ImVec2(x + s * .1f, y + t), ImVec2(x + s * .1f, y + t + s * .1f), blue);
            draw.AddLine(ImVec2(x + s * .9f, y + t), ImVec2(x + s * .9f, y + t + s * .1f), blue);
        }
        break;
    case 4: // crop
        draw.AddLine(ImVec2(x + s * .3f, y), ImVec2(x + s * .3f, y + s * .75f), edge, 2);
        draw.AddLine(ImVec2(x, y + s * .25f), ImVec2(x + s * .8f, y + s * .25f), edge, 2);
        draw.AddLine(ImVec2(x + s * .3f, y + s * .75f), ImVec2(x + s, y + s * .75f), edge, 2);
        draw.AddLine(ImVec2(x + s * .8f, y + s * .25f), ImVec2(x + s * .8f, y + s), edge, 2);
        break;
    case 5: // resize
        draw.AddRect(ImVec2(x + s * .12f, y + s * .15f), ImVec2(x + s * .88f, y + s * .85f), blue);
        draw.AddRectFilled(ImVec2(x + s * .12f, y + s * .5f), ImVec2(x + s * .48f, y + s * .85f),
                           IM_COL32(188, 221, 245, 255));
        draw.AddLine(ImVec2(x + s * .4f, y + s * .6f), ImVec2(x + s * .85f, y + s * .15f), blue, 2);
        break;
    case 6: // rotate
        draw.AddRectFilled(ImVec2(x + s * .2f, y + s * .35f), ImVec2(x + s * .65f, y + s * .85f),
                           IM_COL32(183, 214, 236, 255));
        draw.PathArcTo(ImVec2(x + s * .5f, y + s * .45f), s * .33f, 3.4f, 6.6f, 14);
        draw.PathStroke(blue, 0, 2);
        draw.AddTriangleFilled(ImVec2(x + s * .98f, y + s * .45f), ImVec2(x + s * .67f, y + s * .42f),
                               ImVec2(x + s * .86f, y + s * .64f), blue);
        break;
    case 7: // pencil
        draw.AddLine(ImVec2(x + s * .2f, y + s * .8f), ImVec2(x + s * .82f, y + s * .18f), gold, s * .19f);
        draw.AddTriangleFilled(ImVec2(x + s * .08f, y + s * .94f), ImVec2(x + s * .14f, y + s * .68f),
                               ImVec2(x + s * .34f, y + s * .87f), edge);
        draw.AddLine(ImVec2(x + s * .76f, y + s * .24f), ImVec2(x + s * .87f, y + s * .13f),
                     IM_COL32(218, 121, 110, 255), s * .19f);
        break;
    case 8: // fill
        draw.AddQuadFilled(ImVec2(x + s * .18f, y + s * .4f), ImVec2(x + s * .48f, y + s * .13f),
                           ImVec2(x + s * .8f, y + s * .45f), ImVec2(x + s * .5f, y + s * .75f),
                           IM_COL32(169, 196, 216, 255));
        draw.AddQuad(ImVec2(x + s * .18f, y + s * .4f), ImVec2(x + s * .48f, y + s * .13f),
                     ImVec2(x + s * .8f, y + s * .45f), ImVec2(x + s * .5f, y + s * .75f), edge);
        draw.AddTriangleFilled(ImVec2(x + s * .81f, y + s * .55f), ImVec2(x + s * .68f, y + s * .93f),
                               ImVec2(x + s * .96f, y + s * .93f), blue);
        break;
    case 9:
        draw.AddText(ImVec2(x + s * .18f, y - s * .1f), edge, "A");
        break;
    case 10:
        draw.AddQuadFilled(ImVec2(x + s * .12f, y + s * .63f), ImVec2(x + s * .53f, y + s * .16f),
                           ImVec2(x + s * .91f, y + s * .47f), ImVec2(x + s * .48f, y + s * .94f),
                           IM_COL32(233, 147, 155, 255));
        draw.AddLine(ImVec2(x + s * .13f, y + s * .65f), ImVec2(x + s * .48f, y + s * .93f), edge, 2);
        break;
    case 11:
        draw.AddLine(ImVec2(x + s * .18f, y + s * .84f), ImVec2(x + s * .76f, y + s * .24f), blue, s * .16f);
        draw.AddLine(ImVec2(x + s * .62f, y + s * .08f), ImVec2(x + s * .92f, y + s * .38f), edge, s * .2f);
        break;
    case 12:
        draw.AddCircle(ImVec2(x + s * .4f, y + s * .4f), s * .28f, blue, 20, 2);
        draw.AddLine(ImVec2(x + s * .61f, y + s * .61f), ImVec2(x + s * .91f, y + s * .91f), edge, 3);
        break;
    case 13:
        draw.AddLine(ImVec2(x + s * .75f, y + s * .05f), ImVec2(x + s * .45f, y + s * .54f),
                     IM_COL32(160, 96, 48, 255), s * .17f);
        draw.AddQuadFilled(ImVec2(x + s * .25f, y + s * .48f), ImVec2(x + s * .56f, y + s * .62f),
                           ImVec2(x + s * .46f, y + s * .92f), ImVec2(x + s * .1f, y + s * .79f),
                           IM_COL32(41, 92, 157, 255));
        draw.AddLine(ImVec2(x + s * .27f, y + s * .49f), ImVec2(x + s * .55f, y + s * .61f),
                     IM_COL32(166, 183, 194, 255), s * .13f);
        break;
    case 14:
        draw.AddRectFilled(ImVec2(x + s * .08f, y + s * .05f), ImVec2(x + s * .92f, y + s * .95f),
                           IM_COL32(113, 132, 191, 255));
        draw.AddRectFilled(ImVec2(x + s * .25f, y + s * .06f), ImVec2(x + s * .71f, y + s * .4f),
                           IM_COL32(225, 231, 237, 255));
        draw.AddRectFilled(ImVec2(x + s * .24f, y + s * .58f), ImVec2(x + s * .77f, y + s * .93f),
                           IM_COL32(250, 250, 252, 255));
        break;
    case 15:
    case 16: {
        float direction = icon == 15 ? 1.0f : -1.0f;
        draw.PathArcTo(ImVec2(x + s * .5f, y + s * .52f), s * .32f, -2.6f, 1.0f, 16);
        draw.PathStroke(blue, 0, 2);
        float tip = x + (icon == 15 ? s * .08f : s * .92f);
        draw.AddTriangleFilled(ImVec2(tip, y + s * .16f), ImVec2(tip + direction * s * .38f, y + s * .1f),
                               ImVec2(tip + direction * s * .08f, y + s * .43f), blue);
        break;
    }
    case 17:
        draw.AddRectFilled(ImVec2(x + s * .2f, y + s * .1f), ImVec2(x + s * .8f, y + s * .94f),
                           IM_COL32(255, 255, 255, 255));
        draw.AddRect(ImVec2(x + s * .2f, y + s * .1f), ImVec2(x + s * .8f, y + s * .94f), blue);
        break;
    case 18:
        draw.AddRectFilled(ImVec2(x + s * .1f, y + s * .3f), ImVec2(x + s * .94f, y + s * .85f), gold, 2);
        draw.AddRectFilled(ImVec2(x + s * .15f, y + s * .15f), ImVec2(x + s * .5f, y + s * .4f), gold, 2);
        break;
    case 19:
        draw.AddRectFilled(ImVec2(x + s * .18f, y + s * .36f), ImVec2(x + s * .85f, y + s * .7f),
                           IM_COL32(176, 187, 197, 255), 2);
        draw.AddRectFilled(ImVec2(x + s * .28f, y + s * .65f), ImVec2(x + s * .73f, y + s * .97f),
                           IM_COL32(255, 255, 255, 255));
        draw.AddRect(ImVec2(x + s * .28f, y + s * .02f), ImVec2(x + s * .73f, y + s * .38f), blue);
        break;
    case 20:
        draw.AddCircleFilled(ImVec2(x + s * .5f, y + s * .3f), s * .2f, IM_COL32(157, 96, 58, 255));
        draw.AddRectFilled(ImVec2(x + s * .38f, y + s * .3f), ImVec2(x + s * .63f, y + s * .65f),
                           IM_COL32(157, 96, 58, 255));
        draw.AddRectFilled(ImVec2(x + s * .12f, y + s * .65f), ImVec2(x + s * .88f, y + s * .9f), edge);
        break;
    case 21:
        draw.AddLine(ImVec2(x + s * .1f, y + s * .8f), ImVec2(x + s * .45f, y + s * .12f), blue, 1.5f);
        draw.AddLine(ImVec2(x + s * .45f, y + s * .12f), ImVec2(x + s * .9f, y + s * .7f), blue, 1.5f);
        draw.AddLine(ImVec2(x + s * .9f, y + s * .7f), ImVec2(x + s * .1f, y + s * .8f), blue, 1.5f);
        draw.AddCircleFilled(ImVec2(x + s * .45f, y + s * .12f), 3, blue);
        draw.AddCircleFilled(ImVec2(x + s * .9f, y + s * .7f), 3, blue);
        break;
    case 22:
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                ImVec2 p(x + s * (.12f + .36f * col), y + s * (.12f + .36f * row));
                if (col < 2) {
                    draw.AddLine(p, ImVec2(p.x + s * .36f, p.y), blue);
                }
                if (row < 2) {
                    draw.AddLine(p, ImVec2(p.x, p.y + s * .36f), blue);
                }
                draw.AddCircleFilled(p, 2, blue);
            }
        }
        break;
    default:
        draw.AddText(ImVec2(x + 3, y + 1), color, "?");
        break;
    }
}
bool ribbon_button(const char* id, const char* label, int icon, ImVec2 position, ImVec2 size, bool selected,
                   const char* tooltip) {
    ImGui::SetCursorPos(position);
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##button", size);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    if (hovered || selected) {
        draw.AddRectFilled(a, b, selected ? IM_COL32(204, 228, 249, 255) : IM_COL32(229, 241, 252, 255));
        draw.AddRect(a, b, selected ? IM_COL32(125, 174, 217, 255) : IM_COL32(169, 204, 232, 255));
    }
    bool tall = size.y > 40;
    float icon_size = tall ? 28.0f : 16.0f;
    if (icon >= 0) {
        classic_icon(
            draw, icon,
            ImVec2(a.x + (tall ? (size.x - icon_size) / 2 : 4), a.y + (tall ? 5 : (size.y - icon_size) / 2)),
            icon_size);
    }
    if (label && label[0]) {
        ImVec2 text = ImGui::CalcTextSize(label);
        draw.AddText(ImVec2(a.x + (tall       ? (size.x - text.x) / 2
                                   : icon < 0 ? (size.x - text.x) / 2
                                              : 25),
                            a.y + (tall ? 38 : (size.y - text.y) / 2)),
                     IM_COL32(35, 49, 64, 255), label);
    }
    if (hovered && tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::PopID();
    return clicked;
}
static void group_label(float left, float right, const char* label) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImVec2 base = ImGui::GetWindowPos();
    draw.AddLine(ImVec2(base.x + right, base.y + 57), ImVec2(base.x + right, base.y + 148),
                 IM_COL32(210, 215, 221, 255));
    ImVec2 size = ImGui::CalcTextSize(label);
    draw.AddText(ImVec2(base.x + (left + right - size.x) / 2, base.y + 139), IM_COL32(88, 103, 121, 255),
                 label);
}
static bool command_menu(const char* name, const char* shortcut, Application& app, Command command,
                         bool enabled = true) {
    bool chosen = ImGui::MenuItem(name, shortcut, false, enabled);
    if (chosen) {
        app.command(command);
    }
    return chosen;
}
void Application::ribbon(float width) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImVec2 base = ImGui::GetWindowPos();
    draw.AddRectFilledMultiColor(ImVec2(base.x, base.y), ImVec2(base.x + width, base.y + 27),
                                 IM_COL32(225, 237, 250, 255), IM_COL32(239, 245, 252, 255),
                                 IM_COL32(214, 229, 245, 255), IM_COL32(213, 231, 249, 255));
    draw.AddRectFilled(ImVec2(base.x, base.y + 27), ImVec2(base.x + width, base.y + 52),
                       IM_COL32(246, 248, 250, 255));
    draw.AddRectFilledMultiColor(ImVec2(base.x, base.y + 52), ImVec2(base.x + width, base.y + 157),
                                 IM_COL32(250, 251, 252, 255), IM_COL32(250, 251, 252, 255),
                                 IM_COL32(230, 236, 244, 255), IM_COL32(230, 236, 244, 255));
    draw.AddLine(ImVec2(base.x, base.y + 157), ImVec2(base.x + width, base.y + 157),
                 IM_COL32(168, 184, 204, 255));
    classic_icon(draw, 13, ImVec2(base.x + 8, base.y + 4), 18);
    if (ribbon_button("Quick save", nullptr, 14, {36, 2}, {24, 23}, false, "Save (Ctrl+S / Command+S)")) {
        command(Command::Save);
    }
    if (ribbon_button("Quick undo", nullptr, 15, {63, 2}, {24, 23}, false, "Undo (Ctrl+Z / Command+Z)")) {
        command(Command::Undo);
    }
    if (ribbon_button("Quick redo", nullptr, 16, {89, 2}, {24, 23}, false,
                      "Redo (Ctrl+Y / Command+Shift+Z)")) {
        command(Command::Redo);
    }
    draw.AddText(ImVec2(base.x + 135, base.y + 7), IM_COL32(32, 54, 76, 255), "Rainstar Paint");
    draw.AddRectFilled(ImVec2(base.x, base.y + 27), ImVec2(base.x + 56, base.y + 51),
                       IM_COL32(40, 116, 183, 255));
    ImGui::SetCursorPos({0, 27});
    if (ImGui::InvisibleButton("File tab", {56, 24})) {
        ImGui::OpenPopup("File");
    }
    draw.AddText(ImVec2(base.x + 16, base.y + 32), IM_COL32(255, 255, 255, 255), "File");
    if (ribbon_button("Home tab", "Home", -1, {59, 27}, {58, 25}, !view_tab && !text_tab)) {
        view_tab = false;
        text_tab = false;
    }
    if (ribbon_button("View tab", "View", -1, {118, 27}, {56, 25}, view_tab)) {
        view_tab = true;
        text_tab = false;
    }
    if (text_active && ribbon_button("Text tab", "Text", -1, {175, 27}, {56, 25}, text_tab)) {
        text_tab = true;
        view_tab = false;
    }
    if (ribbon_button("Extras menu", "Patterns & tools", -1, {text_active ? 235.0f : 183.0f, 27}, {112, 25},
                      false, "Pattern fills, continuous paths, rubber stamps and reshape")) {
        ImGui::OpenPopup("Extra tools");
    }
    if (ribbon_button("Help", "?", -1, {width - 29, 28}, {25, 23}, show_help, "Help (F1)")) {
        show_help = !show_help;
    }
    if (ImGui::BeginPopup("File")) {
        GuiScope popup_scope(GuiEnd::Popup);
        command_menu("New", "Ctrl+N", *this, Command::New);
        command_menu("Open...", "Ctrl+O", *this, Command::Open);
        command_menu("Save", "Ctrl+S", *this, Command::Save);
        command_menu("Save as...", "F12", *this, Command::SaveAs);
        ImGui::Separator();
        command_menu("Print...", "Ctrl+P", *this, Command::Print);
        command_menu("Page setup...", nullptr, *this, Command::PageSetup);
        command_menu("Properties", "Ctrl+E", *this, Command::Properties);
        ImGui::Separator();
        command_menu("About Rainstar Paint", nullptr, *this, Command::About);
        command_menu("Exit", nullptr, *this, Command::Quit);
    }
    if (ImGui::BeginPopup("Extra tools")) {
        const double minimum_mesh_spacing = 20.0, maximum_mesh_spacing = 140.0;
        GuiScope popup_scope(GuiEnd::Popup);
        if (ImGui::MenuItem("Continuous junction path", nullptr,
                            document.tool == Tool::Path && document.continuous_path)) {
            choose_tool(Tool::Path);
            document.continuous_path = true;
            status = "Click points. Blue dots snap to old junctions. Escape finishes.";
        }
        if (ImGui::MenuItem("Rubber stamp", nullptr, document.tool == Tool::Stamp)) {
            choose_tool(Tool::Stamp);
        }
        if (ImGui::MenuItem("Reshape selected object", nullptr, document.tool == Tool::Reshape,
                            document.selection.active)) {
            choose_tool(Tool::Reshape);
        }
        ImGui::SliderScalar("Mesh spacing", ImGuiDataType_Double, &mesh_spacing, &minimum_mesh_spacing,
                            &maximum_mesh_spacing, "%.0f px");
        ImGui::Separator();
        int pattern = static_cast<int>(document.ink.pattern);
        if (ImGui::Combo("Fill / brush pattern", &pattern, pattern_names, 18)) {
            document.ink.pattern = static_cast<Pattern>(pattern);
        }
        ImGui::Checkbox("Transparent second pattern color", &document.ink.transparent_pattern);
        ImGui::Separator();
        int stamp = static_cast<int>(document.stamp_shape);
        const char* stamp_names[] = {"Circle", "Pill", "Square", "Rectangle"};
        if (ImGui::Combo("Stamp outline", &stamp, stamp_names, 4)) {
            document.stamp_shape = static_cast<StampShape>(stamp);
        }
        ImGui::SliderInt("Stamp width", &stamp_width, 4, 500);
        if (document.stamp_shape == StampShape::Pill || document.stamp_shape == StampShape::Rectangle) {
            ImGui::SliderInt("Stamp height", &stamp_height, 4, 500);
        }
        ImGui::Checkbox("Transparent stamp (skip Color 2)", &document.stamp_transparent);
        if (ImGui::Button("Lift a new stamp") && !warp_worker.busy()) {
            document.stamp = {};
            stamp_field.reset();
            stamp_render_pending = false;
            choose_tool(Tool::Stamp);
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextDisabled("Click to lift; click again to repeat. R rotates; +/- scales.");
    }
    if (view_tab) {
        if (ribbon_button("Zoom in", "Zoom in", 12, {8, 57}, {65, 70})) {
            zoom = std::min(8.0f, zoom * 2);
        }
        if (ribbon_button("Zoom out", "Zoom out", 12, {76, 57}, {65, 70})) {
            zoom = std::max(0.125f, zoom / 2);
        }
        if (ribbon_button("100 percent", "100%", 5, {144, 57}, {65, 70})) {
            zoom = 1;
        }
        group_label(0, 220, "Zoom");
        ImGui::SetCursorPos({232, 61});
        ImGui::Checkbox("Rulers", &show_rulers);
        ImGui::SetCursorPos({232, 86});
        ImGui::Checkbox("Gridlines", &show_grid);
        ImGui::SetCursorPos({232, 111});
        ImGui::Checkbox("Status bar", &show_status);
        group_label(220, 366, "Show or hide");
        if (ribbon_button("Full screen", "Full screen", 5, {376, 57}, {82, 70})) {
            full_screen = !full_screen;
            SDL_SetWindowFullscreen(window, full_screen);
        }
        if (ribbon_button("Fit view", "Fit window", 5, {461, 57}, {85, 70})) {
            ImVec2 size = ImGui::GetIO().DisplaySize;
            zoom = std::min((size.x - 30) / document.image.width, (size.y - 205) / document.image.height);
        }
        group_label(366, 557, "Display");
        return;
    }
    if (text_tab && text_active) {
        ImGui::SetCursorPos({12, 63});
        const char* fonts[] = {"Arial / Sans serif", "Courier / Monospace"};
        int font = text_style.mono ? 1 : 0;
        ImGui::SetNextItemWidth(185);
        if (ImGui::Combo("##Font", &font, fonts, 2)) {
            text_style.mono = font == 1;
        }
        ImGui::SetCursorPos({12, 96});
        ImGui::SetNextItemWidth(90);
        ImGui::InputInt("Size", &text_style.size);
        text_style.size = std::clamp(text_style.size, 6, 300);
        ImGui::SetCursorPos({215, 64});
        ImGui::Checkbox("Bold", &text_style.bold);
        ImGui::SameLine();
        ImGui::Checkbox("Italic", &text_style.italic);
        ImGui::SetCursorPos({215, 94});
        ImGui::Checkbox("Underline", &text_style.underline);
        ImGui::SameLine();
        ImGui::Checkbox("Strikeout", &text_style.strikeout);
        group_label(0, 427, "Font");
        ImGui::SetCursorPos({438, 64});
        ImGui::Checkbox("Opaque background", &text_style.opaque);
        ImGui::SetCursorPos({438, 95});
        if (ImGui::Button("Place text")) {
            finish_text();
        }
        group_label(427, 635, "Background");
        if (ribbon_button("Text color", "Edit colors", 8, {650, 57}, {80, 70})) {
            begin_color();
        }
        return;
    }
    if (ribbon_button("Paste", "Paste", 0, {5, 57}, {48, 68}, false, "Paste an image from the clipboard")) {
        command(Command::Paste);
    }
    if (ribbon_button("Paste dropdown", "v", -1, {7, 126}, {43, 13})) {
        ImGui::OpenPopup("Paste choices");
    }
    if (ImGui::BeginPopup("Paste choices")) {
        GuiScope popup_scope(GuiEnd::Popup);
        command_menu("Paste", "Ctrl+V", *this, Command::Paste);
        command_menu("Paste from...", nullptr, *this, Command::PasteFrom);
    }
    if (ribbon_button("Cut", "Cut", 1, {55, 60}, {50, 24})) {
        command(Command::Cut);
    }
    if (ribbon_button("Copy", "Copy", 2, {55, 86}, {50, 24})) {
        command(Command::Copy);
    }
    group_label(0, 111, "Clipboard");
    if (ribbon_button("Select", "Select", 3, {116, 57}, {48, 67},
                      document.tool == Tool::Select || document.tool == Tool::Lasso)) {
        choose_tool(Tool::Select);
    }
    if (ribbon_button("Selection menu", "v", -1, {118, 126}, {44, 13})) {
        ImGui::OpenPopup("Selection");
    }
    if (ImGui::BeginPopup("Selection")) {
        GuiScope popup_scope(GuiEnd::Popup);
        if (ImGui::MenuItem("Rectangular selection", nullptr, document.tool == Tool::Select)) {
            choose_tool(Tool::Select);
        }
        if (ImGui::MenuItem("Free-form selection", nullptr, document.tool == Tool::Lasso)) {
            choose_tool(Tool::Lasso);
        }
        ImGui::Separator();
        command_menu("Select all", "Ctrl+A", *this, Command::SelectAll);
        command_menu("Invert selection", nullptr, *this, Command::InvertSelection);
        command_menu("Delete", "Del", *this, Command::Delete);
        ImGui::MenuItem("Transparent selection", nullptr, &document.transparent_selection);
    }
    if (ribbon_button("Crop", "Crop", 4, {171, 58}, {72, 23})) {
        command(Command::Crop);
    }
    if (ribbon_button("Resize", "Resize", 5, {171, 84}, {72, 23})) {
        command(Command::Resize);
    }
    if (ribbon_button("Rotate", "Rotate v", 6, {171, 110}, {76, 23})) {
        ImGui::OpenPopup("Rotate");
    }
    if (ImGui::BeginPopup("Rotate")) {
        GuiScope popup_scope(GuiEnd::Popup);
        command_menu("Rotate right 90 degrees", nullptr, *this, Command::RotateRight);
        command_menu("Rotate left 90 degrees", nullptr, *this, Command::RotateLeft);
        command_menu("Rotate 180 degrees", nullptr, *this, Command::Rotate180);
        ImGui::Separator();
        command_menu("Flip vertical", nullptr, *this, Command::FlipVertical);
        command_menu("Flip horizontal", nullptr, *this, Command::FlipHorizontal);
    }
    group_label(111, 253, "Image");
    const Tool tools[6] = {Tool::Pencil, Tool::Fill, Tool::Text, Tool::Eraser, Tool::Picker, Tool::Magnifier};
    const char* names[6] = {"Pencil", "Fill with color", "Text", "Eraser", "Color picker", "Magnifier"};
    for (int i = 0; i < 6; ++i) {
        if (ribbon_button(names[i], nullptr, 7 + i, {261.0f + (i % 3) * 25.0f, 63.0f + (i / 3) * 28.0f},
                          {24, 25}, document.tool == tools[i], names[i])) {
            choose_tool(tools[i]);
        }
    }
    group_label(253, 341, "Tools");
    if (ribbon_button("Brushes", "Brushes", 13, {346, 57}, {58, 68}, document.tool == Tool::Brush)) {
        choose_tool(Tool::Brush);
        ImGui::OpenPopup("Brushes");
    }
    if (ImGui::BeginPopup("Brushes")) {
        GuiScope popup_scope(GuiEnd::Popup);
        for (int i = 0; i < 9; ++i) {
            if (ImGui::MenuItem(brush_names[i], nullptr, static_cast<int>(document.ink.brush) == i)) {
                document.ink.brush = static_cast<Brush>(i);
                choose_tool(Tool::Brush);
            }
        }
    }
    group_label(341, 410, "");
    draw.AddRectFilled(ImVec2(base.x + 418, base.y + 60), ImVec2(base.x + 597, base.y + 132),
                       IM_COL32(255, 255, 255, 255));
    draw.AddRect(ImVec2(base.x + 418, base.y + 60), ImVec2(base.x + 597, base.y + 132),
                 IM_COL32(177, 186, 198, 255));
    for (int i = 0; i < 21; ++i) {
        if (ribbon_button(shape_names[i], nullptr, 100 + i,
                          {419.0f + (i % 7) * 25.0f, 61.0f + (i / 7) * 23.0f}, {25, 23},
                          document.tool == Tool::Shape && static_cast<int>(document.shape) == i,
                          shape_names[i])) {
            choose_tool(i == 5 ? Tool::Path : Tool::Shape);
            document.continuous_path = false;
            document.shape = static_cast<Shape>(i);
        }
    }
    if (ribbon_button("More shapes", "v", -1, {596, 60}, {15, 72})) {
        ImGui::OpenPopup("Shape gallery");
    }
    if (ImGui::BeginPopup("Shape gallery")) {
        GuiScope popup_scope(GuiEnd::Popup);
        for (int i = 0; i < 23; ++i) {
            if (ImGui::MenuItem(shape_names[i])) {
                choose_tool(i == 5 ? Tool::Path : Tool::Shape);
                document.continuous_path = false;
                document.shape = static_cast<Shape>(i);
            }
        }
    }
    if (ribbon_button("Outline", "Outline v", -1, {614, 61}, {78, 25})) {
        ImGui::OpenPopup("Outline style");
    }
    if (ImGui::BeginPopup("Outline style")) {
        GuiScope popup_scope(GuiEnd::Popup);
        if (ImGui::MenuItem("No outline", nullptr, !document.shape_outline)) {
            document.shape_outline = false;
        }
        if (ImGui::MenuItem("Solid color", nullptr, document.shape_outline)) {
            document.shape_outline = true;
            document.ink.brush = Brush::Round;
        }
        for (int i = 4; i < 9; ++i) {
            if (ImGui::MenuItem(brush_names[i])) {
                document.shape_outline = true;
                document.ink.brush = static_cast<Brush>(i);
            }
        }
    }
    if (ribbon_button("Fill", "Fill v", -1, {614, 88}, {78, 25})) {
        ImGui::OpenPopup("Fill style");
    }
    if (ImGui::BeginPopup("Fill style")) {
        GuiScope popup_scope(GuiEnd::Popup);
        if (ImGui::MenuItem("No fill", nullptr, !document.shape_fill)) {
            document.shape_fill = false;
        }
        if (ImGui::MenuItem("Solid color", nullptr,
                            document.shape_fill && document.ink.pattern == Pattern::Solid)) {
            document.shape_fill = true;
            document.ink.pattern = Pattern::Solid;
        }
        for (int i = 1; i < 18; ++i) {
            if (ImGui::MenuItem(pattern_names[i], nullptr,
                                document.shape_fill && static_cast<int>(document.ink.pattern) == i)) {
                document.shape_fill = true;
                document.ink.pattern = static_cast<Pattern>(i);
            }
        }
    }
    group_label(410, 701, "Shapes");
    if (ribbon_button("Size", "Size", -1, {708, 57}, {49, 76})) {
        ImGui::OpenPopup("Stroke size");
    }
    for (int i = 0; i < 4; ++i) {
        draw.AddLine(ImVec2(base.x + 717, base.y + 65 + i * 8), ImVec2(base.x + 748, base.y + 65 + i * 8),
                     IM_COL32(72, 99, 130, 255), static_cast<float>(i + 1));
    }
    if (ImGui::BeginPopup("Stroke size")) {
        GuiScope popup_scope(GuiEnd::Popup);
        const int sizes[] = {1, 3, 5, 8, 12, 20, 32, 48};
        for (int size : sizes) {
            char label[32] = {};
            std::snprintf(label, sizeof(label), "%d px", size);
            if (ImGui::MenuItem(label, nullptr, document.ink.size == size)) {
                document.ink.size = size;
            }
        }
        ImGui::SliderInt("Custom", &document.ink.size, 1, 100);
    }
    group_label(701, 765, "");
    for (int slot = 0; slot < 2; ++slot) {
        float x = 771.0f + slot * 47;
        if (ribbon_button(slot == 0 ? "Color1" : "Color2", slot == 0 ? "Color 1" : "Color 2", -1, {x, 57},
                          {44, 75}, primary_slot == (slot == 0))) {
            primary_slot = slot == 0;
        }
        Color color = slot == 0 ? document.ink.primary : document.ink.secondary;
        draw.AddRectFilled(ImVec2(base.x + x + 8, base.y + 64), ImVec2(base.x + x + 36, base.y + 93),
                           packed(color));
        draw.AddRect(ImVec2(base.x + x + 7, base.y + 63), ImVec2(base.x + x + 37, base.y + 94),
                     IM_COL32(128, 143, 160, 255));
    }
    // Office 2007/2010 theme accents and their light tints, in the classic Paint grid.
    const unsigned colors[30] = {
        0x000000, 0xFFFFFF, 0x1F497D, 0xEEECE1, 0x4F81BD, 0xC0504D, 0x9BBB59, 0x8064A2, 0x4BACC6, 0xF79646,
        0x7F7F7F, 0xF2F2F2, 0xC6D9F1, 0xDDD9C3, 0xDBE5F1, 0xF2DCDB, 0xEBF1DE, 0xE4DFEC, 0xDBEEF3, 0xFDE9D9,
        0xBFBFBF, 0xD9D9D9, 0x8DB3E2, 0xC4BD97, 0xB8CCE4, 0xE5B9B7, 0xD7E3BC, 0xCCC1D9, 0xB7DEE8, 0xFBD5B5};
    for (int i = 0; i < 30; ++i) {
        unsigned value = colors[i];
        Color color{static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
                    static_cast<std::uint8_t>(value), 255};
        ImVec2 pos(871.0f + (i % 10) * 21.0f, 60.0f + (i / 10) * 23.0f);
        ImGui::SetCursorPos(pos);
        ImGui::PushID(i);
        ImGui::InvisibleButton("Office color", {19, 21},
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if (ImGui::IsItemClicked(0)) {
            if (primary_slot) {
                document.ink.primary = color;
            } else {
                document.ink.secondary = color;
            }
        }
        if (ImGui::IsItemClicked(1)) {
            document.ink.secondary = color;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nLeft: selected color. Right: Color 2.", to_hex(color).c_str());
        }
        draw.AddRectFilled(ImVec2(base.x + pos.x + 2, base.y + pos.y + 2),
                           ImVec2(base.x + pos.x + 17, base.y + pos.y + 19), packed(color));
        draw.AddRect(ImVec2(base.x + pos.x + 1, base.y + pos.y + 1),
                     ImVec2(base.x + pos.x + 18, base.y + pos.y + 20), IM_COL32(160, 170, 181, 255));
        ImGui::PopID();
    }
    if (ribbon_button("Edit colors", "Edit\ncolors", -1, {1085, 57}, {48, 76}, false,
                      "Edit RGB, OKLab and hexadecimal colors")) {
        begin_color();
    }
    for (int x = 0; x < 20; ++x) {
        float hue = x / 20.0f;
        float r = 0, g = 0, b = 0;
        ImGui::ColorConvertHSVtoRGB(hue, 0.85f, 0.95f, r, g, b);
        ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
        draw.AddRectFilled(ImVec2(base.x + 1098 + x, base.y + 63), ImVec2(base.x + 1099 + x, base.y + 89),
                           color);
    }
    group_label(765, 1138, "Colors");
    if (width > 1210) {
        if (ribbon_button("Stamp shortcut", "Stamp", 20, {1145, 58}, {58, 66},
                          document.tool == Tool::Stamp)) {
            choose_tool(Tool::Stamp);
        }
        if (ribbon_button("Path shortcut", "Path", 21, {1206, 58}, {58, 66},
                          document.tool == Tool::Path && document.continuous_path)) {
            choose_tool(Tool::Path);
            document.continuous_path = true;
        }
        group_label(1138, width - 3, "Extra tools");
    }
}
} // namespace paint
