#!/bin/sh
set -eu
# Check cursor ownership before the full toolkit build so native failures carry
# focused diagnostics without waiting for all application targets.
g++ -std=c++20 -Ibuild-deps/gui-forms/include \
  build-deps/gui-forms/tests/windows_cursor_tests.cpp \
  build-deps/gui-forms/src/core/types/cursor_image/cursor_image.cpp \
  -luser32 -lgdi32 -o build-deps/windows-cursor-check.exe
build-deps/windows-cursor-check.exe
cmake -S build-deps/gui-forms -B build-deps/gui-forms-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DGUI_FORMS_ENABLE_WINDOWS_HOST=ON \
  -DGUI_FORMS_ENABLE_SKIA=OFF -DGUI_FORMS_ENABLE_HARFBUZZ_TEXT=OFF \
  -DGUI_FORMS_BUILD_GALLERY=OFF -DGUI_FORMS_BUILD_TESTS=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/build-deps/gui-forms-sdk"
cmake --build build-deps/gui-forms-build --parallel 4
ctest --test-dir build-deps/gui-forms-build --output-on-failure
cmake --install build-deps/gui-forms-build
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRAINSTAR_LEGACY_UI=OFF -DRAINSTAR_GUI_FORMS=ON -DRAINSTAR_BUNDLED_TIFF=ON \
  -DGUIForms_DIR="$PWD/build-deps/gui-forms-sdk/lib/cmake/GUIForms" \
  -DCMAKE_PREFIX_PATH="$(cygpath -m /mingw64)"
cmake --build build-windows --parallel 4
ctest --test-dir build-windows --output-on-failure
python scripts/check-style.py
python scripts/package-windows.py --build build-windows \
  --gui-forms-sdk build-deps/gui-forms-sdk --runtime-dir /mingw64/bin
