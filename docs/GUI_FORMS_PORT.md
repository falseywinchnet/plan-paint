# GUI.Forms frontend

The native GUI.Forms integration is a separate executable, `rainstar-paint-forms`.
It links the installed `GUIForms::Application` target and the same Paint document,
image, raster, material, CONV and format libraries as the existing frontend.
It does not link SDL or ImGui. The existing `rainstar-paint` target remains the
feature-complete comparison application while integration proceeds.

## Build

Use the macOS ARM64 GUI.Forms SDK built from `codex/paint-canvas-extension`
(commit `ae40f6e`), with the canvas, ribbon composition, native full-screen, scrolling-layout,
keyboard, and diagonal-cursor extensions. The CMake configuration checks the
required API before compiling the frontend. The original `9a4b156`
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
SDK and are copied into the macOS bundle and beside Windows executables.
The build-directory executable uses development SDK and codec libraries.

To produce the separate macOS development application and installer:

```sh
python3 scripts/package-macos.py --frontend forms --gui-forms-sdk /path/to/sdk
```

The output under `dist/gui-forms/` includes its dependency closure, fonts and
licenses. The packager resolves `@rpath` references, rewrites them to bundled
libraries, removes development search paths, and verifies the ad-hoc signature.
The app is named `Rainstar Paint GUI.Forms.app`, with a separate installer
identifier. It is a local development package, not a notarized public release.

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
- Atlas thumbnail strip and scrollable gallery, equal-frame sprite grids with
  margins/spacing, frame sequences, whole-sheet editing, and icon/cursor size
  generation. Cursor hotspots support numeric entry and canvas picking. Legacy
  XOR cursor pixels display against the checkerboard while save retains metadata.
  Thumbnail image resources are limited to visible frames, including 4096-frame
  sheets. Opening the gallery reveals the current frame after layout.
- Recent pictures, native file drops, image properties, monochrome conversion,
  JPEG quality, print preview, acquisition, email composition, and desktop
  backgrounds through the existing native services. Deferred New/Open/Close
  requests resume after the icon-size save dialog succeeds.
- Canvas boundary handles and eight selection resize handles, with diagonal
  native cursors. Selection resizing uses CONV; canvas resizing uses the existing
  boundary operation. The comparison frontend's keyboard shortcuts are connected.
- Brush movement publishes only a conservative affected rectangle. GUI.Forms
  owns the tiled display update. Selection and shape previews currently publish
  the complete composed image.

## Remaining integration

The accepted command groups are connected on macOS, but cross-platform parity
has not been established:

- Windows source syntax checks pass; this frontend still needs a complete native
  Windows build, runtime interaction checks and packaging validation.
- Native Linux execution requires the toolkit's native Linux host.
- Acquisition devices, email attachment handoff, wallpaper changes and actual
  printer jobs have not been exercised end-to-end in this port session. Their
  command handlers reuse Paint's existing platform implementations.

Keep the existing frontend available until those platform and service checks
are complete. The native preview is a fit-to-window artwork preview, not a
printer-specific pagination proof.

## Toolkit findings

`FlowLayoutPanel` now measures its flowed content into the inherited scrolling
viewport. Wrapping remeasures when a scrollbar consumes width; scrolling moves
layout, clipping and input together. The regression covers horizontal scrolling,
wrapped rows, scroll-into-view and resize convergence. Paint uses the existing
control rather than maintaining a separate scrolling implementation.

Physical-key constants now cover the full F1–F12 range and stamp size keys.
Diagonal cursor kinds map to Win32 resize cursors and AppKit frame-resize cursors
on macOS 15 and later, with a crosshair fallback on older macOS.

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
document undo and destruction during background work. Atlas tests cover grid
cancellation, editing across frame switches, sequences, expanded gallery input,
CUR save/reload with hotspots, and bounded resources across 4096 frames. Desktop
transaction tests cover property cancellation/undo, alpha-preserving monochrome
conversion, image drops with unsaved-work cancellation, deferred icon saving,
print preview, and canvas/selection resize handles.
`paint-forms-native-tests` exercises application startup, native host attachment,
routed drawing/curve handles, capture release and application close. Run its
macOS bundle with `--keep-open` to inspect the real rendered interaction fixture.
This fixture uses Paint's actual tools; it is not a mockup.

Continue running the existing core, format, warp and interaction suites. A
successful toolkit gallery or the current integration fixture is not evidence
for the remaining platform and service checks listed above.
