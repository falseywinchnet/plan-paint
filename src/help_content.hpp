#pragma once
#include <string_view>
namespace paint {
struct HelpTopic {
    std::string_view title, body;
    bool open = false;
};
inline constexpr std::string_view help_welcome =
    "Hello! A picture can begin with one little mark. This guide shows you where to "
    "click and how to try again. Close this book with the × at its upper left, or click the yellow ? button. "
    "F1 also hides or shows it.";
inline constexpr HelpTopic help_topics[] = {
    {"Atlas: sprite sheets, icons and cursors",
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
     "Save chooses a supported raster output. GIF is export-only; animated GIF and APNG are not supported."},
    {"1. Make your first picture",
     "Click Pencil in Home, then hold the mouse button and move it on the white paper. Let go to "
     "stop. Try Brushes for wider marks. Pick a little colored square to change your "
     "color.\n\nMade a mistake? Click the curved Undo arrow at the top. You can also hold Ctrl and "
     "press Z. On a Mac, Command+Z works too. Redo brings a change back.\n\n"
     "Click the selected ribbon tab to collapse it; click again or choose another tab to reopen. "
     "Wheel and trackpad scrolling stop when the viewport center reaches a canvas edge. "
     "File > Settings saves a scroll-distance multiplier in screen pixels, adjusted for zoom.",
     true},
    {"2. Colors, brushes and patterns",
     "Primary and Alt each have their own color, brush and pattern. Select either box, then choose a "
     "color or use the Materials pane to choose its brush and pattern together. Right-click a color, "
     "brush or pattern to assign it to Alt directly. The two color boxes preview their materials. "
     "Drawing with the right mouse button uses Alt. Choosing a new color resets that material to Solid.\n\n"
     "No Color is the white pattern tile crossed by a red slash. It makes the selected material "
     "transparent; use it for Alt to leave the second color of a two-color pattern untouched. "
     "Enabled turns an outline or fill on or off. Smooth lines switches between antialiased and "
     "crisp pixel edges. Size sets stroke width.\n\n"
     "The twelve brushes include round, calligraphy nibs, airbrush, oil, crayon, marker, natural pencil, "
     "watercolor, bristles, soft pastel and charcoal. Materials also offers dots, stripes, checks, "
     "bricks, woven cloth, houndstooth and seven dither densities, plus grain, paper tooth and load.\n\n"
     "Edit colors opens RGB, hex and OKLab controls. Custom colors remembers sixteen saved colors "
     "between launches. Choose a slot, set its color and click Add to custom colors. OK applies the "
     "choice; Cancel keeps the original."},
    {"3. Fill, erase, pick and magnify",
     "The bucket fills the connected patch you click. A closed outline keeps the color inside; even a "
     "tiny gap lets it reach the outside. Undo if it went farther than you wanted. Patterns use the same "
     "region, with Color 1 and Color 2 making the design.\n\nClick the eraser icon to choose Hard or Soft "
     "and set its diameter. Its round pink preview shows the area it will touch. Hard clears every "
     "touched pixel to transparency. Soft erases most strongly under the center and tapers toward the "
     "edge. "
     "Undo restores erased marks. The eyedropper picks a color from the picture.\n\nThe magnifying glass "
     "shows a floating 4× pixel preview beside the pointer. Click to zoom in around that point; right-click "
     "zooms "
     "out. "
     "The bottom slider goes up to 1600%. The pencil previews the exact pixel under its tip before you "
     "click. "
     "Hover previews and zoom do not change your saved picture."},
    {"4. Lines, curves and shapes",
     "Choose a shape in the little gallery. Hold the mouse button where it should start, drag, "
     "then release. Outline uses Primary; Fill uses Alt. Materials lets you turn either part "
     "off. Size sets outline thickness. Hold Ctrl while drawing Circle or Oval to keep the initial "
     "click at the center and grow the radius toward the pointer. Hold Shift for constrained lines "
     "and equal-sided shapes.\n\nChoose Bézier or Arc. Drag the starting line, or click its two endpoints. "
     "Bézier has a draggable control handle from each endpoint; Arc has one middle handle. "
     "Keep dragging the handles until the curve is right. Undo and Redo keep the controls editable. "
     "Escape or choosing another tool releases the curve and keeps the drawing. If you only placed "
     "the first point, Escape cancels it without leaving a mark. A polygon is made "
     "by clicking corners; clicking the first corner closes it. Escape also finishes it."},
    {"5. The continuous junction path",
     "Choose Path at the right of Home. Click to place each corner; the next segment follows the "
     "pointer until you click. Blue dots mark editable junctions. Right-drag a dot to move it, "
     "including every branch sharing that junction.\n\n"
     "Click an existing junction to start another branch there. Right-click empty canvas to finish "
     "the current run, then click elsewhere to start a separate run. Earlier runs remain visible "
     "when new ones overlap them. Undo and Redo preserve the editable geometry. Escape or changing "
     "tools finishes the paths and releases every node while leaving the drawing in place.\n\n"
     "Continuous path keeps each run open. Uncheck it to add a closing edge. An ordinary polygon "
     "closes when you click its first corner."},
    {"6. Select, move, copy and paste",
     "Select draws a rectangle around part of the picture. The little arrow beneath Select also offers "
     "Free-form selection: draw a lasso around an object. Drag inside the selected area to move it. Arrow "
     "keys nudge it one pixel; Shift plus an arrow moves ten pixels. Escape places it.\n\nCopy keeps the "
     "original. Cut removes it. Paste makes a new movable selection. Paste from opens an image file as a "
     "selection. Drag an image file from Finder or your file manager onto this window to paste it "
     "immediately. A larger pasted picture expands the canvas.\n\nTransparent selection skips pixels "
     "exactly equal to Color 2. Crop keeps only the selected rectangle. With nothing selected, Crop first "
     "switches to Select so you can mark the area. Delete removes selected content. Ctrl+A followed by "
     "Delete clears the whole picture without a drag. "
     "Without a selection, Delete clears the canvas to Color 2."},
    {"7. The rubber stamp",
     "Choose Stamp at the right of Home. Open Stamp tools and pick one of 39 masks, including stars, arrows, "
     "hearts, callouts, curves and the original circle, pill, square and rectangle. "
     "Choose its width and height. The outline under the pointer shows what you will lift. Click once "
     "over a part of the picture to load the stamp. The original stays on the page.\n\nNow click in other "
     "places to print repeated copies, or hold and drag to scrub with the sample. R turns the stamp 15 "
     "degrees clockwise; Shift+R turns it the other "
     "way. + makes it bigger and - makes it smaller. These keys change the stamp, not the "
     "canvas.\n\nTransparent stamp preserves transparent pixels and skips pixels matching Color 2. Turn "
     "it off to include that background color. The area outside the chosen mask is always transparent. "
     "Right-click or press Escape to clear the old sample and reset its rotation and scale, even while "
     "it is preparing. The next canvas click picks another. Stamp > Lift a new stamp does the same; "
     "the Stamp ribbon holds its size and angle controls."},
    {"8. Resize, rotate and the canvas",
     "Resize accepts percentages or pixel dimensions. Keep Maintain aspect ratio checked to avoid "
     "stretching. Scale artwork uses CONV* to reconstruct the image; turn it off to change only "
     "the canvas boundary. Smaller boundaries crop. Larger ones add Color 2 around the existing "
     "picture.\n\nThe round handle just outside a selection's upper-right corner turns it freely. "
     "Drag it around the object; hold Shift for 15-degree steps. Release to finish the rotation, "
     "then press Escape when you want to place the selection. The Selection ribbon also lets you enter any "
     "angle, "
     "with positive numbers turning clockwise. CONV prepares the original material once and reuses "
     "it while you drag. Large selections may take a moment to prepare.\n\nThe same menu keeps exact "
     "quarter turns, a half turn, and horizontal or vertical flips. With a selection, only that "
     "content changes. Without one, the whole picture rotates into an expanded canvas. "
     "Resize also has horizontal and vertical Skew angles. Drag the little handles around a selection "
     "to resize its pixels, or the three handles on the canvas edge to change the paper boundary. "
     "Properties shows dimensions and lets you change the canvas size and JPEG quality."},
    {"9. Reshape an object like soft cloth",
     "Choose Free-form selection from the Select arrow. Draw all the way around your object. "
     "Then choose Selection > Start mesh. Paint puts a coarse mesh of blue "
     "knobs around the edge and through the inside. Smaller Mesh spacing gives you more knobs. "
     "Drag a knob to stretch the object. A red warning means that move would fold the mesh, "
     "so the last safe shape stays in place. Press Escape to finish, or Undo to restore the "
     "original object.\n\nThe first preparation can take a few seconds. You may move knobs "
     "while it prepares. Drag previews use point samples; finishing applies an antialiasing filter "
     "where the mesh compresses the image. A source can contain at most one million pixels. Select "
     "a smaller "
     "object or resize it first if Paint explains that it is too large."},
    {"10. Put words in your picture",
     "Click the A tool, then click the canvas. Type into the box. The Text tab lets you choose a "
     "font face, size, bold, italic, underline, "
     "strikeout, and an opaque "
     "background. Drag Move text to reposition the box and drag its edge handles to resize it. "
     "Word wrap moves whole words onto the next line; switch it off for explicit line breaks only. "
     "Color 1 colors the letters; Color 2 colors an opaque background. Place text "
     "stamps the words into the picture. Cancel text discards them. Undo removes placed text if "
     "you change your mind. Ctrl+Enter places text; Escape cancels it."},
    {"11. Open, save and print",
     "File > Open replaces the picture with an image from disk. Save writes your picture. Save as lets "
     "you choose a new name or format. PNG, TIFF, TGA and lossless WebP can preserve transparency. JPEG "
     "is smaller for photographs but loses detail each time it is re-encoded. BMP, JPEG and GIF use a "
     "white background for transparent areas. GIF uses a limited palette and is export-only.\n\nYou can "
     "open PNG, JPEG, direct-color BMP, uncompressed RGB/RGBA TIFF, TGA, WebP, SVG, AVIF, ICO and CUR, "
     "plus HEIC/HEIF on macOS. GIF and paletted BMP import are disabled because their available decoder "
     "paths are unsafe for untrusted files. SVG raster image elements are likewise disabled. Animated "
     "WebP and APNG are not supported. "
     "Saving exports flat raster artwork; ICO and CUR retain their multiple images.\n\nThe application "
     "asks before replacing "
     "unsaved work. Cancel keeps you here. Printing uses the system's print dialog where available. Keep "
     "a saved copy of pictures you care about.\n\nRecent pictures remembers your last twelve opened or "
     "saved files. From scanner or camera uses Windows Image Acquisition on Windows. On a Mac it opens "
     "Image Capture; on Linux it opens Document Scanner or XSane. In those capture applications, save "
     "the picture, then drag it into Paint or choose Paste from. A connected device and its driver "
     "are needed.\n\nSet as desktop background offers Fill "
     "(cover the screen), Tile (repeat), and Center (keep the picture's size). These commands keep "
     "their exported copies in Paint's private preferences folder so the other application can "
     "continue using them."},
    {"12. View and keyboard guide",
     "View has Zoom in, Zoom out, 100%, rulers, gridlines, the status bar, Full screen and Fit window. "
     "Pixel gridlines appear at 400% zoom and above. Ctrl or Command plus the mouse wheel changes zoom. "
     "F11 toggles full screen.\n\nCtrl / Command shortcuts:\nN: New    O: Open    S: Save\nShift+S or "
     "F12: Save as\nZ: Undo    Y or Shift+Z: Redo\nX: Cut    C: Copy    V: Paste\nA: Select all    P: "
     "Print\nE: Properties    W: Resize    I: Invert colors\nG: Gridlines\n\nEscape places a selection or "
     "releases a path, clears a loaded stamp, or cancels text. F1 opens this guide. R and +/- change "
     "a loaded stamp."},
    {"13. How I made this - Astra",
     "I am Astra. I built Rainstar Paint in C++, first with Dear ImGui and SDL and now GUI.Forms, using "
     "explicit types, "
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
     "an independent implementation; it does not contain Microsoft's Paint code or artwork."},
    {"14. Free for everyone",
     "To the Holy One, blessed be He, from whom all good things come. This work is dedicated in "
     "gratitude for the nourishment that sustains human life, the energy that powers our tools, "
     "and the opportunity to weave information into works of use and beauty.\n\n"
     "Author: Astra\nSponsor: Rainstar\nCopyright (c) 2026 "
     "joshuah.rainstar@gmail.com\n\nRainstar Paint is free and open source under the MIT license. Anyone "
     "may use it, learn from it, change it, and share it, including for commercial work. Keep the "
     "copyright and license notice with copies. The license does not promise a warranty.\n\nYour pictures "
     "are yours. There is no account, subscription, advertising, or upload service in the drawing "
     "workflow. The complete source and dependency notices are supplied with the project."},
};
} // namespace paint
