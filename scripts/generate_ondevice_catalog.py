#!/usr/bin/env python3
"""
Builds the two tiny manifests CydOs itself fetches over WiFi in the on-
device App Store's Browse tab, plus the files they point at:

  docs/ondevice_apps.txt       - slug|name|/sdapps/<slug>.cydapp, one per
                                  sd-apps/<slug>/ folder.
  docs/sdapps/<slug>.cydapp    - copied straight from sd-apps/<slug>/.

  docs/ondevice_wallpapers.txt - slug|name|/wallpapers_bmp/<slug>.bmp, one
                                  per entry in docs/wallpapers.json.
  docs/wallpapers_bmp/<slug>.bmp - the same image, cover-fit cropped to
                                  exactly 320x214 and encoded as an
                                  uncompressed 24-bit BMP (same format/size
                                  src/core/Wallpaper.h expects) - so the
                                  device can write the downloaded bytes
                                  straight to the SD card with no on-device
                                  image decoding at all.

Both manifests are plain pipe-delimited text, not JSON - the device parses
them with the same ad-hoc line/field splitting every other on-device
network response already uses (see ObsApp/SpotifyApp's extractJson*
helpers), not a JSON library.

Run with no arguments from the repo root:
    python3 scripts/generate_ondevice_catalog.py
Requires Pillow (`pip install pillow`) for the wallpaper BMP conversion.
"""
import json
import re
import shutil
import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SD_APPS_SRC = ROOT / "sd-apps"
WALLPAPERS_JSON = ROOT / "docs" / "wallpapers.json"
WALLPAPERS_DIR = ROOT / "docs" / "wallpapers"
OUT_SDAPPS_DIR = ROOT / "docs" / "sdapps"
OUT_WALLPAPERS_BMP_DIR = ROOT / "docs" / "wallpapers_bmp"
OUT_APPS_MANIFEST = ROOT / "docs" / "ondevice_apps.txt"
OUT_WALLPAPERS_MANIFEST = ROOT / "docs" / "ondevice_wallpapers.txt"

SLUG_RE = re.compile(r"^[a-z0-9_][a-z0-9_-]*$")
WP_W, WP_H = 320, 214  # Cfg::SCREEN_W x (Cfg::SCREEN_H - Cfg::STATUS_BAR_H)


def fail(msg):
    print(f"generate_ondevice_catalog.py: error: {msg}", file=sys.stderr)
    sys.exit(1)


def slugify(name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return slug or "item"


def bmp24_from_image(img: Image.Image) -> bytes:
    img = img.convert("RGB")
    sw, sh = img.size
    scale = max(WP_W / sw, WP_H / sh)
    rw, rh = round(sw * scale), round(sh * scale)
    img = img.resize((rw, rh), Image.LANCZOS)
    x0, y0 = (rw - WP_W) // 2, (rh - WP_H) // 2
    img = img.crop((x0, y0, x0 + WP_W, y0 + WP_H))

    row_bytes = (WP_W * 3 + 3) & ~3
    pixel_data_size = row_bytes * WP_H
    file_size = 54 + pixel_data_size

    header = b"BM" + struct.pack("<IHHI", file_size, 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, WP_W, WP_H, 1, 24, 0, pixel_data_size, 2835, 2835, 0, 0)

    px = img.load()
    pad = row_bytes - WP_W * 3
    rows = bytearray()
    for y in range(WP_H - 1, -1, -1):
        for x in range(WP_W):
            r, g, b = px[x, y]
            rows += bytes((b, g, r))
        rows += bytes(pad)
    return header + dib + bytes(rows)


def build_sdapps_manifest():
    lines = []
    if OUT_SDAPPS_DIR.exists():
        shutil.rmtree(OUT_SDAPPS_DIR)
    OUT_SDAPPS_DIR.mkdir(parents=True, exist_ok=True)

    if not SD_APPS_SRC.is_dir():
        OUT_APPS_MANIFEST.write_text("")
        return

    for app_dir in sorted(p for p in SD_APPS_SRC.iterdir() if p.is_dir()):
        slug = app_dir.name
        if not SLUG_RE.match(slug):
            fail(f"sd-apps/{slug}: folder name must be lowercase letters, digits, '-' or '_'")
        manifest_path = app_dir / "app.json"
        if not manifest_path.is_file():
            fail(f"sd-apps/{slug}: missing app.json")
        data = json.loads(manifest_path.read_text())
        for field in ("name", "file"):
            if not isinstance(data.get(field), str) or not data[field].strip():
                fail(f"sd-apps/{slug}/app.json: \"{field}\" must be a non-empty string")
        src_file = app_dir / data["file"]
        if not src_file.is_file():
            fail(f"sd-apps/{slug}/app.json: file \"{data['file']}\" doesn't exist")
        if not data["file"].endswith(".cydapp"):
            fail(f"sd-apps/{slug}/app.json: \"file\" must end in .cydapp")

        dest = OUT_SDAPPS_DIR / f"{slug}.cydapp"
        shutil.copy2(src_file, dest)
        # name|slug can't contain '|' - the on-device parser splits on it.
        if "|" in data["name"]:
            fail(f"sd-apps/{slug}/app.json: \"name\" can't contain '|'")
        lines.append(f"{slug}|{data['name']}|/sdapps/{slug}.cydapp")

    OUT_APPS_MANIFEST.write_text("\n".join(lines) + ("\n" if lines else ""))


def build_wallpapers_manifest():
    if OUT_WALLPAPERS_BMP_DIR.exists():
        shutil.rmtree(OUT_WALLPAPERS_BMP_DIR)
    OUT_WALLPAPERS_BMP_DIR.mkdir(parents=True, exist_ok=True)

    entries = json.loads(WALLPAPERS_JSON.read_text()) if WALLPAPERS_JSON.is_file() else []
    lines = []
    for e in entries:
        name = e.get("name", "")
        image = e.get("image", "")
        if not name or not image:
            continue
        slug = slugify(name)
        src = ROOT / "docs" / image
        if not src.is_file():
            fail(f"wallpapers.json entry \"{name}\": image \"{image}\" doesn't exist")
        bmp = bmp24_from_image(Image.open(src))
        (OUT_WALLPAPERS_BMP_DIR / f"{slug}.bmp").write_bytes(bmp)
        if "|" in name:
            fail(f"wallpapers.json entry \"{name}\": name can't contain '|'")
        lines.append(f"{slug}|{name}|/wallpapers_bmp/{slug}.bmp")

    OUT_WALLPAPERS_MANIFEST.write_text("\n".join(lines) + ("\n" if lines else ""))


def main():
    build_sdapps_manifest()
    build_wallpapers_manifest()
    print(f"generate_ondevice_catalog.py: wrote {OUT_APPS_MANIFEST.relative_to(ROOT)} "
          f"and {OUT_WALLPAPERS_MANIFEST.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
