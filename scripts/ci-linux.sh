#!/bin/sh
set -eu
apk add --no-cache build-base linux-headers cmake clang lld python3 pkgconf git ca-certificates \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev tiff-dev libwebp-dev dav1d-dev zlib-dev \
  libjpeg-turbo-dev gn samurai xvfb xauth ghostscript patchelf
if ! command -v ninja >/dev/null; then ln -s /usr/bin/samu /usr/local/bin/ninja; fi
export BUILD_JOBS=${BUILD_JOBS:-2}
GUI_FORMS_SYSTEM_BUILD_TOOLS=1 sh build-deps/gui-forms/third_party/fetch_skia_cpu.sh
sh build-deps/gui-forms/third_party/fetch_text_stack.sh
sh build-deps/gui-forms/third_party/build_skia_cpu_linux.sh
cmake -S build-deps/gui-forms -B build-deps/gui-forms-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DGUI_FORMS_ENABLE_SKIA=ON -DGUI_FORMS_ENABLE_HARFBUZZ_TEXT=ON \
  -DGUI_FORMS_ENABLE_MACOS_HOST=OFF -DGUI_FORMS_ENABLE_LINUX_HOST=ON \
  -DGUI_FORMS_BUILD_GALLERY=OFF -DGUI_FORMS_BUILD_TESTS=ON \
  -DGUI_FORMS_SKIA_PREBUILT=ON \
  -DGUI_FORMS_SKIA_OUT="$PWD/build-deps/gui-forms/third_party/skia/out/gui_forms-linux-cpu" \
  -DCMAKE_INSTALL_PREFIX="$PWD/build-deps/gui-forms-sdk"
cmake --build build-deps/gui-forms-build --parallel "$BUILD_JOBS"
export DISPLAY=:99
Xvfb "$DISPLAY" -screen 0 1280x800x24 -nolisten tcp > /tmp/rainstar-xvfb.log 2>&1 &
xserver=$!
trap 'kill "$xserver" 2>/dev/null || true' EXIT
# Wait for the server socket without an unbounded startup delay.
for attempt in 1 2 3 4 5; do
  test -S /tmp/.X11-unix/X99 && break
  sleep 1
done
GUI_FORMS_FONT_DIR="$PWD/build-deps/gui-forms/assets/fonts" \
  ctest --test-dir build-deps/gui-forms-build --output-on-failure
cmake --install build-deps/gui-forms-build
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRAINSTAR_LEGACY_UI=OFF -DRAINSTAR_GUI_FORMS=ON -DRAINSTAR_BUNDLED_TIFF=ON \
  -DCMAKE_PREFIX_PATH="$PWD/build-deps/gui-forms-sdk"
cmake --build build-linux --parallel "$BUILD_JOBS"
ctest --test-dir build-linux --output-on-failure
python3 scripts/check-style.py
python3 scripts/package-linux.py --build build-linux --gui-forms-sdk build-deps/gui-forms-sdk
