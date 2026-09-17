#!/usr/bin/env python3
"""Bundle the exact Mach-O dependency closure, sign ad hoc, and build an installer."""
from pathlib import Path
import argparse
import json
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def run(arguments):
    return subprocess.check_output(arguments, text=True).strip()

def dependencies(path):
    lines = run(["otool", "-L", str(path)]).splitlines()[1:]
    return [line.strip().split(" (", 1)[0] for line in lines]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--version", default="0.1.1")
    args = parser.parse_args()
    distribution = ROOT / "dist"
    distribution.mkdir(exist_ok=True)
    app = distribution / "Rainstar Paint.app"
    if app.exists():
        shutil.rmtree(app)
    shutil.copytree(ROOT / args.build / "rainstar-paint.app", app)
    executable = app / "Contents/MacOS/rainstar-paint"
    frameworks = app / "Contents/Frameworks"
    resources = app / "Contents/Resources"
    plist_path = app / "Contents/Info.plist"
    plist = plistlib.loads(plist_path.read_bytes())
    plist["CFBundleIconFile"] = "app-icon.icns"
    plist["LSMinimumSystemVersion"] = "26.0"
    plist["CFBundleDocumentTypes"] = [{"CFBundleTypeName": "Image", "CFBundleTypeRole": "Editor", "LSHandlerRank": "Alternate", "LSItemContentTypes": ["public.image"]}]
    plist_path.write_bytes(plistlib.dumps(plist))
    frameworks.mkdir(exist_ok=True)
    resources.mkdir(exist_ok=True)
    shutil.copy2(ROOT / "LICENSE", resources)
    shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", resources)
    shutil.copy2(ROOT / "assets/fonts/PROJECT.md", resources / "Portsmouth-PROJECT.md")
    notices = resources / "licenses"
    if (ROOT / "packaging/licenses").exists():
        shutil.copytree(ROOT / "packaging/licenses", notices, dirs_exist_ok=True)
    queue = [executable]
    seen = set()
    bundled = []
    while queue:
        target = queue.pop(0)
        if target in seen:
            continue
        seen.add(target)
        for dependency in dependencies(target):
            if dependency.startswith(("/System/", "/usr/lib/")):
                continue
            if dependency.startswith(("@rpath/", "@loader_path/", "@executable_path/")):
                continue
            original = Path(dependency)
            if not original.is_absolute() or not original.exists():
                raise RuntimeError(f"Unresolved dependency {dependency} in {target}")
            destination = frameworks / original.name
            if not destination.exists():
                shutil.copy2(original, destination)
                destination.chmod(0o755)
                run(["install_name_tool", "-id", "@rpath/" + destination.name, str(destination)])
                queue.append(destination)
                bundled.append({"name": destination.name, "source": str(original.resolve())})
            replacement = ("@executable_path/../Frameworks/" if target == executable else "@loader_path/") + destination.name
            run(["install_name_tool", "-change", dependency, replacement, str(target)])
    for target in sorted(seen):
        for dependency in dependencies(target):
            if dependency.startswith(("/opt/", "/Users/", "/usr/local/")):
                raise RuntimeError(f"Nonportable dependency remains: {dependency}")
    for library in frameworks.iterdir():
        run(["codesign", "--force", "--sign", "-", str(library)])
    run(["codesign", "--force", "--deep", "--sign", "-", str(app)])
    run(["codesign", "--verify", "--deep", "--strict", str(app)])
    architecture = run(["uname", "-m"])
    package = distribution / f"rainstar-paint-{args.version}-macos-{architecture}.pkg"
    run(["pkgbuild", "--component", str(app), "--install-location", "/Applications", "--identifier", "org.rainstar.paint", "--version", args.version, str(package)])
    (distribution / "macos-bundle-receipt.json").write_text(json.dumps({"package": package.name, "architecture": architecture, "dependencies": bundled, "signature": "ad-hoc; not Developer ID notarized"}, indent=2) + "\n")
    print(package)

if __name__ == "__main__":
    main()
