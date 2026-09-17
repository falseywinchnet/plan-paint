#include "application.hpp"
#include "gui_scope.hpp"
namespace paint {
static void help_topic(const char* title, const char* body, bool open = false) {
    if (ImGui::CollapsingHeader(title, open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None)) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(body);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }
}
void Application::help(float x, float y, float width, float height) {
    ImGui::SetCursorPos({x, y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 255, 211, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(238, 234, 168, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(230, 225, 150, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::BeginChild("Rainstar Paint Help", {width, height}, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
    GuiScope help_scope(GuiEnd::Child, 1, 4);
    ImGui::TextUnformatted("Rainstar Paint Help");
    ImGui::Separator();
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted("Hello! A picture can begin with one little mark. This guide shows you where to "
                           "click and how to try again. Press F1 to hide or show this book.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    help_topic("Atlas: sprite sheets, icons and cursors",
               "Open a sprite sheet and choose Rows and Columns. Filenames containing sprite, sheet, atlas, "
               "tileset, walk or idle offer this setup automatically; use Atlas > Set up grid for any other "
               "image. Margins and spacing handle padded sheets.\n\nClick a sprite in the ribbon to edit it. "
               "Left / Right steps through frames, even after returning to Home to paint. Ctrl-click several "
               "sprites to hold a sequence. Expand gallery shows the original rows and columns so a vertical "
               "sequence is easy to select. All frames restores the full sequence. Save writes the complete "
               "sheet. Each sprite is one flat canvas; no layers or animation container are created.\n\nICO "
               "and CUR open every stored size. CUR hotspots can be entered as X/Y pixels or chosen with "
               "Pick on canvas. Save preserves all entries. Save As from a picture to ICO or CUR offers "
               "standard sizes. Save to PNG or another ordinary image format exports the current icon "
               "image.\n\nSVG is rasterized on import. AVIF and macOS HEIC/HEIF also import to the canvas; "
               "Save chooses a supported raster output. Animated GIF and APNG are not supported.");
    help_topic("1. Make your first picture",
               "Click Pencil in Home, then hold the mouse button and move it on the white paper. Let go to "
               "stop. Try Brushes for wider marks. Pick a little colored square to change your "
               "color.\n\nMade a mistake? Click the curved Undo arrow at the top. You can also hold Ctrl and "
               "press Z. On a Mac, Command+Z works too. Redo brings a change back.",
               true);
    help_topic(
        "2. Colors, brushes and patterns",
        "Color 1 is the color you draw with. Color 2 is the background color. Click either box, then click a "
        "swatch. Right-clicking a swatch always picks Color 2. Drawing with the right mouse button swaps the "
        "colors.\n\nThe swatches use the classic Microsoft Office theme and its pastel tints. Edit colors "
        "opens a big color chooser. You can type RGB values, a six-digit hex code such as #4F81BD, or OKLab "
        "L, a and b. L is lightness from 0 to 1. The other two numbers steer the hue. Colors outside the "
        "screen's range are clipped to sRGB. The sixteen Custom colors boxes remember colors after Paint "
        "closes. Click a slot, choose your color, then Add to custom colors. Click a saved box to use it "
        "again. Original restores the color you started with; OK applies your choice and Cancel leaves "
        "the drawing color alone.\n\nBrushes includes round, two calligraphy nibs, airbrush, oil, "
        "crayon, marker, natural pencil, watercolor, bristles, soft pastel and charcoal. Size changes the "
        "width. The Patterns & tools ribbon lets "
        "brushes and the bucket use two-color dots, stripes, checks, bricks, woven cloth, houndstooth and "
        "seven dither densities. Transparent second pattern color leaves those spaces untouched.");
    help_topic(
        "3. Fill, erase, pick and magnify",
        "The bucket fills the connected patch you click. A closed outline keeps the color inside; even a "
        "tiny gap lets it reach the outside. Undo if it went farther than you wanted. Patterns use the same "
        "region, with Color 1 and Color 2 making the design.\n\nClick the eraser icon to choose Hard or Soft "
        "and set its diameter. Its round pink preview shows the area it will touch. Hard clears every "
        "touched pixel to transparency. Soft erases most strongly under the center and tapers toward the "
        "edge. "
        "Undo restores erased marks. The eyedropper picks a color from the picture.\n\nThe magnifying glass "
        "shows an enlarged view under the pointer. Click to zoom in around that point; right-click zooms "
        "out. "
        "The bottom slider goes up to 1600%. The pencil previews the exact pixel under its tip before you "
        "click. "
        "Hover previews and zoom do not change your saved picture.");
    help_topic("4. Lines, curves and shapes",
               "Choose a shape in the little gallery. Hold the mouse button where it should start, drag, "
               "then release. Outline uses Color 1; Fill uses Color 2. Their menus let you turn either part "
               "off. Size sets outline thickness. Hold Shift for constrained lines and equal-sided "
               "shapes.\n\nA curve begins as a dragged line. Click once to bend its first control point, "
               "then again for its second control point. Escape finishes the curve early. A polygon is made "
               "by clicking corners; clicking the first corner closes it. Escape also finishes it.");
    help_topic(
        "5. The continuous junction path",
        "Choose Path in Patterns & tools or at the right of Home. Click to "
        "place each corner. Move near any old corner: a blue dot appears. Click that dot to snap your new "
        "segment exactly onto the old junction. You can keep drawing from there, including through "
        "loops.\n\nRight-click ends the current run without dropping its junctions. Click an old junction "
        "to begin a branch, or elsewhere to start a separate run. Undo removes one node and its segment "
        "at a time; Redo restores them. The remaining junctions stay available. Saving keeps them too. "
        "Escape or choosing another tool releases all junctions; the drawing remains, and its segments "
        "can still be undone individually.\n\nReturning to the first point does not finish the continuous "
        "path. The ordinary polygon closes when you return to the first corner of its current run.");
    help_topic(
        "6. Select, move, copy and paste",
        "Select draws a rectangle around part of the picture. The little arrow beneath Select also offers "
        "Free-form selection: draw a lasso around an object. Drag inside the selected area to move it. Arrow "
        "keys nudge it one pixel; Shift plus an arrow moves ten pixels. Escape places it.\n\nCopy keeps the "
        "original. Cut removes it. Paste makes a new movable selection. Paste from opens an image file as a "
        "selection. Drag an image file from Finder or your file manager onto this window to paste it "
        "immediately. A larger pasted picture expands the canvas.\n\nTransparent selection skips pixels "
        "exactly equal to Color 2. Crop keeps only the selected rectangle. Delete removes selected content. "
        "Without a selection, Delete clears the canvas to Color 2.");
    help_topic(
        "7. The rubber stamp",
        "Choose Stamp in Patterns & tools or at the right of Home. Pick a circle, pill, square or rectangle "
        "and choose its width and height. The outline under the pointer shows what you will lift. Click once "
        "over a part of the picture to load the stamp. The original stays on the page.\n\nNow click in other "
        "places to print repeated copies. R turns the stamp 15 degrees clockwise; Shift+R turns it the other "
        "way. + makes it bigger and - makes it smaller. These keys change the stamp, not the "
        "canvas.\n\nTransparent stamp preserves transparent pixels and skips pixels matching Color 2. Turn "
        "it off to include that background color. The outside of a circle or pill is always transparent. "
        "Right-click or press Escape to clear the old sample and reset its rotation and scale, even while "
        "it is preparing. The next canvas click picks another. Stamp > Lift a new stamp does the same; "
        "these controls are also in Patterns & tools.");
    help_topic(
        "8. Resize, rotate and the canvas",
        "Resize accepts percentages or pixel dimensions. Keep Maintain aspect ratio checked to avoid "
        "stretching. Scale artwork uses CONV* to reconstruct the image; turn it off to change only "
        "the canvas boundary. Smaller boundaries crop. Larger ones add Color 2 around the existing "
        "picture.\n\nThe round handle just outside a selection's upper-right corner turns it freely. "
        "Drag it around the object; hold Shift for 15-degree steps. Release to finish the rotation, "
        "then press Escape when you want to place the selection. Rotate also lets you enter any angle, "
        "with positive numbers turning clockwise. CONV prepares the original material once and reuses "
        "it while you drag. Large selections may take a moment to prepare.\n\nThe same menu keeps exact "
        "quarter turns, a half turn, and horizontal or vertical flips. With a selection, only that "
        "content changes. Without one, the whole picture rotates into an expanded canvas. "
        "Resize also has horizontal and vertical Skew angles. Drag the little handles around a selection "
        "to resize its pixels, or the three handles on the canvas edge to change the paper boundary. "
        "Properties shows dimensions and lets you change the canvas size and JPEG quality.");
    help_topic("9. Reshape an object like soft cloth",
               "Choose Free-form selection from the Select arrow. Draw all the way around your object. "
               "Then choose Patterns & tools > Mesh. Paint puts a coarse mesh of blue "
               "knobs around the edge and through the inside. Smaller Mesh spacing gives you more knobs. "
               "Drag a knob to stretch the object. A red warning means that move would fold the mesh, "
               "so the last safe shape stays in place. Press Escape to finish, or Undo to restore the "
               "original object.\n\nThe first preparation can take a few seconds. You may move knobs "
               "while it prepares. Drag previews use point samples; finishing applies an antialiasing filter "
               "where the mesh compresses the image. A source can contain at most one million pixels. Select "
               "a smaller "
               "object or resize it first if Paint explains that it is too large.");
    help_topic("10. Put words in your picture",
               "Click the A tool, then click the canvas. Type into the box. The Text tab lets you choose a "
               "font face, size, bold, italic, underline, "
               "strikeout, and an opaque "
               "background. Drag Move text to reposition the box and drag its edge handles to resize it. "
               "Word wrap moves whole words onto the next line; switch it off for explicit line breaks only. "
               "Color 1 colors the letters; Color 2 colors an opaque background. Place text "
               "stamps the words into the picture. Cancel text discards them. Undo removes placed text if "
               "you change your mind. Ctrl+Enter places text; Escape cancels it.");
    help_topic(
        "11. Open, save and print",
        "File > Open replaces the picture with an image from disk. Save writes your picture. Save as lets "
        "you choose a new name or format. PNG, TIFF, TGA and lossless WebP can preserve transparency. JPEG "
        "is smaller for photographs but loses detail each time it is re-encoded. BMP, JPEG and GIF use a "
        "white background for transparent areas. GIF uses a limited palette.\n\nYou can open PNG, JPEG, BMP, "
        "static GIF, TIFF, TGA, WebP, PSD composite images, PNM, HDR, PIC, SVG, AVIF, ICO and CUR, plus "
        "HEIC/HEIF on macOS. Animated GIF and APNG are not supported. "
        "Saving exports flat raster artwork; ICO and CUR retain their multiple images.\n\nThe application "
        "asks before replacing "
        "unsaved work. Cancel keeps you here. Printing uses the system's print dialog where available. Keep "
        "a saved copy of pictures you care about.\n\nRecent pictures remembers your last twelve opened or "
        "saved files. From scanner or camera uses Windows Image Acquisition on Windows. On a Mac it opens "
        "Image Capture; on Linux it opens Document Scanner or XSane. In those capture applications, save "
        "the picture, then drag it into Paint or choose Paste from. A connected device and its driver "
        "are needed.\n\nSend in email opens a mail draft with a copy of the picture attached. You choose "
        "the recipient and send it in your mail application. Set as desktop background offers Fill "
        "(cover the screen), Tile (repeat), and Center (keep the picture's size). These commands keep "
        "their exported copies in Paint's private preferences folder so the other application can "
        "continue using them.");
    help_topic(
        "12. View and keyboard guide",
        "View has Zoom in, Zoom out, 100%, rulers, gridlines, the status bar, Full screen and Fit window. "
        "Pixel gridlines appear at 400% zoom and above. Ctrl or Command plus the mouse wheel changes zoom. "
        "F11 toggles full screen.\n\nCtrl / Command shortcuts:\nN: New    O: Open    S: Save\nShift+S or "
        "F12: Save as\nZ: Undo    Y or Shift+Z: Redo\nX: Cut    C: Copy    V: Paste\nA: Select all    P: "
        "Print\nE: Properties    W: Resize    I: Invert colors\nG: Gridlines\n\nEscape places a selection or "
        "releases a path, clears a loaded stamp, or cancels text. F1 opens this guide. R and +/- change "
        "a loaded stamp.");
    help_topic(
        "13. How I made this - Astra",
        "I am Astra. I built Rainstar Paint in C++ with Dear ImGui and SDL, using explicit types, "
        "named operations and callbacks, clear ownership, and inspectable "
        "numerical loops. I chose the Windows 7/10 Paint ribbon as the visual and behavioral reference.\n\nI "
        "keep the document as a contiguous RGBA image. Editing gestures create undo checkpoints, and a "
        "floating selection owns its pixels until it is placed. Flood fill finds the region before painting "
        "it, so patterned fills can safely reuse the original colors.\n\nFor scaling, I ported the website "
        "CONV* demonstrator to native C++ using double precision, precomputed sampling plans and reusable "
        "worker storage. Enlargements interpolate the nodal reconstruction. Reductions integrate its quintic "
        "pieces over destination basins. Alpha is premultiplied during resampling to avoid colored fringes. "
        "Lines shorter than five pixels use an explicit short-line interpolation rule because the five-point "
        "stencil does not exist there.\n\nFor free deformation I compiled the paper's joint CONV field "
        "into a shared quintic control atlas, then built a coarse triangle mesh for the geometry. "
        "The compact beta-star "
        "variant supplies the material. Color is constrained to the available opacity before averaging. "
        "A bounded antialiasing filter grows smoothly where a transform compresses the image. Preparation "
        "runs on a background worker, and dragging reuses the compiled material.\n\nI use stb, gif-h, "
        "libtiff and libwebp for file encoding and "
        "decoding. Dear ImGui and the image libraries retain their own permissive license notices. This is "
        "an independent implementation; it does not contain Microsoft's Paint code or artwork.");
    help_topic(
        "14. Free for everyone",
        "To the Holy One, blessed be He, from whom all good things come. This work is dedicated in "
        "gratitude for the nourishment that sustains human life, the energy that powers our tools, "
        "and the opportunity to weave information into works of use and beauty.\n\n"
        "Author: Astra\nSponsor: Rainstar\nCopyright (c) 2026 "
        "joshuah.rainstar@gmail.com\n\nRainstar Paint is free and open source under the MIT license. Anyone "
        "may use it, learn from it, change it, and share it, including for commercial work. Keep the "
        "copyright and license notice with copies. The license does not promise a warranty.\n\nYour pictures "
        "are yours. There is no account, subscription, advertising, or upload service in the drawing "
        "workflow. The complete source and dependency notices are supplied with the project.");
}
} // namespace paint
