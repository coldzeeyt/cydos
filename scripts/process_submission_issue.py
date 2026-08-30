#!/usr/bin/env python3
"""
Turns a GitHub issue opened by the App Store's submission forms
(labels: wallpaper-submission or app-submission) into the actual files a
PR needs. Run inside GitHub Actions (see
.github/workflows/process-submissions.yml), which has normal internet
access to fetch the attached image - this session's own sandbox does
not, which is why this exists as a script the workflow runs rather than
something done by hand here.

Usage:
  python3 scripts/process_submission_issue.py wallpaper --body-file body.txt --out-dir .
  python3 scripts/process_submission_issue.py app       --body-file body.txt --out-dir .

Prints a one-line JSON summary to stdout: {"slug", "name", "files": [...]}
so the workflow can build a branch name / PR title / commit message from
it without re-parsing anything itself.
"""
import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path


def slugify(name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return slug or "submission"


def extract_image_url(body: str):
    # The generator's own markdown drop, wherever the submitter's cursor
    # happened to land in the text (GitHub inserts it verbatim at that
    # point, mid-sentence and all - don't assume it's on its own line).
    m = re.search(r"!\[[^\]]*\]\((https://\S+?)\)", body)
    if m:
        return m.group(1)
    # Newer GitHub attachment UI sometimes drops an <img>/<picture> tag instead.
    m = re.search(r'<img[^>]+src="([^"]+)"', body)
    if m:
        return m.group(1)
    return None


def extract_field(body: str, label: str):
    m = re.search(rf"-\s*{label}:\s*(.+)", body)
    return m.group(1).strip() if m else None


def fail(msg: str):
    print(f"process_submission_issue.py: error: {msg}", file=sys.stderr)
    sys.exit(1)


def process_wallpaper(body: str, out_dir: Path) -> dict:
    name = extract_field(body, "Name")
    author = extract_field(body, "Author")
    description = extract_field(body, "Description") or ""
    if not name or not author:
        fail("missing \"- Name:\" or \"- Author:\" line in the issue body")

    # The image markdown can land mid-sentence in the trailing instructional
    # line if that's where the submitter's cursor was - strip anything from
    # that fixed sentence onward if it leaked into the Description capture.
    description = re.split(r"\*\*Attach", description)[0].strip()
    if description.lower() in ("(none)", "none", ""):
        description = ""

    image_url = extract_image_url(body)
    if not image_url:
        fail("no attached image found in the issue body")

    slug = slugify(name)
    req = urllib.request.Request(image_url, headers={"User-Agent": "cydos-bot"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = resp.read()
        content_type = resp.headers.get("Content-Type", "")

    ext = {
        "image/png": "png", "image/jpeg": "jpg", "image/bmp": "bmp",
        "image/gif": "gif", "image/webp": "webp",
    }.get(content_type.split(";")[0].strip(), "png")

    wallpapers_dir = out_dir / "docs" / "wallpapers"
    wallpapers_dir.mkdir(parents=True, exist_ok=True)
    image_path = wallpapers_dir / f"{slug}.{ext}"
    image_path.write_bytes(data)

    store_json_path = out_dir / "docs" / "wallpapers.json"
    entries = json.loads(store_json_path.read_text()) if store_json_path.exists() else []
    entries = [e for e in entries if not (e.get("name") == name and e.get("author") == author)]
    entries.append({
        "name": name,
        "author": author,
        "description": description,
        "image": f"wallpapers/{slug}.{ext}",
    })
    store_json_path.write_text(json.dumps(entries, indent=2) + "\n")

    return {"slug": slug, "name": name, "kind": "wallpaper",
             "files": [str(image_path), str(store_json_path)]}


def process_app(body: str, out_dir: Path) -> dict:
    m = re.search(r"```json\s*\n(.*?)\n```", body, re.S)
    if not m:
        fail("no ```json app.json block found in the issue body")
    try:
        app_data = json.loads(m.group(1))
    except json.JSONDecodeError as e:
        fail(f"app.json block isn't valid JSON ({e})")

    m2 = re.search(r"~~~cpp\s*\n(.*?)\n~~~", body, re.S)
    if not m2:
        fail("no ~~~cpp code block found in the issue body")
    code = m2.group(1)

    name = app_data.get("name")
    header = app_data.get("header")
    if not name or not header:
        fail("app.json is missing \"name\" or \"header\"")
    slug = slugify(name)

    app_dir = out_dir / "community-apps" / slug
    app_dir.mkdir(parents=True, exist_ok=True)
    (app_dir / "app.json").write_text(json.dumps(app_data, indent=2) + "\n")
    (app_dir / header).write_text(code if code.endswith("\n") else code + "\n")

    return {"slug": slug, "name": name, "kind": "app",
             "files": [str(app_dir / "app.json"), str(app_dir / header)]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kind", choices=["wallpaper", "app"])
    ap.add_argument("--body-file", required=True)
    ap.add_argument("--out-dir", default=".")
    args = ap.parse_args()

    body = Path(args.body_file).read_text()
    out_dir = Path(args.out_dir)
    result = process_wallpaper(body, out_dir) if args.kind == "wallpaper" else process_app(body, out_dir)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
