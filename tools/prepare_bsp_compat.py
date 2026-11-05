#!/usr/bin/env python3
"""Generate the two reviewed BSP 2.0.0 fixes without editing managed sources."""

import argparse
import hashlib
from pathlib import Path


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise ValueError("BSP compatibility patch context has changed")
    return source.replace(before, after, 1)


def prepare(bsp_dir, mipi_dir, output):
    inputs = [
        (bsp_dir / "targets/tanmatsu/badge_bsp_display.c",
         "92093d950441ff7fb50dd30ee5ea98040e241307013bec1e09ce2502d486670a"),
        (mipi_dir / "dsi_panel_nicolaielectronics_st7701.c",
         "0867ae08350d2225c938f4649c79b81c148246025414d7b43b1fc64d45fbcf7a"),
    ]
    sources = []
    for path, expected in inputs:
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f"Unreviewed BSP/MIPI source: {path}; review compatibility patches")
        sources.append(data.decode())
    sources[0] = replace_once(
        sources[0], "    st7701_initialize(&config);\n    return ESP_OK;",
        "    return st7701_initialize(&config);")
    sources[1] = replace_once(
        sources[1],
        "esp_err_t st7701_initialize(const st7701_configuration_t* config) {\n",
        "esp_err_t st7701_initialize(const st7701_configuration_t* config) {\n"
        "    if (!config) return ESP_ERR_INVALID_ARG;\n"
        "    st7701_id_valid = false;\n"
        "    st7701_is_display_2 = false;\n"
        "    st7701_vcom_otp_valid = false;\n")
    cleanup = ("        esp_lcd_panel_io_del(mipi_dbi_io);\n"
               "        mipi_dbi_io = NULL;\n"
               "        esp_lcd_del_dsi_bus(mipi_dsi_bus);\n")
    sources[1] = replace_once(
        sources[1],
        '        ESP_LOGE(TAG, "No response from panel while reading display ID, is a display connected?");\n',
        '        ESP_LOGE(TAG, "No response from panel while reading display ID, is a display connected?");\n'
        + cleanup)
    sources[1] = replace_once(
        sources[1],
        "        mipi_dpi_panel = NULL;\n        return ESP_ERR_NOT_FOUND;",
        "        mipi_dpi_panel = NULL;\n"
        "        st7701_id_valid = false;\n"
        "        st7701_is_display_2 = false;\n" + cleanup +
        "        return ESP_ERR_NOT_FOUND;")
    output.mkdir(parents=True, exist_ok=True)
    for (path, _), source in zip(inputs, sources):
        destination = output / path.name
        if not destination.exists() or destination.read_text() != source:
            destination.write_text(source)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bsp-dir", required=True, type=Path)
    parser.add_argument("--mipi-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.bsp_dir, args.mipi_dir, args.output)
