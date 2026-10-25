"""Assemble the GitHub Pages site: web flasher page, firmware images and the
manifest used by both ESP Web Tools (first install) and the boards'
http_request update entity (self-updates).

usage: build_site.py OUT_DIR VERSION NAME:CHIP_FAMILY...
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
    out, version, boards = Path(sys.argv[1]), sys.argv[2], sys.argv[3:]
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy("web/index.html", out)
    builds = []
    for board in boards:
        name, chip = board.split(":")
        factory, ota = f"{name}.factory.bin", f"{name}.ota.bin"
        shutil.copy(find_image(name, "factory"), out / factory)
        shutil.copy(find_image(name, "ota"), out / ota)
        builds.append({
            "chipFamily": chip,
            "parts": [{"path": factory, "offset": 0}],
            "ota": {
                "path": ota,
                "md5": hashlib.md5((out / ota).read_bytes()).hexdigest(),
                "summary": f"ATC collector {version}",
                "release_url": f"{REPO_URL}/commit/{version}",
            },
        })
    manifest = {
        "name": "ATC Collector",
        "version": version,
        # Lets ESP Web Tools offer adding the device to Home Assistant.
        "home_assistant_domain": "esphome",
        "new_install_prompt_erase": True,
        "builds": builds,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
