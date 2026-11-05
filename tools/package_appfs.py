#!/usr/bin/env python3
"""Build a complete CHRONOS AppFS package and optionally sync its repository folder."""

import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile
import wave

from check_release import inspect_build, sha256

APP_SLUG = "com.github.siviqt.chronos"
SOUNDS = {f"alarm{i}.wav": f"TF{number}.WAV"
          for i, number in enumerate(("022", "023", "034", "048", "050"), 1)}


def verify_documentation(package, names):
    for name in names:
        for link in re.findall(r"\]\(([^)]+)\)", (package / name).read_text()):
            target = link.split("#", 1)[0]
            if target and "://" not in target and not (package / target).is_file():
                raise ValueError(f"Broken package documentation link: {target}")


def verify_package(package):
    metadata = json.loads((package / "metadata.json").read_text())
    app = metadata["application"][0]
    required = [app["executable"], metadata["license_file"], "THIRD_PARTY_LICENSES.txt",
                "README.md", "external-display.md", "ACCEPTANCE.md", "BUILD_INFO.json",
                *metadata["icon"].values()]
    for asset in app["assets"]:
        if asset["source_file"] != asset["target_file"]:
            raise ValueError("Package assets must retain their installed filenames")
        required.append(asset["source_file"])
    if {asset["source_file"] for asset in app["assets"]} != set(SOUNDS):
        raise ValueError("Metadata must declare all five bundled sounds")
    for filename in required:
        if Path(filename).name != filename or not (package / filename).is_file():
            raise ValueError(f"Missing or invalid package file: {filename}")
    verify_documentation(package, ("README.md", "external-display.md", "ACCEPTANCE.md"))
    for filename in SOUNDS:
        with wave.open(str(package / filename), "rb") as sound:
            if sound.getsampwidth() != 2 or sound.getnchannels() not in (1, 2) or not 8000 <= sound.getframerate() <= 48000:
                raise ValueError(f"Unsupported bundled WAV: {filename}")
    checksummed = set()
    for line in (package / "SHA256SUMS").read_text().splitlines():
        digest, filename = line.split("  ", 1)
        if filename in checksummed or Path(filename).name != filename or sha256(package / filename) != digest:
            raise ValueError(f"Package checksum mismatch: {filename}")
        checksummed.add(filename)
    if checksummed != {path.name for path in package.iterdir() if path.is_file() and path.name != "SHA256SUMS"}:
        raise ValueError("Checksums must cover every package file exactly once")
    info = json.loads((package / "BUILD_INFO.json").read_text())
    if info["firmware_sha256"] != sha256(package / app["executable"]):
        raise ValueError("Build information does not match the firmware")
    if (info["application_version"], info["application_revision"], info["bsp_version"]) != (
            metadata["version"], app["revision"], app["bsp_version"]):
        raise ValueError("Build information does not match package metadata")


def package_appfs(root, build):
    info = inspect_build(root, build)
    destination = build / "appfs" / APP_SLUG
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".chronos-package-", dir=destination.parent) as temporary:
        staging = Path(temporary)
        files = {"chronos.bin": build / "chronos.bin", "metadata.json": root / "assets/icons/metadata.json",
                 "README.md": root / "README.md", "external-display.md": root / "docs/external-display.md",
                 "ACCEPTANCE.md": root / "docs/ACCEPTANCE.md", "LICENSE.txt": root / "LICENSE.txt"}
        for size in (16, 32, 64):
            files[f"icon{size}.png"] = root / f"assets/icons/icon{size}.png"
        for name, source in SOUNDS.items():
            files[name] = root / "assets/sounds" / source
        licenses = {"LICENSE.ibm-plex.txt": root / "assets/fonts/LICENSE.txt",
                    "LICENSE.font-compatibility.txt": root / "assets/fonts/LICENSE.compatibility",
                    "LICENSE.timezone.txt": root / "components/chronos_app/features/LICENSE.timezone",
                    "LICENSE.esp-hal-lcd.txt": root / "components/esp_hal_lcd/LICENSE",
                    "LICENSE.badge-bsp.txt": root / "managed_components/badgeteam__badge-bsp/LICENSE",
                    "LICENSE.mipi-dsi.txt": root / "licenses/LICENSE.mipi-dsi.txt",
                    "LICENSE.pax-gfx.txt": root / "managed_components/robotman2412__pax-gfx/LICENSE",
                    "LICENSE.nokia5110.txt": root / "components/nokia5110/LICENSE"}
        files.update(licenses)
        for name, source in files.items():
            shutil.copy2(source, staging / name)
        readme = (staging / "README.md").read_text().split("## Additional information", 1)[0]
        readme = readme.replace("(docs/external-display.md)", "(external-display.md)")
        readme += ("## Additional information\n\n"
                   "- [External display wiring and setup](external-display.md)\n"
                   "- [Hardware acceptance record](ACCEPTANCE.md)\n"
                   "- [Third-party licenses](THIRD_PARTY_LICENSES.txt)\n")
        (staging / "README.md").write_text(readme)
        notices = ["THIRD-PARTY LICENSES\n\nSee the individual license files supplied with this package.\n"]
        for name in licenses:
            notices.append(f"\n{'=' * 72}\n{name}\n{'=' * 72}\n\n" + (staging / name).read_text())
        (staging / "THIRD_PARTY_LICENSES.txt").write_text("\n".join(notices))
        metadata = json.loads((staging / "metadata.json").read_text())
        info.update(application_version=metadata["version"], application_revision=metadata["application"][0]["revision"],
                    bsp_version=metadata["application"][0]["bsp_version"], package_slug=APP_SLUG)
        (staging / "BUILD_INFO.json").write_text(json.dumps(info, indent=2) + "\n")
        (staging / "SHA256SUMS").write_text("".join(f"{sha256(path)}  {path.name}\n" for path in sorted(staging.iterdir())))
        verify_package(staging)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(staging, destination)
    return destination


def sync_repository(package, destination):
    """Export repository payload and consolidated notices; keep build records local."""
    verify_package(package)
    if destination.resolve() == package.resolve():
        raise ValueError("Repository destination must differ from the full build package")
    metadata = json.loads((package / "metadata.json").read_text())
    app = metadata["application"][0]
    names = {"metadata.json", app["executable"], metadata["license_file"],
             "README.md", "external-display.md", "THIRD_PARTY_LICENSES.txt",
             *metadata["icon"].values(), *(asset["source_file"] for asset in app["assets"])}
    files = {name: (package / name).read_bytes() for name in names}
    readme = files["README.md"].decode().replace(
        "- [Hardware acceptance record](ACCEPTANCE.md)\n", "")
    files["README.md"] = readme.encode()
    display_doc = files["external-display.md"].decode().replace(
        "See [acceptance tracking](ACCEPTANCE.md).", "")
    files["external-display.md"] = ("\n".join(line.rstrip() for line in display_doc.splitlines()) + "\n").encode()
    notices = files["THIRD_PARTY_LICENSES.txt"].decode().replace(
        "See the individual license files supplied with this package.",
        "Full third-party license texts are reproduced below.")
    for license_file in package.glob("LICENSE.*.txt"):
        if license_file.read_text() not in notices:
            raise ValueError(f"Consolidated notice is missing {license_file.name}")
    files["THIRD_PARTY_LICENSES.txt"] = notices.encode()
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (destination / name).write_bytes(data)
    # Only ordinary package files are pruned; Git and host-managed hidden entries stay intact.
    for path in destination.iterdir():
        if path.is_file() and not path.name.startswith(".") and path.name not in names:
            path.unlink()
    verify_documentation(destination, ("README.md", "external-display.md"))
    if any((destination / name).read_bytes() != data for name, data in files.items()):
        raise ValueError("Repository export does not match the verified payload")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--sync-to", type=Path)
    args = parser.parse_args()
    package = package_appfs(Path(__file__).resolve().parents[1], args.build_dir.resolve())
    if args.sync_to:
        if args.sync_to.name != APP_SLUG:
            parser.error(f"Repository folder must be named {APP_SLUG}")
        sync_repository(package, args.sync_to)
        print(f"Updated app repository: {args.sync_to.resolve()}")
    print(f"Verified AppFS package: {package}")
