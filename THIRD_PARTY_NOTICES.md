# Third-party notices

Rainstar Paint's original code is MIT, Copyright (c) 2026 joshuah.rainstar@gmail.com.

- Dear ImGui (optional comparison frontend) v1.91.9b, commit f5befd2d29e66809cd1110a152e375a7f1981f06: MIT, Copyright Omar Cornut. See vendor/imgui/LICENSE.txt.
- stb, commit 2c980bb59875b0d32144a71867fbdebb2f77cd20: MIT option selected. See vendor/stb/LICENSE.
- gif-h, commit 70b645280d5e687f5217177c9cfa2889b0a2ad5f: MIT, Charlie Tangora. License is at the top of vendor/gif-h/gif.h.
- SDL3 (optional comparison frontend): zlib license. https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt
- libtiff: permissive TIFF license. https://gitlab.com/libtiff/libtiff/-/blob/master/LICENSE.md
- libavif 1.4.0, commit bcfcd821dab042c83dbb83ef72121f59b3c00661: BSD-2-Clause. See packaging/licenses/libavif-LICENSE.
- dav1d AV1 decoder: BSD-2-Clause. See packaging/licenses/dav1d-COPYING.
- LunaSVG 3.5.0, commit 83c58df8103dc7dca423dfd824992af94d49bed6, and its bundled PlutoVG 1.3.1: MIT. PlutoVG includes FreeType rasterizer code and stb components under their permissive terms. See packaging/licenses/svg. This software uses portions of the FreeType Project (https://freetype.org), copyright David Turner, Robert Wilhelm and Werner Lemberg.
- TinyXML-2 11.0.0: zlib license. Used to check SVG features and resolve local image resources. See packaging/licenses/svg/tinyxml2-LICENSE.txt.
- libwebp: BSD-style license. https://chromium.googlesource.com/webm/libwebp/+/HEAD/COPYING
- Portsmouth Regular, Bold, Italic, Bold Italic and Mono: embedded font family. Original source-font copyright and license notices are preserved in [packaging/licenses/Portsmouth-source-notices.txt](packaging/licenses/Portsmouth-source-notices.txt). System fonts selected for artwork are not redistributed.
- The vendored Dear ImGui source tree also contains Droid Sans under Apache License 2.0; it is not the application interface font.

Microsoft, Windows and Paint names identify the reference application and belong to their respective owner. Rainstar Paint is independent and uses newly drawn icons.

Binary packages also carry the notices for their bundled codec dependencies. macOS bundles zstd under its BSD option, liblzma under 0BSD, and libjpeg-turbo under the IJG/BSD terms. This software is based in part on the work of the Independent JPEG Group. The native release uses GUI.Forms under its retained MIT license. The GUI.Forms package carries the notices for its Skia, HarfBuzz, FreeType, Unicode, PNG/zlib and bundled font resources. Windows uses the native Win32 CPU renderer. macOS and Linux use the private CPU Skia/text adapter.

Portable Linux packages include musl and X11 client libraries, with their notices in `licenses/runtime`; X11 locale data comes from libX11. Linux and MinGW Windows packages carry GCC runtime notices and the GCC Runtime Library Exception. MinGW-w64 and winpthreads notices are also included. Runtime source locations are listed in [packaging/licenses/runtime/SOURCES.md](packaging/licenses/runtime/SOURCES.md).

The released GUI.Forms application does not link SDL, Dear ImGui or GTK. Those sources and notices remain for the optional comparison frontend. Linux printing uses the configured operating-system CUPS service; acquisition and wallpaper commands use the relevant desktop services.
