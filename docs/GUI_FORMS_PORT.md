# GUI.Forms frontend

The native GUI.Forms integration is a separate executable, `rainstar-paint-forms`.
It links the installed `GUIForms::Application` target and the same Paint document,
image, raster, material, CONV and format libraries as the existing frontend.
It does not link SDL or ImGui. The existing `rainstar-paint` target remains the
feature-complete comparison application while integration proceeds.

## Build

Use the macOS ARM64 GUI.Forms SDK built from `codex/paint-canvas-extension`,
commit `7444815`, or an SDK that includes that change. The original `9a4b156`
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
  and branching, outline/fill, patterns and basic click-to-lift stamping.
- Open, save and save-as through native dialogs and the existing format engine;
  saving keeps live curve/path sessions. Clipboard images use GUI.Forms host
  services. macOS printing and page setup reuse Paint's native implementation.
- Anchored wheel zoom, middle-button pan, keyboard commands and image status.
- Brush movement publishes only a conservative affected rectangle. GUI.Forms
  owns the tiled display update. Selection and shape previews currently publish
  the complete composed image.

## Remaining integration

The GUI.Forms executable does **not** yet have feature parity. In particular,
these accepted functions remain in the comparison frontend:

- Canvas text editing, its formatting and custom-font controls.
- Asynchronous CONV reshape, free rotation and transform controls; stamp size,
  shape, scale and angle controls and its compiled CONV preview.
- Full atlas/frame/cursor-hotspot UI, resize/skew/property dialogs, rich color
  editing, custom palette persistence and brush material settings.
- Recent-file menus, acquisition, email/wallpaper commands, print preview,
  drag-and-drop, the full keyboard set and the original icon-based ribbon detail.
- Windows native execution and packaging; Linux native execution requires the
  toolkit's native Linux host. Neither is established by this macOS port.

Do not replace the default frontend or advertise parity until those behaviors
are carried across and compared against the existing interaction tests.

## Toolkit findings

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
selection movement, path branching, stamp reset and pointer-anchored zoom.
`paint-forms-native-tests` exercises application startup, native host attachment,
routed drawing/curve handles, capture release and application close. Run its
macOS bundle with `--keep-open` to inspect the real rendered interaction fixture.
This fixture uses Paint's actual tools; it is not a mockup.

Continue running the existing core, format, warp and interaction suites. A
successful toolkit gallery or the current integration fixture is not evidence
for the still-unported behavior listed above.
