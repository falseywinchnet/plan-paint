#!/bin/sh
set -eu
build_jobs=${BUILD_JOBS:-3}
sh build-deps/gui-forms/third_party/fetch_skia_cpu.sh
sh build-deps/gui-forms/third_party/fetch_text_stack.sh
cmake -S build-deps/gui-forms -B build-deps/gui-forms-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DGUI_FORMS_ENABLE_MACOS_HOST=ON \
  -DGUI_FORMS_ENABLE_SKIA=ON -DGUI_FORMS_ENABLE_HARFBUZZ_TEXT=ON \
  -DGUI_FORMS_BUILD_GALLERY=OFF -DGUI_FORMS_BUILD_TESTS=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/build-deps/gui-forms-sdk"
cmake --build build-deps/gui-forms-build --parallel "$build_jobs"
ctest --test-dir build-deps/gui-forms-build --output-on-failure
cmake --install build-deps/gui-forms-build
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRAINSTAR_LEGACY_UI=OFF -DRAINSTAR_GUI_FORMS=ON -DRAINSTAR_BUNDLED_TIFF=ON \
  -DGUIForms_DIR="$PWD/build-deps/gui-forms-sdk/lib/cmake/GUIForms" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build-macos --parallel "$build_jobs"
ctest --test-dir build-macos --output-on-failure
python3 scripts/check-style.py
python3 scripts/check-languages.py
python3 scripts/package-macos.py --build build-macos --gui-forms-sdk build-deps/gui-forms-sdk
# Exercise the signed, packaged app with its shipped dependency closure.
python3 - <<'PYTHON'
from pathlib import Path
import subprocess
bundle = Path("dist/Plan Paint.app").resolve()
binary = bundle / "Contents/MacOS/plan-paint"
process = subprocess.Popen([str(binary), "--language=ru-ru"], cwd=bundle)
try:
    result = process.wait(timeout=5)
except subprocess.TimeoutExpired:
    process.terminate()
    process.wait(timeout=10)
    print("Packaged Russian macOS startup passed")
else:
    raise SystemExit(f"Packaged macOS application exited early: {result}")
PYTHON
