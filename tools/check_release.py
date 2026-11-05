#!/usr/bin/env python3
"""Verify the production configuration, dependencies and linked compatibility fixes."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect_build(root, build):
    description = json.loads((build / "project_description.json").read_text())
    metadata = json.loads((root / "assets/icons/metadata.json").read_text())
    if description["project_version"] != metadata["version"]:
        raise ValueError("Firmware project version does not match package metadata")
    if metadata["application"][0].get("bsp_version") != 2:
        raise ValueError("Package must advertise BSP major version 2")
    config = Path(description["config_file"]).read_text()
    for option in ("BSP_TARGET_TANMATSU", "MIPI_DSI_ABSTRACTION_USE_VENDORED_ESP_HAL_LCD",
                   "LCD_DSI_ISR_CACHE_SAFE"):
        if f"CONFIG_{option}=y\n" not in config:
            raise ValueError(f"Missing required configuration: CONFIG_{option}=y")
    lock = (root / "dependencies.lock").read_text()
    versions = {"badgeteam/badge-bsp": "2.0.0", "nicolaielectronics/mipi_dsi_abstraction": "2.0.0",
                "nicolaielectronics/tanmatsu-wifi": "1.3.1", "robotman2412/pax-gfx": "2.2.2", "idf": "6.0.2"}
    for name, version in versions.items():
        match = re.search(r"^  " + re.escape(name) + r":\n(.*?)(?=^  \S|\Z)", lock, re.M | re.S)
        if not match or not re.search(r"^    version: ['\"]?" + re.escape(version) + r"['\"]?$", match[1], re.M):
            raise ValueError(f"Unexpected locked dependency: {name}, expected {version}")
        if name != "idf":
            if "type: service" not in match[1]:
                raise ValueError(f"Release dependency must come from registry: {name}")
            component_hash = (root / "managed_components" / name.replace("/", "__") / ".component_hash").read_text().strip()
            if f"component_hash: {component_hash}" not in match[1]:
                raise ValueError(f"Registry hash mismatch: {name}")
    files = {Path(entry["file"]).resolve() for entry in json.loads((build / "compile_commands.json").read_text())}
    required_sources = [root / "components/esp_hal_lcd/mipi_dsi_hal.c",
                        build / "chronos_bsp_compat/badge_bsp_display.c",
                        build / "chronos_bsp_compat/dsi_panel_nicolaielectronics_st7701.c"]
    for source in required_sources:
        if source.resolve() not in files:
            raise ValueError(f"Compatibility source not compiled: {source}")
    for original in ("badgeteam__badge-bsp/targets/tanmatsu/badge_bsp_display.c",
                     "nicolaielectronics__mipi_dsi_abstraction/dsi_panel_nicolaielectronics_st7701.c"):
        if (root / "managed_components" / original).resolve() in files:
            raise ValueError(f"Unpatched source also compiled: {original}")
    link_map = (build / "chronos.map").read_text()
    for symbol, caller in (("__wrap_bsp_storage_initialize", "badge_bsp_device.c.obj"),
                           ("mipi_dsi_hal_host_gen_read_timed_out", "dsi_panel_nicolaielectronics_st7701.c.obj")):
        reference = re.search(r"^" + symbol + r"\s+([^\n]+)\n((?: +[^\n]+\n)*)", link_map, re.M)
        if not reference or caller not in reference[2]:
            raise ValueError(f"Missing linked reference: {symbol} from {caller}")
    if "--wrap=bsp_storage_initialize" not in (build / "build.ninja").read_text():
        raise ValueError("Storage linker interception is missing")
    binary = build / "chronos.bin"
    size = binary.stat().st_size
    if size > 2 * 1024 * 1024:
        raise ValueError("Firmware exceeds the 2 MiB application partition")
    info = {"dependencies": versions, "firmware_sha256": sha256(binary), "firmware_bytes": size,
            "partition_free_bytes": 2 * 1024 * 1024 - size,
            "dependencies_lock_sha256": sha256(root / "dependencies.lock"),
            "sdkconfig_sha256": sha256(Path(description["config_file"])),
            "bluetooth_enabled": "CONFIG_BT_ENABLED=y\n" in config,
            "hardware_acceptance": "pending"}
    release = json.loads((root / "release.json").read_text())
    if (release["firmware_sha256"], release["version"], release["revision"]) == (
            info["firmware_sha256"], metadata["version"], metadata["application"][0]["revision"]):
        info["hardware_acceptance"] = release["hardware_acceptance"]
        info["hardware_accepted_on"] = release["accepted_on"]
    return info


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--build-dir", type=Path)
    selection.add_argument("--fresh", action="store_true", help="Verify a temporary clean build without replacing the accepted binary")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.fresh:
        with tempfile.TemporaryDirectory(prefix="chronos-release-check-") as temporary:
            subprocess.run(["make", "package-appfs", f"BUILD_DIR={temporary}",
                            f"SDKCONFIG={temporary}/sdkconfig"], cwd=root, check=True)
        print("Temporary clean build verified and removed; the accepted build is unchanged.")
    else:
        print(json.dumps(inspect_build(root, args.build_dir.resolve()), indent=2))
