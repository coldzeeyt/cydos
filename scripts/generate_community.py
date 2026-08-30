#!/usr/bin/env python3
"""
Turns community-apps/<slug>/app.json + source files into:
  - src/community_registration.inc  (registerApp()/addTile() glue, included
    from main.cpp's setup())
  - src/apps/community/<slug>/...   (copies of each app's source files, so
    PlatformIO's normal source globbing just picks them up)
  - docs/store.json                 (the App Store page's catalog)

Run with no arguments from the repo root before building the Community
Edition firmware:
    python3 scripts/generate_community.py

A folder starting with "_" (e.g. community-apps/_example/) is a reference
template: copied and compiled so it's proven to work, but left out of
store.json so it never appears as if someone submitted it.

Fails loudly (non-zero exit, message on stderr) on anything that looks
wrong, rather than silently producing a broken or unsafe build - this
runs in CI against pull requests from strangers.
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMMUNITY_SRC = ROOT / "community-apps"
GENERATED_APPS_DIR = ROOT / "src" / "apps" / "community"
REGISTRATION_INC = ROOT / "src" / "community_registration.inc"
STORE_JSON = ROOT / "docs" / "store.json"

MAX_COMMUNITY_APPS = 8  # keep real headroom under AppManager::MAX_APPS (24)
SLUG_RE = re.compile(r"^[a-z0-9_][a-z0-9_-]*$")  # leading "_" marks a template, e.g. "_example"
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
REQUIRED_STR_FIELDS = ["name", "author", "description", "icon", "class", "header"]
MAX_DESC_LEN = 140


def fail(msg):
    print(f"generate_community.py: error: {msg}", file=sys.stderr)
    sys.exit(1)


def load_app(app_dir: Path):
    slug = app_dir.name
    if not SLUG_RE.match(slug):
        fail(f"community-apps/{slug}: folder name must be lowercase letters, "
             f"digits, '-' or '_' (and not start with one of those)")

    manifest_path = app_dir / "app.json"
    if not manifest_path.is_file():
        fail(f"community-apps/{slug}: missing app.json")

    try:
        data = json.loads(manifest_path.read_text())
    except json.JSONDecodeError as e:
        fail(f"community-apps/{slug}/app.json: invalid JSON ({e})")

    for field in REQUIRED_STR_FIELDS:
        if not isinstance(data.get(field), str) or not data[field].strip():
            fail(f"community-apps/{slug}/app.json: \"{field}\" must be a non-empty string")

    if len(data["description"]) > MAX_DESC_LEN:
        fail(f"community-apps/{slug}/app.json: \"description\" must be "
             f"{MAX_DESC_LEN} characters or fewer (keep it to one sentence - "
             f"put the rest in your own repo's README)")

    if not IDENT_RE.match(data["class"]):
        fail(f"community-apps/{slug}/app.json: \"class\" must be a valid C++ "
             f"identifier (letters, digits, underscore, not starting with a digit)")

    sources = data.get("sources")
    if not isinstance(sources, list) or not sources or not all(isinstance(s, str) for s in sources):
        fail(f"community-apps/{slug}/app.json: \"sources\" must be a non-empty list of filenames")
    if data["header"] not in sources:
        fail(f"community-apps/{slug}/app.json: \"header\" must be one of the \"sources\" entries")
    for s in sources:
        if "/" in s or "\\" in s or s in ("..", ".") or s.startswith("."):
            fail(f"community-apps/{slug}/app.json: source filename \"{s}\" must be a "
                 f"plain filename in the app's own folder, not a path")
        if not (app_dir / s).is_file():
            fail(f"community-apps/{slug}/app.json: source file \"{s}\" doesn't exist")

    repo = data.get("repo", "")
    if repo and not isinstance(repo, str):
        fail(f"community-apps/{slug}/app.json: \"repo\" must be a string if present")

    data["_slug"] = slug
    data["_is_template"] = slug.startswith("_")
    return data


def main():
    if not COMMUNITY_SRC.is_dir():
        apps = []
    else:
        app_dirs = sorted(p for p in COMMUNITY_SRC.iterdir() if p.is_dir())
        apps = [load_app(d) for d in app_dirs]

    real_apps = [a for a in apps if not a["_is_template"]]
    if len(real_apps) > MAX_COMMUNITY_APPS:
        fail(f"{len(real_apps)} community apps found, but the cap is "
             f"{MAX_COMMUNITY_APPS} (see MAX_COMMUNITY_APPS in this script "
             f"and AppManager::MAX_APPS) - split into a Community Edition v2 "
             f"or raise the cap deliberately, don't just bump this silently")

    slugs_seen = set()
    classes_seen = set()
    for a in apps:
        if a["_slug"] in slugs_seen:
            fail(f"duplicate slug: {a['_slug']}")
        slugs_seen.add(a["_slug"])
        if a["class"] in classes_seen:
            fail(f"duplicate class name across apps: {a['class']} - class names "
                 f"must be unique since they become distinct C++ types")
        classes_seen.add(a["class"])

    if GENERATED_APPS_DIR.exists():
        shutil.rmtree(GENERATED_APPS_DIR)
    GENERATED_APPS_DIR.mkdir(parents=True, exist_ok=True)

    includes = []
    blocks = []
    for i, app in enumerate(apps):
        slug = app["_slug"]
        dest_dir = GENERATED_APPS_DIR / slug
        dest_dir.mkdir(parents=True, exist_ok=True)
        for src_name in app["sources"]:
            shutil.copy2(COMMUNITY_SRC / slug / src_name, dest_dir / src_name)

        includes.append(f'#include "apps/community/{slug}/{app["header"]}"')
        blocks.append(
            "{{\n"
            "  static {cls} communityApp{i};\n"
            "  uint8_t communityIdx{i} = appManager.registerApp(&communityApp{i});\n"
            "  homeApp.addTile(UI::iconPuzzle, communityIdx{i});\n"
            "}}".format(cls=app["class"], i=i)
        )

    lines = [
        "// Auto-generated by scripts/generate_community.py - do not edit by hand.",
        f"// {len(apps)} community app(s) ({len(real_apps)} real, "
        f"{len(apps) - len(real_apps)} template).",
    ]
    lines.extend(includes)
    lines.append("")
    lines.extend(blocks)
    REGISTRATION_INC.write_text("\n".join(lines) + "\n")

    store_entries = [
        {
            "name": a["name"],
            "author": a["author"],
            "description": a["description"],
            "icon": a["icon"],
            "repo": a.get("repo", ""),
        }
        for a in real_apps
    ]
    STORE_JSON.parent.mkdir(parents=True, exist_ok=True)
    STORE_JSON.write_text(json.dumps(store_entries, indent=2) + "\n")

    print(f"generate_community.py: wrote {REGISTRATION_INC.relative_to(ROOT)} "
          f"and {STORE_JSON.relative_to(ROOT)} ({len(apps)} app(s), "
          f"{len(real_apps)} listed in the store)")


if __name__ == "__main__":
    main()
