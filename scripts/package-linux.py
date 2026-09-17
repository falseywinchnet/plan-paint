#!/usr/bin/env python3
"""Make a relocatable folder; keep system graphics drivers and glibc on the host."""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
SYSTEM = {"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1", "ld-linux-x86-64.so.2", "linux-vdso.so.1"}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--version", default="0.1.0")
    args = parser.parse_args()
    distribution = ROOT / "dist"
    bundle = distribution / "RainstarPaint"
    if bundle.exists():
        shutil.rmtree(bundle)
    (bundle / "bin").mkdir(parents=True)
    (bundle / "lib").mkdir()
    executable = bundle / "bin/rainstar-paint"
    shutil.copy2(ROOT / args.build / "rainstar-paint", executable)
    output = subprocess.check_output(["ldd", str(executable)], text=True)
    # GTK is the desktop printing service. Like X11/Wayland and glibc, use the host's
    # complete GTK stack rather than redistributing only part of that platform.
    desktop = set()
    for line in output.splitlines():
        match = re.search(r"(libgtk-3\.so\.0) => (/\S+)", line)
        if match:
            desktop.add(match[1])
            gtk_dependencies = subprocess.check_output(["ldd", match[2]], text=True)
            for dependency in gtk_dependencies.splitlines():
                name = dependency.strip().split(" ", 1)[0]
                desktop.add(name)
    if "not found" in output:
        raise RuntimeError(output)
    for line in output.splitlines():
        match = re.search(r"(\S+) => (/\S+)", line)
        if match and match[1] not in SYSTEM and match[1] not in desktop:
            shutil.copy2(Path(match[2]).resolve(), bundle / "lib" / match[1])
    launcher = bundle / "Rainstar Paint"
    launcher.write_text('#!/bin/sh\napp_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexport LD_LIBRARY_PATH="$app_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexec "$app_dir/bin/rainstar-paint" "$@"\n')
    launcher.chmod(0o755)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "README.md"):
        shutil.copy2(ROOT / name, bundle)
    shutil.copytree(ROOT / "packaging/licenses", bundle / "licenses", dirs_exist_ok=True)
    shutil.copy2(ROOT / "assets/fonts/PROJECT.md", bundle / "licenses/Portsmouth-PROJECT.md")
    archive = distribution / f"rainstar-paint-{args.version}-linux-x64.tar.gz"
    with tarfile.open(archive, "w:gz") as output_file:
        output_file.add(bundle, arcname="RainstarPaint")
    print(archive)

if __name__ == "__main__":
    main()
