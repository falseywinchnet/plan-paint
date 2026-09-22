# Plan Paint native build

GUI.Forms is the primary frontend in Plan Paint 1.0. The SDL/ImGui frontend is deprecated and excluded from release packages.


Plan Paint 0.3.4 uses GUI.Forms by default. The executable links the installed
`GUIForms::Application` target and Paint's document, raster, material, CONV and
format libraries. It does not link SDL, ImGui or GTK. The optional comparison
frontend remains available with `RAINSTAR_LEGACY_UI=ON`.

## Build the SDK and application

The release includes a checksum-pinned GUI.Forms source snapshot. The fetch
command is an explicit development step; the installed application never fetches
source or dependencies. Python 3.10 or newer is required for the packaging tools.

```sh
python3 scripts/fetch-gui-forms.py
```

On Apple Silicon macOS, install CMake, Ninja and the codec development libraries,
fetch the toolkit's pinned rendering dependencies, and install an SDK:

```sh
brew install cmake ninja libtiff webp dav1d
sh build-deps/gui-forms/third_party/fetch_skia_cpu.sh
sh build-deps/gui-forms/third_party/fetch_text_stack.sh
cmake -S build-deps/gui-forms -B build-deps/gui-forms-build \
  -DCMAKE_BUILD_TYPE=Release -DGUI_FORMS_BUILD_GALLERY=OFF \
  -DGUI_FORMS_ENABLE_MACOS_HOST=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/build-deps/gui-forms-sdk"
cmake --build build-deps/gui-forms-build --parallel 4
ctest --test-dir build-deps/gui-forms-build --output-on-failure
cmake --install build-deps/gui-forms-build
cmake -S . -B build-forms -DCMAKE_BUILD_TYPE=Release -DRAINSTAR_BUNDLED_TIFF=ON \
  -DGUIForms_DIR="$PWD/build-deps/gui-forms-sdk/lib/cmake/GUIForms" \
  -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build-forms --parallel 4
ctest --test-dir build-forms --output-on-failure
open build-forms/plan-paint.app
python3 scripts/package-macos.py --gui-forms-sdk build-deps/gui-forms-sdk
```

[ci-windows.sh](../scripts/ci-windows.sh) contains the MinGW/MSYS2 build and
[ci-linux.sh](../scripts/ci-linux.sh) contains the Alpine musl build. Their
prerequisites and invocations are in the [workflow](../.github/workflows/build.yml).
Windows uses the native GDI host. Mac and Linux use CPU Skia; Linux also uses
HarfBuzz and FreeType for text. Each package includes its fonts and licenses.

## Platform behavior

| Platform | Window and input host | Desktop integration |
| --- | --- | --- |
| macOS ARM64 | AppKit, native fullscreen, native file dialogs and clipboard | Native print/page setup, Image Capture and desktop background services |
| Windows x64 | Win32, GDI presentation, native dialogs, DIB/private RGBA clipboard | Native print/page setup, acquisition and desktop background services; Wine runtime checks also cover the application |
| Linux x64 / ARM64 | X11; works in XWayland sessions, including Weston; dwm and twm checked | Retained GUI.Forms file dialogs, cross-process PNG/RGBA/text clipboard, Xdnd file drops; optional CUPS printing and scanner/desktop services |

The Linux host currently uses one X screen at scale 1. It does not implement a
native Wayland protocol backend, RandR monitor changes, mixed-DPI displays or
Linux accessibility publication. A Wayland desktop must provide XWayland.
Linux print setup submits a fitted, alpha-flattened PostScript page to the
configured `lp` service. Tests rasterize the output with Ghostscript and verify
orientation, margins and pixel quadrants. Physical printer output and every
scanner/desktop combination have not been verified. Missing optional desktop
services produce an error rather than reporting success.

Linux archives bundle their musl loader, complete shared-library closure, fonts
and X11 locale data. Run the top-level `Plan Paint` launcher and retain the
complete directory. They can start on a glibc system without an installed musl
loader. No package manager or network connection is needed to draw, use help,
open or save pictures. The Mac application is signed ad hoc; the installer is
unsigned and not Developer ID notarized.

## Document and display ownership

Paint retains straight-alpha RGBA pixels, meaningful RGB under zero alpha,
selection state, tool transactions, undo history and format metadata. Only its
display cache is premultiplied BGRA. The GUI.Forms `RasterCanvas` owns tiled
presentation and clipping; small strokes publish bounded bitmap edits.
Selections and shape previews publish the composed image. While a selection is
rotating or resizing, a separate viewport-bounded overlay shows the transformed
pixels immediately. Its temporary bilinear display samples never enter document
history; release commits the CONV result from the unchanged original samples.

The derived canvas uses normal routed pointer input, focus, capture, and clipped
paint overlays. Compound controls establish children in `initialize_control_tree`.
CONV preparation runs outside the UI thread, posts completion through the toolkit
dispatcher, and rejects stale generations. Closing or cancelling an operation
preserves lifetime boundaries.

The ribbon provides Home, View, Materials, Atlas and context-sensitive tool pages.
Selection includes mesh, rotation, placement and cancellation. Stamp Scale and
Angle labels support scrubbing. The help sidebar is bundled locally. Printing
remains application code over native window ownership; it does not introduce a
second toolkit or renderer.

Zoom-in preserves the image coordinate under the focus. Zoom-out then clamps each
screen translation to `[viewport - image * scale, 0]` while the image exceeds
that viewport dimension. Once an axis fits, its translation is
`(viewport - image * scale) / 2`. Wheel, ribbon commands, status buttons and the
slider share this rule.

## Verification

CTest covers document editing and formats, numerical transforms, RGBA
preservation, input routing and capture, undo, editable curves, branching paths,
selection transforms, stamp reset, pointer-anchored zoom, context ribbon settings,
text placement, modal transactions, asynchronous cancellation, Atlas sequences,
and icon/cursor round trips. The native fixture opens an actual application,
drives canvas input, checks tool/history state, and closes through the host.

The toolkit's Linux integration tests exchange large UTF-8 and PNG clipboard
payloads with an external X11 process, exercise INCR transfers and file drops,
and accept actual open/save dialogs. Native app screenshots and interactive
checks complement these tests; a headless test result alone is not a claim that
every desktop service or window-manager configuration was exercised.

## Source layout and toolkit snapshot

Application UI code lives in `src/forms/`. The shared document, raster tools,
CONV transforms and codecs live in `src/`; the optional earlier frontend remains
available for comparison and is excluded from the default build.

The pinned GUI.Forms snapshot includes optional font packs, shared font storage
and macOS exposure handling. Fetch it with `scripts/fetch-gui-forms.py` before
building the SDK. Install that SDK into a separate prefix and configure Paint
with `CMAKE_PREFIX_PATH` pointing to it. Every packaged release includes its
matching toolkit library.

## Nested dropdown focus correction

The pinned 0.2.3 toolkit source archive is augmented by the hash-verified patch
listed in `third_party/gui-forms.lock.json`. The fetch script validates both
the archive and patch, applies the patch in a temporary source directory, and
records it in `SOURCE_PATCHES.json` before publishing the destination. Git is
required for this source-preparation step.

The patch recognizes the registered popup owner when a separate window overlay
enters a containing focus scope. It fixes dropdowns inside Paint's color and
settings dialogs while continuing to reject unrelated popup roots. The toolkit
change is maintained independently as commit
`f47893b` and includes repeated open/close, modal containment, focus restoration,
and owner-unavailability regressions.
