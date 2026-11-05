# CHRONOS artwork

`chronos-logo.png` and `chronos-icon.png` are the current prepared source images,
created from the user's supplied clock artwork. The unused reference sheet is
not part of the current build inputs.

`tools/generate_brand_assets.py` (Python + Pillow) generates the three launcher
icons and `components/chronos_ui/assets/splash.c`. The embedded 420×420 indexed
logo occupies 177,424 bytes including its palette and needs no runtime decoder.

`assets/icons/metadata.json` is authoritative for version 1.2.0, revision 6,
BSP major 2, established author/license and the five installed WAV names. CMake
uses that version for the firmware descriptor.

`make package-appfs` creates
`build/release-check/appfs/com.github.siviqt.chronos/`. Copy the complete folder to
`/sd/apps/com.github.siviqt.chronos/` and use the launcher installation workflow.
`make sync-app-repository APP_REPOSITORY_DIR=...` exports firmware, metadata, icons,
WAVs, user guides and consolidated licenses. Build information, acceptance record,
checksums and individual notices remain in the complete local package.
