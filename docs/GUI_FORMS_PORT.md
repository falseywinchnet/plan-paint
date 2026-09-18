# GUI.Forms frontend

The native GUI.Forms integration is a separate executable, `rainstar-paint-forms`.
It links the installed `GUIForms::Application` target and the same Paint document,
image, raster, material, CONV and format libraries as the existing frontend.
It does not link SDL or ImGui. The existing `rainstar-paint` target remains the
feature-complete comparison application while integration proceeds.

## Build

Use the macOS ARM64 GUI.Forms SDK built from `codex/paint-canvas-extension`,
commit `8d448f5`, or an SDK that includes its canvas, ribbon composition,
and native full-screen handle extensions. The original `9a4b156`
package declares RasterCanvas final and cannot build this frontend. The extension
allows an application canvas to override normal Control input and overlay hooks;
bitmap presentation and resource ownership remain in GUI.Forms.

```sh
cmake -S . -B build-forms \
  -DRAINSTAR_GUI_FORMS=ON \
  -DRAINSTAR_LEGACY_UI=OFF \
  -DGUIForms_DIR=/path/to/sdk/lib/cmake/GUIForms \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-forms --parallel 4
ctest --test-dir build-forms --output-on-failure
```

Set both UI options to ON to build both executables together. An ordinary build
continues to build the existing frontend. No GUI.Forms source checkout or private
renderer headers are included by Paint. Font resources come from the installed
SDK and are copied into the macOS bundle. This is a development build: its SDK
and codec dynamic libraries must remain available. It is not a redistributable
release package.

## Connected behavior

- Native application lifecycle, close confirmation, retained menu and ribbon
  controls, focus, pointer capture and canvas-local tool input.
- Straight-alpha RGBA documents and undo history. The presentation cache alone
  uses rounded premultiplied BGRA; hidden transparent RGB stays in the document.
- Pencil, material brushes, flood fill, eraser, color picker, rectangular and
  lasso selections, movement, cut/copy/paste, crop, quarter turns and flips.
- Shape drawing, Shift constraints, retained Bézier and arc handles, path nodes
  and branching, outline/fill, and patterns.
- Open, save and save-as through native dialogs and the existing format engine;
  saving keeps live curve/path sessions. Clipboard images use GUI.Forms host
  services. macOS printing and page setup reuse Paint's native implementation.
- A classic icon ribbon with Home, View, Patterns & tools, and an active-tool
  context page. The context page follows selection, stamp, brush, shape, path,
  pencil, fill, eraser, picker, magnifier, and text settings. Transform controls
  include mesh editing, arbitrary-angle rotation, placement and cancellation.
- Optical-size SVG icon artwork at 16, 24 and 32 logical pixels, rasterized at
  twice density and embedded as PNG. Expanded shapes have named hover/focus
  tooltips. Brush and pattern galleries show samples from the actual raster engine.
- Graphical Fill pattern choices, grain scale, paper tooth, paint load, grain
  angle, and separate edge/fill media. View includes zoom, rulers, pixel gridlines
  (visible at 400% and above), status visibility, full screen, and fit window.
- Anchored wheel zoom, middle-button pan, keyboard commands, image/selection
  dimensions, image-local cursor coordinates, and a logarithmic zoom slider with
  plus/minus and a percentage button that resets to 100%.
- Retained color and resize dialogs with modal focus/input boundaries. Color
  editing includes HSV, RGBA, hex, OKLab and the persistent custom palette. Resize
  supports pixels/percentages, aspect lock, CONV scaling, canvas boundary changes,
  and horizontal/vertical skew.
- Editable canvas text with Unicode caret/selection, clipboard, local text undo,
  installed/custom fonts, formatting, wrapping, opaque backgrounds, and draggable
  bounds. Placing uses the same text rasterizer as the comparison frontend.
- Asynchronous CONV mesh deformation and free rotation. Rotation has a canvas
  handle, numeric angle and Shift snapping. Stamp capture dimensions, scale and
  angle controls use the compiled CONV material and a retained cursor preview.
  Worker completion posts through GUI.Forms' dispatcher; generation checks reject
  obsolete results. Cancellation and destruction preserve ownership boundaries.
- Brush movement publishes only a conservative affected rectangle. GUI.Forms
  owns the tiled display update. Selection and shape previews currently publish
  the complete composed image.

## Remaining integration

The GUI.Forms executable does **not** yet have feature parity. In particular,
these accepted functions remain in the comparison frontend:

- Full atlas/frame/cursor-hotspot UI and property dialogs.
- Recent-file menus, acquisition, email/wallpaper commands, print preview,
  drag-and-drop and the full keyboard set.
- Windows native execution and packaging; Linux native execution requires the
  toolkit's native Linux host. Neither is established by this macOS port.

Do not replace the default frontend or advertise parity until those behaviors
are carried across and compared against the existing interaction tests.

## Toolkit findings

`DropDownButtonEdge::bottom` puts the disclosure in a narrow lower strip for
large icon commands while preserving the same split-button routing. Multiline
button captions measure and paint as separate centered lines. These are normal
control composition features, shared with other consumers.

`ApplicationWindowHandle::toggle_full_screen()` keeps native full-screen
requests in the toolkit's host adapters. macOS transitions were exercised in
Paint; the Windows adapter was syntax-checked with MinGW but has not been run
on Windows as part of this port.


`RasterCanvas` inheritance removes the need to put tool handling on a parent
or observe input that cannot be marked handled. Paint's derived canvas uses a
weak editor reference, ordinary routed pointer input and the normal clipped
paint-overlay hook. No second rendering or event-routing system is introduced.

Compound controls use GUI.Forms' existing `initialize_control_tree` factory
hook. Children cannot establish weak parent links during C++ shared-owner
construction. That ownership rule needs a clear example in the application
guide; it does not require a Paint-specific workaround or changed ownership.

Application changes must not accidentally erase the toolkit's bounded-update
benefits: converting an entire document or invalidating the entire window on
small strokes makes the application do unnecessary work even when the viewport
supports local damage. The stroke path now publishes bounded bitmap edits.

Native print services remain application code. They do not justify building a
new printing subsystem inside GUI.Forms.

## Verification

`paint-forms-tests` exercises routed capture beyond control bounds, RGBA
preservation, one-coat material deposition, undo, editable curves across save,
selection movement, path branching, stamp reset, pointer-anchored zoom, ribbon
page switching, context settings, material values, status controls and cursor
coordinates after pan/zoom. Gallery tests verify shape tooltip scheduling,
dismissal and reopening, plus modal color/resize transactions and RGBA palette
persistence. Text tests compare placed pixels with its preview and cover Unicode
editing, undo, and cancellation. Transform tests compare rotated image bytes
with the CONV renderer and cover mesh/stamp/skew cancellation, late results,
document undo and destruction during background work.
`paint-forms-native-tests` exercises application startup, native host attachment,
routed drawing/curve handles, capture release and application close. Run its
macOS bundle with `--keep-open` to inspect the real rendered interaction fixture.
This fixture uses Paint's actual tools; it is not a mockup.

Continue running the existing core, format, warp and interaction suites. A
successful toolkit gallery or the current integration fixture is not evidence
for the still-unported behavior listed above.
