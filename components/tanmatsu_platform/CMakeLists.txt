idf_component_register(
    SRCS "src/platform_board.c" "src/platform_display.c" "src/platform_sd.c" "src/platform_audio.c"
         "src/platform_clock.c" "src/platform_pcm.c" "src/platform_leds.c" "src/platform_power.c" "src/platform_internal.c"
         "src/platform_bsp_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common robotman2412__pax-gfx
    PRIV_REQUIRES badgeteam__badge-bsp esp_lcd nvs_flash esp_timer esp_driver_gpio
                  nicolaielectronics__tanmatsu-wifi nicolaielectronics__esp-hosted-tanmatsu nicolaielectronics__wifi-manager lwip esp_driver_sdmmc sdmmc fatfs wear_levelling esp_driver_i2s freertos)

# CHRONOS owns SD/LDO4 across both hosted and standalone controller lifetimes.
# Intercept the new BSP initialization call without modifying managed sources.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=bsp_storage_initialize")
