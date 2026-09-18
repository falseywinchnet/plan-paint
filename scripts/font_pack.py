"""Copy Paint's explicit font selection, including all required font notices."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]

def copy_fonts(sdk, destination):
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name in (ROOT / "packaging/fonts.txt").read_text().splitlines():
        shutil.copy2(sdk / "share/GUIForms/fonts" / name, destination / name)
    for source in (ROOT / "assets/fonts/cairo-unicode").iterdir():
        if source.is_file():
            shutil.copy2(source, destination / source.name)
