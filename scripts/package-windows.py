#!/usr/bin/env python3
"""Package the GUI.Forms application and its verified PE import closure."""
from pathlib import Path
from release_version import project_version
from font_pack import copy_fonts
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SYSTEM = {"kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "comdlg32.dll",
          "ole32.dll", "oleaut32.dll", "comctl32.dll", "shell32.dll", "shlwapi.dll",
          "usp10.dll", "windowscodecs.dll", "ws2_32.dll", "version.dll", "imm32.dll",
          "dwmapi.dll", "ntdll.dll", "msvcrt.dll", "ucrtbase.dll", "bcrypt.dll",
          "secur32.dll", "rpcrt4.dll", "setupapi.dll", "winspool.drv", "winmm.dll",
          "crypt32.dll", "normaliz.dll", "api-ms-win-core-synch-l1-2-0.dll"}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, default=ROOT / "build-forms-windows")
    parser.add_argument("--gui-forms-sdk", type=Path, required=True)
    parser.add_argument("--runtime-dir", type=Path, action="append", default=[])
    parser.add_argument("--objdump", default="objdump")
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--version", default=project_version())
    args = parser.parse_args()
    bundle = args.output / "RainstarPaint"
    if bundle.exists(): shutil.rmtree(bundle)
    bundle.mkdir(parents=True)
    executable = bundle / "rainstar-paint.exe"
    shutil.copy2(args.build / "rainstar-paint-forms.exe", executable)
    directories = [args.build, args.gui_forms_sdk / "bin"] + args.runtime_dir
    candidates = {}
    for directory in directories:
        if directory.is_dir():
            for path in directory.iterdir():
                if path.is_file() and path.suffix.lower() == ".dll": candidates.setdefault(path.name.lower(), path)
    pending = [executable]
    inspected = set()
    while pending:
        binary = pending.pop()
        if binary.name.lower() in inspected: continue
        inspected.add(binary.name.lower())
        output = subprocess.check_output([args.objdump, "-p", str(binary)], text=True)
        for name in re.findall(r"DLL Name:\s*(\S+)", output):
            key = name.lower()
            if key in SYSTEM or key.startswith(("api-ms-win-", "ext-ms-win-")): continue
            if key not in candidates: raise RuntimeError("Missing runtime import: " + name)
            destination = bundle / name
            if not destination.exists(): shutil.copy2(candidates[key], destination)
            pending.append(destination)
    copy_fonts(args.gui_forms_sdk, bundle / "fonts")
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"): shutil.copy2(ROOT / name, bundle)
    shutil.copy2(ROOT / "packaging/README.txt", bundle / "README.txt")
    shutil.copytree(ROOT / "packaging/licenses", bundle / "licenses")
    shutil.copytree(args.gui_forms_sdk / "share/licenses/GUIForms", bundle / "licenses/GUIForms")
    manifest = {"version": args.version, "architecture": "x64", "frontend": "GUI.Forms",
                "files": {str(path.relative_to(bundle)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in sorted(bundle.rglob("*")) if path.is_file()}}
    (bundle / "package-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive = args.output / f"rainstar-paint-{args.version}-windows-x64.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for path in sorted(bundle.rglob("*")):
            if path.is_file(): output.write(path, "RainstarPaint/" + path.relative_to(bundle).as_posix())
    print(archive)

if __name__ == "__main__": main()
