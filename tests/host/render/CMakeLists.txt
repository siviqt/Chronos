cmake_minimum_required(VERSION 3.13)
project(chronos_render_tests C CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(PAX_STATIC TRUE)
add_subdirectory("${ROOT}/managed_components/robotman2412__pax-gfx" pax)
add_executable(test_render
    ../test_render.c
    "${ROOT}/components/chronos_app/features/screen.c"
    "${ROOT}/components/chronos_app/features/clock.c"
    "${ROOT}/components/chronos_app/features/settings.c"
    "${ROOT}/components/chronos_app/features/alarms.c"
    "${ROOT}/components/chronos_app/features/runtime.c"
    "${ROOT}/components/chronos_app/features/signals.c"
    "${ROOT}/components/timing_core/src/timing_core.c"
    "${ROOT}/components/settings_store/src/settings_store.c"
    "${ROOT}/components/external_display/src/format.c"
    "${ROOT}/components/chronos_ui/src/chronos_ui_splash.c"
    "${ROOT}/components/chronos_ui/assets/splash.c"
    "${ROOT}/components/chronos_ui/src/chronos_ui_render.c"
    "${ROOT}/components/chronos_ui/src/chronos_ui.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_mono_medium_16.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_mono_medium_24.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_mono_medium_28.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_mono_semibold_56.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_mono_semibold_72.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_medium_16.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_medium_20.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_regular_12.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_regular_14.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_regular_16.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_semibold_18.c"
    "${ROOT}/components/chronos_ui/assets/fonts/ibm_plex_sans_semibold_24.c"
    "${ROOT}/components/chronos_ui/assets/function_keys.c")
target_include_directories(test_render PRIVATE "${ROOT}/components/chronos_ui/include" "${ROOT}/components/external_display/include" "${ROOT}/components/chronos_app/include" "${ROOT}/components/timing_core/include" "${ROOT}/components/settings_store/include")
target_link_libraries(test_render PRIVATE pax_gfx)
enable_testing()
add_test(NAME render_stability COMMAND test_render)
