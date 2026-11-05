# CHRONOS alarm sounds

The generated fallback tone needs no asset or SD card. Settings select a plain
filename, searched in the order documented in [sound storage](../../docs/ALARM_SOUNDS.md).
User sounds take priority over canonical/legacy SD and internal package assets.

The package maps the current source files without conversion:

| Source | Installed name |
| --- | --- |
| `TF022.WAV` | `alarm1.wav` |
| `TF023.WAV` | `alarm2.wav` |
| `TF034.WAV` | `alarm3.wav` |
| `TF048.WAV` | `alarm4.wav` |
| `TF050.WAV` | `alarm5.wav` |

Input must be uncompressed PCM16 RIFF/WAVE, mono/stereo at 8–48 kHz. Source and
44.1-kHz converted audio each fit within 2 MiB; container overhead is limited to
64 KiB. Missing/invalid/unreadable/unallocatable clips select the built-in tone.
Alarm clips loop while ringing. Hardware checks for 1.2.0 revision 6 passed on
2026-11-02; see [acceptance](../../docs/ACCEPTANCE.md).
