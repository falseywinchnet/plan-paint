# Third-party notices

Rainstar Paint's original code is MIT, Copyright (c) 2026 joshuah.rainstar@gmail.com.

- Dear ImGui v1.91.9b, commit f5befd2d29e66809cd1110a152e375a7f1981f06: MIT, Copyright Omar Cornut. See vendor/imgui/LICENSE.txt.
- stb, commit 2c980bb59875b0d32144a71867fbdebb2f77cd20: MIT option selected. See vendor/stb/LICENSE.
- gif-h, commit 70b645280d5e687f5217177c9cfa2889b0a2ad5f: MIT, Charlie Tangora. License is at the top of vendor/gif-h/gif.h.
- SDL3: zlib license. https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt
- libtiff: permissive TIFF license. https://gitlab.com/libtiff/libtiff/-/blob/master/LICENSE.md
- libavif 1.4.0, commit bcfcd821dab042c83dbb83ef72121f59b3c00661: BSD-2-Clause. See packaging/licenses/libavif-LICENSE.
- dav1d AV1 decoder: BSD-2-Clause. See packaging/licenses/dav1d-COPYING.
- LunaSVG 3.5.0, commit 83c58df8103dc7dca423dfd824992af94d49bed6, and its bundled PlutoVG 1.3.1: MIT. PlutoVG includes FreeType rasterizer code and stb components under their permissive terms. See packaging/licenses/svg. This software uses portions of the FreeType Project (https://freetype.org), copyright David Turner, Robert Wilhelm and Werner Lemberg.
- TinyXML-2 11.0.0: zlib license. Used to check SVG features and resolve local image resources. See packaging/licenses/svg/tinyxml2-LICENSE.txt.
- libwebp: BSD-style license. https://chromium.googlesource.com/webm/libwebp/+/HEAD/COPYING
- Portsmouth Regular, Bold, Italic, Bold Italic and Mono: embedded font family. Original source-font copyright and license notices are preserved in [packaging/licenses/Portsmouth-source-notices.txt](packaging/licenses/Portsmouth-source-notices.txt). System fonts selected for artwork are not redistributed.
- The vendored Dear ImGui source tree also contains Droid Sans under Apache License 2.0; it is not the application interface font.

Microsoft, Windows and Paint names identify the reference application and belong to their respective owner. Rainstar Paint is independent and uses newly drawn icons.

Binary packages also carry the notices for their bundled codec dependencies. macOS bundles zstd under its BSD option, liblzma under 0BSD, and libjpeg-turbo under the IJG/BSD terms. This software is based in part on the work of the Independent JPEG Group. Windows packages include each installed vcpkg dependency's copyright notice. Linux uses the host GTK 3 desktop print service; GTK is not part of the MIT application and is not redistributed in the portable folder.
