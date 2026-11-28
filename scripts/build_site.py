"""Assemble the GitHub Pages site: web flasher page, firmware images and the
manifests used by both ESP Web Tools (first install) and the boards'
http_request update entity (self-updates).

Each board gets its own NAME.manifest.json: two different boards with the
same chip family can't share one. VERSION is the board's firmware version
(what the update entity compares against). Boards marked :legacy are also
listed in the old shared manifest.json, which boards flashed before the
per-board manifests still poll; it carries the first legacy board's version,
so the other legacy board is offered one update that moves it to its own
manifest.

usage: build_site.py OUT_DIR COMMIT NAME:CHIP_FAMILY:VERSION[:legacy]...
"""

import hashlib
import json
import shutil
import sys
from pathlib import Path

BUILD_ROOT = Path("esphome/.esphome/build")
REPO_URL = "https://github.com/hmeyer/atc_collector_esp32"


def find_image(name: str, kind: str) -> Path:
    images = sorted(BUILD_ROOT.joinpath(name).rglob(f"firmware.{kind}.bin"))
    if len(images) != 1:
        sys.exit(f"expected one firmware.{kind}.bin for {name}, found {images}")
    return images[0]


def main() -> None:
    out, commit, boards = Path(sys.argv[1]), sys.argv[2], sys.argv[3:]
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy("web/index.html", out)
    legacy_builds, legacy_version = [], None
    for board in boards:
        name, chip, version, *flags = board.split(":")
        factory, ota = f"{name}.factory.bin", f"{name}.ota.bin"
        shutil.copy(find_image(name, "factory"), out / factory)
        shutil.copy(find_image(name, "ota"), out / ota)
        build = {
            "chipFamily": chip,
            "parts": [{"path": factory, "offset": 0}],
            "ota": {
                "path": ota,
                "md5": hashlib.md5((out / ota).read_bytes()).hexdigest(),
                "summary": f"ATC collector {version} (commit {commit})",
                "release_url": f"{REPO_URL}/commit/{commit}",
            },
        }
        write_manifest(out / f"{name}.manifest.json", f"ATC Collector ({name})", version, [build])
        if "legacy" in flags:
            legacy_builds.append(build)
            legacy_version = legacy_version or version
    write_manifest(out / "manifest.json", "ATC Collector", legacy_version, legacy_builds)


def write_manifest(path: Path, title: str, version: str, builds: list) -> None:
    manifest = {
        "name": title,
        "version": version,
        # Lets ESP Web Tools offer adding the device to Home Assistant.
        "home_assistant_domain": "esphome",
        "new_install_prompt_erase": True,
        "builds": builds,
    }
    path.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
