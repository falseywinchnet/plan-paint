# Rainstar Paint

A free native C++ drawing program inspired by the ribbon edition of Windows 7 and Windows 10 Paint. Written by Astra, sponsored by Joshuah, with thanks to Hashem.

The project is in active development. Release downloads and verified platform results will be linked here after the release checks finish.

The familiar Home/View ribbon contains pencils, brushes, erasing, filling, text, shapes, selection, cropping, resizing, and rotation. The extensions include continuous paths with reusable snapping junctions, repeated shape-masked stamps, classic two-color pattern brushes and buckets, Office pastel swatches, and an RGB/OKLab/hex color editor. F1 opens an illustrated-by-the-interface, black-on-yellow help sidebar for new artists, with a first-person account of implementation choices by Astra.

Images open through stb, libtiff and libwebp. PNG, JPEG, BMP, GIF, TIFF, TGA and lossless WebP can be saved. Finder/file-manager drops behave as pasted floating selections. Native image clipboard support is included. Resize uses a tested native port of the website CONV reconstruction, with double-precision kernels and premultiplied alpha.

## Build

C++20, CMake 3.24 or newer, SDL3, libtiff and libwebp are required. Dear ImGui, stb and gif-h are pinned in `vendor/`. The program has no account or network service dependency.

On macOS with Homebrew:

```sh
brew install cmake sdl3 libtiff webp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --parallel
ctest --test-dir build --output-on-failure
open build/rainstar-paint.app
```

See the checked-in workflow for Windows and Linux build dependencies. Setting `RAINSTAR_SYSTEM_SDL=OFF` builds the pinned SDL release from source. The Linux package is a relocatable folder with a launcher; the Windows package is a portable folder; the macOS package installs the application in `/Applications`.

`python3 scripts/check-style.py` checks the house style's mechanical C++ exclusions. The complete supplied guidance is in `docs/PROGRAMMING_HOUSE_STYLE.md`.

## License and credits

All original application code, tests, documentation and newly drawn icons are under the MIT license, copyright (c) 2026 joshuah.rainstar@gmail.com. Anyone may use, study, modify, and share this program, including commercially. Preserve its copyright and license notice.

Third-party libraries and fonts retain their permissive licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and `packaging/licenses/`. The application uses system fonts when available and a bundled open font as fallback.

Rainstar Paint is an independent implementation. It does not contain Microsoft's Paint source code or copied icon assets. Microsoft and Windows are trademarks of Microsoft Corporation.
