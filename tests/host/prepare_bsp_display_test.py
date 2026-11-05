"""Expose the production BSP helper to the driver lifecycle test."""
from pathlib import Path
import sys

directory = Path(sys.argv[1])
source = (directory / "badge_bsp_display.c").read_text()
start = source.index("static esp_err_t bsp_display_initialize_panel(")
body = source.index("{", start)
depth = 1
end = body + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
(directory / "bsp_display_initialize_panel.inc").write_text(source[start:end] + "\n")

# The shim declarations are centralized; every production include uses them.
for header in ("driver/gpio.h", "esp_check.h", "esp_lcd_mipi_dsi.h", "esp_lcd_panel_commands.h",
               "esp_lcd_panel_io.h", "esp_lcd_panel_ops.h", "esp_lcd_st7701.h", "esp_ldo_regulator.h",
               "esp_log.h", "esp_timer.h", "freertos/FreeRTOS.h", "freertos/semphr.h",
               "freertos/task.h", "sdkconfig.h", "hal/mipi_dsi_hal.h"):
    path = directory / "stubs" / header
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('#include "bsp_compat_sdk.h"\n')
