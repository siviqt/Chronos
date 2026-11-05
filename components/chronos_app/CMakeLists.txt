idf_component_register(SRCS "src/chronos_app.c" "features/alarms.c" "features/settings.c" "features/signals.c" "features/runtime.c" "features/screen.c" "features/clock.c" "features/external.c"
    INCLUDE_DIRS "include"
    REQUIRES external_display settings_store timing_core chronos_ui
    PRIV_REQUIRES log tanmatsu_platform)
