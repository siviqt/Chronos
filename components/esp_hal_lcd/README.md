# ESP Hardware Abstraction Layer for LCD Peripherals

## CHRONOS integration

Vendored unchanged from `components/esp_hal_lcd` in the Tanmatsu launcher at
commit `20b1ddd31846a8d06de85d4ca1a7ca447c18e0bb` (2026-10-25 migration).
The Apache-2.0 license text is included in `LICENSE`; source copyright/SPDX
notices are retained. This integration note and license file are local additions.

Source: https://github.com/Nicolai-Electronics/tanmatsu-launcher/tree/20b1ddd31846a8d06de85d4ca1a7ca447c18e0bb/components/esp_hal_lcd

This project component overrides the stock ESP-IDF 6.0.2 LCD HAL. BSP 2.0.0's
MIPI DSI 2.0.0 dependency uses its `mipi_dsi_hal_host_gen_read_timed_out()`
API and bounded 50 ms panel reads. Keep the implementation, public header and
CMake dependency on `esp_timer` together. Do not replace the timeout flag with
a constant or edit the installed SDK. CHRONOS retains its separate display DMA
completion fence; this component protects panel command reads during startup.
MIPI 2.0.0 makes timeout detection optional; CHRONOS explicitly enables
`CONFIG_MIPI_DSI_ABSTRACTION_USE_VENDORED_ESP_HAL_LCD=y` in its board defaults.

> [!NOTE]
> This component is currently in beta. Its API, behavior, and compatibility may change at any time and without notice; backward compatibility is not guaranteed. Use caution when integrating into production systems.

## Overview

The `esp_hal_lcd` component provides a **Hardware Abstraction Layer** for LCD (Liquid Crystal Display) controllers and MIPI DSI (Display Serial Interface) peripherals across ESP-IDF supported targets. This HAL enables efficient communication with various display interfaces including I80 parallel, RGB parallel, and MIPI DSI serial interfaces.

## Architecture

The LCD HAL is structured in two main sub-layers:

1. **HAL Layer (Upper)**: Defines the operational steps and data structures required to control LCD peripherals (e.g., initialization, clock configuration, data transmission).

2. **Low-Level Layer (Bottom)**: Serves as a translation layer between the HAL and the register files defined in the `soc` component, handling target-specific register configurations.

## Supported LCD Interfaces

This HAL supports various LCD interface types depending on the ESP chip:

- **I80 (Intel 8080) Parallel Interface**: 8-bit/16-bit parallel interface commonly used for MCU-based displays
- **RGB Parallel Interface**: Direct RGB pixel data interface for high-performance displays
- **MIPI DSI Interface**: High-speed serial interface supporting:
  - DSI Host Controller
  - DSI Bridge Controller
  - PHY layer configuration
  - DCS (Display Command Set) commands
  - Generic read/write operations

## Features

### LCD Common Features
- Pixel clock (PCLK) calculation and configuration
- Multiple color format support using FourCC standard:
  - RGB565, RGB888
  - YUV422 (YUYV, YVYU, VYUY, UYVY)
  - GRAY8 (8-bit grayscale)
- RGB data endian configuration (MSB/LSB first)
- Color space conversion (RGB ↔ YUV)
- YUV conversion standards (BT.601, BT.709)
- Color range control (Limited/Full)

### MIPI DSI Specific Features
- PHY PLL configuration for high-speed lanes
- Virtual channel management
- DCS command transmission (short/long writes)
- Generic interface for command/data operations
- Test pattern generation (vertical/horizontal bars, BER patterns)
- Multi-lane data transmission support
- DPI (Display Pixel Interface) clock configuration

## Usage

The HAL functions primarily serve ESP-IDF LCD peripheral drivers such as `esp_lcd` component.

Advanced developers can use these interfaces directly when implementing custom display drivers, with the understanding that API stability is not guaranteed.

## Dependencies

- `soc`: Provides chip-specific register definitions and peripheral capabilities
- `hal`: Core hardware abstraction utilities, macros, and color type definitions
