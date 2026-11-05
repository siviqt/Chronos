# Reusable ESP-IDF build commands; activate the SDK before invoking make.
IDF_PY ?= $(if $(IDF_PATH),python "$(IDF_PATH)/tools/idf.py",idf.py)
BUILD_DIR ?= build/release-check
IDF_TARGET ?= esp32p4
SDKCONFIG_DEFAULTS ?= sdkconfigs/general;sdkconfigs/tanmatsu
SDKCONFIG ?= $(BUILD_DIR)/sdkconfig
HOST_BUILD_DIR ?= $(BUILD_DIR)/host
PYTHON ?= python3
IDF_ARGS = -B "$(BUILD_DIR)" -DIDF_TARGET=$(IDF_TARGET) -DSDKCONFIG="$(SDKCONFIG)" -DSDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)"

.PHONY: help build configure menuconfig
help:
	@echo "Without SDK: make test-host. With ESP-IDF 6.0.2: make configure | make build | make menuconfig"
configure:
	$(IDF_PY) $(IDF_ARGS) reconfigure
build:
	$(IDF_PY) $(IDF_ARGS) build
menuconfig:
	$(IDF_PY) $(IDF_ARGS) menuconfig

# Canonical package: /sd/apps/com.github.siviqt.chronos/.
.PHONY: package-appfs
package-appfs: build
	$(PYTHON) tools/package_appfs.py --build-dir "$(BUILD_DIR)"

# This uses a new configuration derived only from the checked-in defaults.
.PHONY: check-release verify-release sync-app-repository
check-release: test-host test-render
	$(PYTHON) tools/check_release.py --fresh

verify-release:
	$(PYTHON) tools/check_release.py --build-dir "$(BUILD_DIR)"

sync-app-repository: package-appfs
	@test -n "$(APP_REPOSITORY_DIR)" || (echo "Set APP_REPOSITORY_DIR to the CHRONOS package folder"; exit 1)
	$(PYTHON) tools/package_appfs.py --build-dir "$(BUILD_DIR)" --sync-to "$(APP_REPOSITORY_DIR)"

HOST_CC ?= cc
HOST_CPPFLAGS = -Icomponents/external_display/include
HOST_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2
.PHONY: test-host
test-host:
	mkdir -p $(HOST_BUILD_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Itests/host/platform_stubs components/tanmatsu_platform/src/platform_bsp_storage.c tests/host/test_bsp_storage.c tests/host/bsp_storage_original.c -Wl,--wrap=bsp_storage_initialize -o $(HOST_BUILD_DIR)/test_bsp_storage
	$(HOST_BUILD_DIR)/test_bsp_storage
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/timing_core/include components/timing_core/src/timing_core.c tests/host/test_timing_core.c -o $(HOST_BUILD_DIR)/test_timing_core
	$(HOST_BUILD_DIR)/test_timing_core
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/chronos_app/include components/chronos_app/features/alarms.c tests/host/test_alarms.c -o $(HOST_BUILD_DIR)/test_alarms
	$(HOST_BUILD_DIR)/test_alarms
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/chronos_app/include -Icomponents/settings_store/include components/chronos_app/features/alarms.c components/chronos_app/features/settings.c components/settings_store/src/settings_store.c tests/host/test_settings.c -Wl,--wrap=rename -o $(HOST_BUILD_DIR)/test_settings
	$(HOST_BUILD_DIR)/test_settings
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/tanmatsu_platform/include components/tanmatsu_platform/src/platform_pcm.c tests/host/test_pcm.c -o $(HOST_BUILD_DIR)/test_pcm
	$(HOST_BUILD_DIR)/test_pcm
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/audio_stubs -Itests/host/platform_stubs -Imanaged_components/badgeteam__badge-bsp -Icomponents/tanmatsu_platform/include components/tanmatsu_platform/src/platform_pcm.c tests/host/test_audio.c -o $(HOST_BUILD_DIR)/test_audio
	$(HOST_BUILD_DIR)/test_audio
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/chronos_app/include -Icomponents/settings_store/include -Icomponents/timing_core/include components/chronos_app/features/alarms.c components/chronos_app/features/settings.c components/chronos_app/features/signals.c components/chronos_app/features/runtime.c components/settings_store/src/settings_store.c components/timing_core/src/timing_core.c tests/host/test_signals.c -o $(HOST_BUILD_DIR)/test_signals
	$(HOST_BUILD_DIR)/test_signals
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/platform_stubs -Imanaged_components/badgeteam__badge-bsp -Icomponents/tanmatsu_platform/include components/tanmatsu_platform/src/platform_sd.c components/tanmatsu_platform/src/platform_leds.c components/tanmatsu_platform/src/platform_power.c tests/host/test_platform.c -Wl,--wrap=mkdir,--wrap=stat -lm -o $(HOST_BUILD_DIR)/test_platform
	$(HOST_BUILD_DIR)/test_platform
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/chronos_ui/include -Icomponents/chronos_app/include -Icomponents/settings_store/include -Icomponents/timing_core/include components/chronos_ui/src/chronos_ui.c components/chronos_app/features/screen.c components/chronos_app/features/clock.c components/external_display/src/format.c components/chronos_app/features/alarms.c components/chronos_app/features/settings.c components/chronos_app/features/signals.c components/chronos_app/features/runtime.c components/settings_store/src/settings_store.c components/timing_core/src/timing_core.c tests/host/test_ui.c -o $(HOST_BUILD_DIR)/test_ui
	$(HOST_BUILD_DIR)/test_ui
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/app_stubs -Itests/host/platform_stubs -Icomponents/tanmatsu_platform/include -Icomponents/chronos_ui/include -Icomponents/chronos_app/include -Icomponents/settings_store/include -Icomponents/timing_core/include components/chronos_ui/src/chronos_ui.c components/chronos_app/features/clock.c components/chronos_app/features/screen.c components/external_display/src/format.c components/chronos_app/features/alarms.c components/chronos_app/features/settings.c components/chronos_app/features/signals.c components/chronos_app/features/runtime.c components/settings_store/src/settings_store.c components/timing_core/src/timing_core.c components/chronos_app/features/external.c tests/host/test_app_startup.c -o $(HOST_BUILD_DIR)/test_app_startup
	$(HOST_BUILD_DIR)/test_app_startup

	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/input_stubs -Itests/host/platform_stubs -Imanaged_components/badgeteam__badge-bsp -Icomponents/tanmatsu_platform/include components/tanmatsu_platform/src/platform_board.c tests/host/test_input.c -o $(HOST_BUILD_DIR)/test_input
	$(HOST_BUILD_DIR)/test_input
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/chronos_ui/include -Icomponents/chronos_app/include -Icomponents/settings_store/include components/chronos_app/features/clock.c components/chronos_app/features/settings.c components/chronos_app/features/alarms.c components/settings_store/src/settings_store.c tests/host/test_clock.c -o $(HOST_BUILD_DIR)/test_clock
	$(HOST_BUILD_DIR)/test_clock
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/clock_stubs -Itests/host/platform_stubs -Icomponents/tanmatsu_platform/include tests/host/test_network_clock.c -o $(HOST_BUILD_DIR)/test_network_clock
	$(HOST_BUILD_DIR)/test_network_clock

# Pixel-level checks using the actual PAX renderer and bundled application font.
.PHONY: test-render
test-render:
	cmake -S tests/host/render -B $(HOST_BUILD_DIR)/render
	cmake --build $(HOST_BUILD_DIR)/render --target test_render -j4
	ctest --test-dir $(HOST_BUILD_DIR)/render --output-on-failure

.PHONY: test-external
test-external:
	mkdir -p $(HOST_BUILD_DIR)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Icomponents/chronos_app/include -Icomponents/chronos_ui/include -Icomponents/settings_store/include -Icomponents/timing_core/include components/chronos_app/features/external.c components/chronos_app/features/clock.c components/chronos_app/features/screen.c components/chronos_app/features/settings.c components/chronos_app/features/alarms.c components/chronos_app/features/runtime.c components/chronos_app/features/signals.c components/chronos_ui/src/chronos_ui.c components/settings_store/src/settings_store.c components/timing_core/src/timing_core.c components/external_display/src/format.c tests/host/test_external.c -o $(HOST_BUILD_DIR)/test_external
	$(HOST_BUILD_DIR)/test_external
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/external_stubs -Itests/host/platform_stubs tests/host/test_external_worker.c -o $(HOST_BUILD_DIR)/test_external_worker
	$(HOST_BUILD_DIR)/test_external_worker
	$(HOST_CC) $(HOST_CFLAGS) -Icomponents/nokia5110/include components/nokia5110/src/nokia5110.c components/nokia5110/src/canvas.c components/nokia5110/tests/test_core.c -o $(HOST_BUILD_DIR)/test_nokia
	$(HOST_BUILD_DIR)/test_nokia
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/platform_stubs components/external_display/src/format.c tests/host/test_external_vfd.c -o $(HOST_BUILD_DIR)/test_external_vfd
	$(HOST_BUILD_DIR)/test_external_vfd
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -Itests/host/external_stubs -Itests/host/platform_stubs tests/host/test_external_transport.c -o $(HOST_BUILD_DIR)/test_external_transport
	$(HOST_BUILD_DIR)/test_external_transport

test-host: test-external

.PHONY: test-display
test-display:
	mkdir -p $(HOST_BUILD_DIR)
	$(HOST_CC) $(HOST_CFLAGS) -Itests/host/display_stubs -Itests/host/platform_stubs -Icomponents/tanmatsu_platform/include tests/host/test_display.c -o $(HOST_BUILD_DIR)/test_display
	$(HOST_BUILD_DIR)/test_display

test-host: test-display

.PHONY: test-bsp-compat
test-bsp-compat:
	$(PYTHON) tools/prepare_bsp_compat.py --bsp-dir managed_components/badgeteam__badge-bsp --mipi-dir managed_components/nicolaielectronics__mipi_dsi_abstraction --output $(HOST_BUILD_DIR)/bsp_compat
	$(PYTHON) tests/host/prepare_bsp_display_test.py "$(HOST_BUILD_DIR)/bsp_compat"
	$(HOST_CC) $(HOST_CFLAGS) -I$(HOST_BUILD_DIR)/bsp_compat -I$(HOST_BUILD_DIR)/bsp_compat/stubs -Itests/host/bsp_compat_stubs -Itests/host/platform_stubs -Imanaged_components/nicolaielectronics__mipi_dsi_abstraction/include tests/host/test_bsp_display.c -o $(HOST_BUILD_DIR)/test_bsp_display
	$(HOST_BUILD_DIR)/test_bsp_display

test-host: test-bsp-compat

.PHONY: test-release-tools
test-release-tools:
	$(PYTHON) tests/host/test_release_tools.py

test-host: test-release-tools
