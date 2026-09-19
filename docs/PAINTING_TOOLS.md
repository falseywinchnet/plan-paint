# Painting and lettering in 0.3

## Brushes

Open the Brushes arrow to choose **Additive**, **Mix**, or **Heal**. The tool tab exposes settings for the selected family. Primary and Alt retain their separate colors and materials.

Additive brushes deposit paint. Calligraphy changes the tip, oil and bristle produce directional strands, and crayon, pencil, watercolor, pastel and charcoal use different grain and deposition profiles. Spray can uses a dense center and a soft falloff, with small lightness variations and smaller color variations in OKLab. Its **Glitter** checkbox adds fine facets and sparse bright glints. Brush-only dynamics do not change the materials used to fill shapes or paths.

Mix brushes transform existing pixels. Choose Ripples, Glass tile, Wigner counterflow, Support braid, Support lens, Moving rooms, Holonomy, or Flux knots. Strength controls the blend with the original image; Scale controls the spatial period and Phase shifts the pattern. Each gesture samples an immutable starting image, so repeated pointer events over the same position do not repeatedly degrade the result.

Heal first needs a source: its first click captures one, or use **Set source** to replace it. The following stroke draws from that source with an aligned offset that persists between strokes. Hardness 1 copies source RGBA values in the brush interior. Softer settings feather the edge and correct source detail toward the target's local mean lightness and chromatic components in OKLab. Correction controls that adjustment. A new source is a snapshot; subsequent painting does not alter the captured source.

Pencil and all brush families offer **Stabilizer** and a lag distance. The pointer trajectory is filtered in distance along the stroke, with the same integration for a straight movement whether it arrives in one event or many.

Eraser offers Hard, Soft, Blur, Sharpen and Smudge. Blur and Sharpen use a local perceptual average; Smudge displaces samples along the drag. Strength and Scale appear for these three modes.

## Stamp

Capture a region with Stamp, then click or drag to place it. **Hardness** 1 retains the capture mask; lower settings feather inward from the chosen shape's boundary. The width, height, scale and angle controls remain available.

**Add material** retains the current stamp and returns to capture mode. The retained stamp appears at half opacity while a new region is chosen. The new content is aligned to the retained stamp's size and contributes detail after local OKLab color correction; the retained silhouette remains. **Clear stamp** discards both the current stamp and a retained basis.

## Guides and selection

Guide is the drafting triangle beside Path. Click to add vertices; right-click or **Set guide** closes it. Drag a vertex to reshape it or drag inside a closed guide to move it. **Protect body** or the filled-square ribbon switch blocks the inside; when off, the boundary acts as a line stencil. **Clear guide** removes it.

A guide constrains Pencil, Brush, Fill and Stamp. It is display and editing state and never becomes exported artwork. Selection, Path, shapes, text, crop, cut, paste, resize, reshape and document resizing clear it. Changing atlas frames also clears it.

Choose **Tightening lasso** or **Inner-void lasso** from the selection menu. Tightening compares the lasso boundary's background in OKLab, ignores small perceptual variations such as JPEG flecks, and retains the connected object nearest its center with a feathered edge. Inner-void starts from the center background and keeps its connected enclosed region. The lasso must surround the intended subject or void; it is a perceptual color selection, not semantic object recognition.

With a selection active, choose Guide to protect its silhouette immediately. Selection-derived guides retain holes and feathered coverage until their vertices are edited.

## Paths

In the Path arrow menu, choose **Swap segment**, then Bézier or Arc. Click an existing segment between active blue endpoints. Drag the controls to reshape it, then right-click to return to ordinary Path editing. The endpoints remain part of the path; Undo and Redo retain the active geometry.

## Atlas painting

These controls apply while an atlas frame is active:

- **Warp edges** wraps strokes horizontally and vertically. Neighbor copies around the current frame show seams as it changes.
- **Preserve transparency** retains every original alpha byte, including partially transparent edges. Fully transparent pixels retain their hidden RGB values. Hard and soft erasing make no changes while this is enabled.
- Right-click another frame thumbnail to show it over the current frame at 50% opacity. The current frame remains editable. **Dismiss reference** removes the overlay. It does not enter the saved image.

## Text

The font list includes DynaPuff, Bubble Sans, Anton and Titan One, with their open-font licenses bundled alongside them. **Outline letters** uses Primary for contours, including the edges of counters such as the holes in B and O, and Alt for the body of the letters. The holes themselves remain open. Outline width is adjustable; a thin font can have little interior left at a large width.

The WordArt selector offers 3D extrusion, embossed edges and an OKLab gradient between Primary and Alt. **Skew**, **Perspective** and **Bend** affect the live text box. Text, selection and caret positioning remain editable under the same mapping. **Reset** restores plain geometry and treatment. **Place text** or Ctrl/Command+Enter commits exactly the current raster preview; Escape cancels it.

## Color and view

The ribbon palette button cycles Basic, Custom and Themed, with 30 slots in each set. The color dialog contains the 30 custom slots; existing 16-color preferences migrate automatically.

Choose RGB or OKHSL above the color plane. The RGB view uses hue, saturation and value to choose sRGB colors; OKHSL uses the perceptual picker conversion by Björn Ottosson. Numeric RGB, alpha, hexadecimal and OKLab controls remain available. **Mosaic** maps the plane to 96 deterministic colors through a perceptual Voronoi palette and coarsens hue selection. Each color space has its own fixed palette.

Themed colors form ten columns of Primary, Alt and accent: Glue, Phosphor, Camo, Royal, Afterglow, Arcade, Lagoon, Porcelain, Bordeaux and Ochre. Hover a themed swatch for its theme and position.

Eyedropper defaults to Exact pixel. Small average samples 5×5 pixels; Representative rejects isolated light and dark flecks in that neighborhood. Its optional local lens and the Magnifier use a round, cursor-centered 8× view.

Settings retain the default canvas surround and add green felt. The ribbon and status bar resize with the window, down to an 800-pixel client width.

## Sources and implementation scope

- [OKHSL and the reference picker](https://bottosson.github.io/posts/colorpicker/) supply the actual OKHSL conversion. The vendored MIT implementation is promoted to double precision.
- [iWantHue's theory](https://medialab.github.io/iwanthue/theory/) informed the perceptual clustering approach. Rainstar's deterministic palette code is independently implemented.
- [Kid Pix 4's user guide](https://www.lisd.org/technology/itswebs/elem/tech/handouts/Kid%20Pix%204%20User%20Guide.pdf) describes local Mixer effects. Rainstar's Mix tools use its own raster implementation.
- The Mix family adapts geometric and perceptual color laws from CONV Support Toys 0.4.1 in the [OBS plugin source distribution](https://paymenottowork.com/obs-plugins/). The local brush implementation does not reproduce the full OBS support solver or claim pixel equivalence with that plugin. Its source names the counterflow effect **Wigner counterflow**.
- The [Fine Glitter Texture demonstration by PrettyWebz Media](https://www.youtube.com/watch?v=wdm7KHKfxxw) informed the combination of fine grain and sparse high-intensity highlights. Rainstar generates its own facets during painting.
