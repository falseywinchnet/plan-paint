#!/usr/bin/env python3
"""Bundle the exact Mach-O dependency closure, sign ad hoc, and build an installer."""
from pathlib import Path
from release_version import project_version
import argparse
import json
import plistlib
import shutil
import re
import hashlib
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def run(arguments):
    return subprocess.check_output(arguments, text=True).strip()

def dependencies(path):
    lines = run(["otool", "-L", str(path)]).splitlines()[1:]
    return [line.strip().split(" (", 1)[0] for line in lines]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build")
    parser.add_argument("--frontend", choices=("legacy", "forms"), default="legacy")
    parser.add_argument("--output")
    parser.add_argument("--gui-forms-sdk", type=Path)
    parser.add_argument("--version", default=project_version())
    args = parser.parse_args()
    forms = args.frontend == "forms"
    build = args.build or ("build-forms" if forms else "build")
    distribution = ROOT / (args.output or ("dist/gui-forms" if forms else "dist"))
    distribution.mkdir(parents=True, exist_ok=True)
    app = distribution / ("Rainstar Paint GUI.Forms.app" if forms else "Rainstar Paint.app")
    if app.exists():
        shutil.rmtree(app)
    binary_name = "rainstar-paint-forms" if forms else "rainstar-paint"
    source_app = ROOT / build / (binary_name + ".app")
    shutil.copytree(source_app, app)
    executable = app / "Contents/MacOS" / binary_name
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
    notices = resources / "licenses"
    if (ROOT / "packaging/licenses").exists():
        shutil.copytree(ROOT / "packaging/licenses", notices, dirs_exist_ok=True)
    if forms:
        if not args.gui_forms_sdk:
            raise RuntimeError("The GUI.Forms package requires --gui-forms-sdk for license provenance.")
        sdk = args.gui_forms_sdk.resolve()
        # The installed dependency and its license accompany the standalone app.
        license_candidates = [sdk / "share/licenses/GUIForms/LICENSE", sdk / "LICENSE"]
        toolkit_license = next((path for path in license_candidates if path.exists()), None)
        if toolkit_license is None:
            raise RuntimeError("GUI.Forms SDK does not contain its license.")
        notices.mkdir(exist_ok=True)
        shutil.copy2(toolkit_license, notices / "GUIForms-LICENSE.txt")
        sdk_notices = sdk / "share/licenses/GUIForms"
        if sdk_notices.is_dir():
            shutil.copytree(sdk_notices, notices / "GUIForms", dirs_exist_ok=True)
        (resources / "README-GUIForms.txt").write_text(
            "Rainstar Paint — GUI.Forms development build\n\n"
            "This application is a local preview of the native GUI.Forms port.\n"
            "It includes its toolkit, codec libraries, and fonts.\n"
            "The signature is ad hoc, not Developer ID notarized.\n"
            "Author: Astra\nSponsor: Rainstar\n", encoding="utf-8")
    def rpaths(path):
        return re.findall(r"cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.*?) \(offset", run(["otool", "-l", str(path)]))
    source_executable = source_app / "Contents/MacOS" / binary_name
    executable_rpaths = rpaths(source_executable)
    def expand(path, owner):
        return Path(path.replace("@loader_path", str(owner.parent)).replace("@executable_path", str(source_executable.parent)))
    def resolve(dependency, owner):
        if dependency.startswith("@rpath/"):
            relative = dependency[len("@rpath/"):]
            candidates = [expand(base, owner) / relative for base in rpaths(owner)]
            candidates += [expand(base, source_executable) / relative for base in executable_rpaths]
        else:
            candidates = [expand(dependency, owner)]
        for candidate in candidates:
            if candidate.is_absolute() and candidate.is_file():
                return candidate.resolve()
        raise RuntimeError(f"Unresolved dependency {dependency} in {owner}")
    queue = [(source_executable, executable)]
    seen = set()
    bundled = []
    original_for_name = {}
    while queue:
        original_target, target = queue.pop(0)
        if target in seen:
            continue
        seen.add(target)
        identifiers = run(["otool", "-D", str(original_target)]).splitlines()[1:]
        own_id = identifiers[0] if identifiers else None
        for dependency in dependencies(original_target):
            if dependency == own_id or dependency.startswith(("/System/", "/usr/lib/")):
                continue
            original = resolve(dependency, original_target)
            # Keep the referenced basename so versioned names retain their ABI identity.
            name = Path(dependency).name
            destination = frameworks / name
            if name in original_for_name and original_for_name[name] != original:
                if hashlib.sha256(original.read_bytes()).digest() != hashlib.sha256(original_for_name[name].read_bytes()).digest():
                    raise RuntimeError(f"Different dependencies share the same basename: {name}")
            if not destination.exists():
                shutil.copy2(original, destination)
                destination.chmod(0o755)
                run(["install_name_tool", "-id", "@rpath/" + destination.name, str(destination)])
                original_for_name[name] = original
                queue.append((original, destination))
                bundled.append({"name": destination.name, "source": str(original)})
            replacement = ("@executable_path/../Frameworks/" if target == executable else "@loader_path/") + destination.name
            run(["install_name_tool", "-change", dependency, replacement, str(target)])
        for search_path in rpaths(target):
            if search_path.startswith(("/opt/", "/Users/", "/usr/local/")):
                run(["install_name_tool", "-delete_rpath", search_path, str(target)])
    for target in sorted(seen):
        identifiers = run(["otool", "-D", str(target)]).splitlines()[1:]
        own_id = identifiers[0] if identifiers else None
        for dependency in dependencies(target):
            if dependency == own_id or dependency.startswith(("/System/", "/usr/lib/")):
                continue
            if dependency.startswith("@executable_path/../Frameworks/"):
                resolved = executable.parent / dependency.replace("@executable_path/", "")
            elif dependency.startswith("@loader_path/"):
                resolved = target.parent / dependency.replace("@loader_path/", "")
            else:
                raise RuntimeError(f"Nonportable dependency remains: {dependency}")
            if not resolved.is_file():
                raise RuntimeError(f"Bundled dependency is missing: {resolved}")
    for library in frameworks.iterdir():
        run(["codesign", "--force", "--sign", "-", str(library)])
    run(["codesign", "--force", "--deep", "--sign", "-", str(app)])
    run(["codesign", "--verify", "--deep", "--strict", str(app)])
    architecture = run(["uname", "-m"])
    package_name = "rainstar-paint-gui-forms" if forms else "rainstar-paint"
    package = distribution / f"{package_name}-{args.version}-macos-{architecture}.pkg"
    run(["pkgbuild", "--component", str(app), "--install-location", "/Applications", "--identifier", "org.rainstar.paint.forms" if forms else "org.rainstar.paint", "--version", args.version, str(package)])
    (distribution / "macos-bundle-receipt.json").write_text(json.dumps({"package": package.name, "architecture": architecture, "dependencies": bundled, "signature": "ad-hoc; not Developer ID notarized"}, indent=2) + "\n")
    print(package)

if __name__ == "__main__":
    main()
