# Alarm sound storage

CHRONOS stores a plain filename in `alarm_sound`. It searches for that filename
in this order, using the first copy it can load:

1. `/sd/chronos/` — user-provided sounds, alongside SD settings.
2. `/sd/apps/com.github.siviqt.chronos/` — assets installed on SD.
3. `/sd/apps/chronos/` — older manually copied SD packages.
4. `/int/apps/com.github.siviqt.chronos/` — assets installed internally.
5. `/int/apps/chronos/` — older internal packages.

AppFS documentation uses `<mountpoint>/apps/<slug>/` as a template. The launcher
uses `/sd` or `/int` for the mountpoint; `/mountpoint` is not a real directory.
The earlier lookup mistakenly used that literal path, and its mocked host test
repeated the same assumption. Those checks could not establish device access.

CHRONOS mounts the existing internal `locfd` FAT partition using the launcher's
wear-levelling configuration, with formatting disabled. It unmounts internal
storage after audio shutdown. Internal sounds remain available without an SD
card, while settings persistence still requires SD.

The sound picker combines available folders in lexical order, lists duplicate
filenames once, and returns to the built-in tone after the last filename. Empty
selection uses the built-in tone. If all copies fail to load, the built-in tone
is used. Removing SD retries the current selection from internal storage.

Accepted files are uncompressed signed PCM16 WAV, mono or stereo, at
8,000–48,000 Hz. Source and converted audio must each fit within 2 MiB.

## Validation

Host regression tests cover lookup priority, directory merging, duplicate
filenames, missing storage, and picker operation without SD. The full host
suite and ESP-IDF 6.0.2 AppFS package build passed. The user confirmed that the current application passed hardware checks on
2026-11-02; see [acceptance](ACCEPTANCE.md).

Installable build output is `build/release-check/appfs/com.github.siviqt.chronos/`.
`make package-appfs` declares and copies `alarm1.wav`–`alarm5.wav` from the five
tracked `TF*.WAV` files. The filenames and audio match the previous repository
package, preserving saved sound selections. The package verifier checks WAV
format, metadata assets, file completeness and checksums. Install the complete
folder so these assets reach the matching SD/internal package directory.
