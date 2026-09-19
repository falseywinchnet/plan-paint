#!/usr/bin/env python3
"""Fetch the pinned native toolkit source for an explicit source build."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

root = Path(__file__).resolve().parents[1]
lock = json.loads((root / "third_party/gui-forms.lock.json").read_text())
parser = argparse.ArgumentParser()
parser.add_argument("--destination", type=Path, default=root / "build-deps/gui-forms")
parser.add_argument("--archive", type=Path)
parser.add_argument("--github-cli", action="store_true", help="Use authenticated gh for a draft release in CI")
args = parser.parse_args()
if args.destination.exists():
    raise SystemExit("Destination exists; supply a fresh toolkit build directory.")
with tempfile.TemporaryDirectory(prefix="rainstar-toolkit-") as temporary:
    archive = args.archive
    if archive is None:
        archive = Path(temporary) / lock["asset"]
        if args.github_cli:
            subprocess.run(["gh", "release", "download", lock["release"], "--repo", lock["repository"], "--pattern", lock["asset"], "--dir", temporary], check=True)
        else:
            url = "https://github.com/" + lock["repository"] + "/releases/download/" + lock["release"] + "/" + lock["asset"]
            with urllib.request.urlopen(url, timeout=120) as response, archive.open("wb") as output:
                shutil.copyfileobj(response, output)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != lock["sha256"]:
        raise SystemExit("GUI.Forms source checksum mismatch.")
    with tarfile.open(archive) as source:
        members = source.getmembers()
        for member in members:
            parts = Path(member.name).parts
            if not parts or parts[0] != "gui-forms" or ".." in parts or not member.isfile():
                raise SystemExit("Unexpected entry in the toolkit source archive.")
        extracted = Path(temporary) / "source"
        extracted.mkdir()
        for member in members:
            target = extracted.joinpath(*Path(member.name).parts[1:])
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.extractfile(member) as data, target.open("wb") as output:
                shutil.copyfileobj(data, output)
            target.chmod(member.mode & 0o777)
    applied = []
    for patch in lock.get("patches", []):
        patch_path = root / patch["file"]
        content = patch_path.read_bytes()
        if hashlib.sha256(content).hexdigest() != patch["sha256"]:
            raise SystemExit("GUI.Forms source patch checksum mismatch: " + patch["file"])
        subprocess.run(["git", "apply", "--check", "-"], input=content, cwd=extracted, check=True)
        subprocess.run(["git", "apply", "-"], input=content, cwd=extracted, check=True)
        applied.append(patch)
    if applied:
        (extracted / "SOURCE_PATCHES.json").write_text(json.dumps(applied, indent=2) + "\n")
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(extracted), str(args.destination))
print(args.destination)
