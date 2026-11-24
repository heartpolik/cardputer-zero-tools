#!/usr/bin/env python3
"""Pre-check an app against the CardputerZero Store rules before `czdev publish`.

usage: store-check.py <app-dir> <deb>

Runs the registry's own policy scripts (fetched from CardputerZero/packages):
  * store_meta_policy.py   - listing fields, icon/screenshot sizes, title/share_code uniqueness
  * install_path_policy.py - where the .deb may install files
and checks that app-builder.json, the .deb and the CMake version agree.
Exit code 1 when anything blocks publishing.
"""
import json
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

REGISTRY = "https://github.com/CardputerZero/packages"
RAW = "https://raw.githubusercontent.com/CardputerZero/packages/main/.github/scripts/"


def fetch(name: str, dest: Path) -> Path:
    path = dest / name
    with urllib.request.urlopen(RAW + name, timeout=30) as r:
        path.write_bytes(r.read())
    return path


def deb_field(deb: Path, field: str) -> str:
    return subprocess.run(["dpkg-deb", "-f", str(deb), field], check=True,
                          capture_output=True, text=True).stdout.strip()


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    app_dir, deb = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    manifest = json.loads((app_dir / "app-builder.json").read_text())
    store = manifest.get("store") or {}
    problems = []

    # --- versions: app-builder.json == .deb == CMakeLists.txt
    pkg = deb_field(deb, "Package")
    deb_version = deb_field(deb, "Version").rsplit("-", 1)[0]
    cmake = (app_dir / "CMakeLists.txt").read_text()
    m = re.search(r"set\(\w+_VERSION ([0-9][0-9.]*)\)", cmake)
    cmake_version = m.group(1) if m else None
    if manifest.get("package_name") != pkg:
        problems.append(f"package_name {manifest.get('package_name')!r} != .deb Package {pkg!r}")
    if len({manifest.get("version"), deb_version, cmake_version}) != 1:
        problems.append(f"version mismatch: app-builder.json {manifest.get('version')}, "
                        f".deb {deb_version}, CMakeLists.txt {cmake_version}")

    work = Path(tempfile.mkdtemp(prefix="cz-store-check-"))
    try:
        meta_policy = fetch("store_meta_policy.py", work)
        path_policy = fetch("install_path_policy.py", work)

        # --- stage pool/main/<pkg>/ the way czdev does
        pkg_dir = work / "stage" / pkg
        (pkg_dir / "screenshots").mkdir(parents=True)
        meta = {"title": manifest.get("app_name") or pkg}
        for key in ("summary", "description", "categories", "license", "source_repo",
                    "author", "share_code", "permissions", "locales"):
            if key in store:
                meta[key] = store[key]
        if store.get("icon"):
            src = app_dir / store["icon"]
            if src.exists():
                shutil.copy2(src, pkg_dir / src.name)
            meta["icon"] = src.name
        meta["screenshots"] = []
        for shot in store.get("screenshots") or []:
            src = app_dir / shot
            if src.exists():
                shutil.copy2(src, pkg_dir / "screenshots" / src.name)
            meta["screenshots"].append(f"screenshots/{src.name}")
        (pkg_dir / "meta.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2))

        # --- registry checkout for uniqueness (meta.json files only), minus our own entry
        pool = work / "registry"
        subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none", "--sparse",
                        REGISTRY, str(pool)], check=True)
        subprocess.run(["git", "-C", str(pool), "sparse-checkout", "set", "--no-cone",
                        "/pool/main/*/meta.json"], check=True)
        shutil.rmtree(pool / "pool" / "main" / pkg, ignore_errors=True)

        errors, warnings = work / "errors.txt", work / "warnings.txt"
        env = {"DEB_MAINTAINER": deb_field(deb, "Maintainer"), "PATH": "/usr/bin:/bin"}
        subprocess.run([sys.executable, str(meta_policy), pkg, str(pkg_dir),
                        str(pool / "pool" / "main"), str(errors), str(warnings)], check=True, env=env)
        bad_paths = work / "paths.txt"
        subprocess.run([sys.executable, str(path_policy), pkg, str(deb), str(bad_paths)], check=True)

        for f in (errors, bad_paths):
            if f.exists():
                problems += [line for line in f.read_text().splitlines() if line.strip()]
        if warnings.exists():
            for line in warnings.read_text().splitlines():
                print(f"warning: {line}")
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if problems:
        for p in problems:
            print(f"BLOCKER: {p}")
        return 1
    print(f"store check passed: {pkg} {deb_version} ({len(store.get('screenshots') or [])} screenshots)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
