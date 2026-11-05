# CHRONOS provenance and licenses

CHRONOS is released under the MIT License in `LICENSE.txt`. The repository
package includes the application notice and consolidated `THIRD_PARTY_LICENSES.txt`.
The complete local package also includes individual third-party notices.

Board defaults, partition layout and formatting originated in the Tanmatsu PAX
template (public domain / CC0-1.0). The application components and root build files
are maintained in this project. ESP-IDF is pinned to 6.0.2; managed dependency
versions and registry hashes are retained in `dependencies.lock`.

| Current dependency | Source revision |
| --- | --- |
| Badge.Team BSP 2.0.0 | `8e9e033f74fd5b0b62323f291687c56111e049b3` |
| MIPI DSI abstraction 2.0.0 | `8a2d9f6377a7150911a0f2a1ec3212f0052a67bc` |
| Vendored launcher LCD HAL | `20b1ddd31846a8d06de85d4ca1a7ca447c18e0bb` |

The BSP component hash is
`81b73f9536a1e4beb03dbc743a29214b39fdf75d094e654b875ea0b8e0fb6f32`;
the MIPI component hash is
`4347e6e2be462590fa90db7846bea53016ef98035b1ab8e1184062d95ef134d1`.
Registry sources match the reviewed SDK snapshots except for generated metadata.
The HAL implementation is unchanged from the pinned launcher component, with
original SPDX notices and Apache-2.0 text; its README is a local integration note.

`tools/prepare_bsp_compat.py` maintains the two small display fixes as hash-checked
build-directory source copies: initialization error propagation and ID/OTP timeout
cleanup. Original MIT notices remain. No installed SDK or managed source is patched.
The MIPI archive declares MIT in its manifest/source SPDX notice but omits a
standalone license file; `licenses/LICENSE.mipi-dsi.txt` supplies the standard text
with the source's Nicolai Electronics copyright.

Storage behavior follows the BSP/launcher wiring and ESP-IDF 6.0.2 FAT/SDMMC
ownership contracts. FAT rename cannot replace an existing destination, so the
settings store uses temporary/backup replacement. Unmount consumes its card/context
before some final error returns; the application does not reuse released handles.
The linker wrapper keeps SD/LDO4 under application ownership without editing BSP.

The timezone catalogue derives from the launcher's `components/timezone/database.c`
at revision `20022fce316d768a1ff966b973ee2127511713f5`. All 598 name/rule pairs are
retained with the type adapted to CHRONOS. Its MIT notice is in
`components/chronos_app/features/LICENSE.timezone`. The snapshot identifies no
tzdata release and contains POSIX rules rather than historical transition tables.

IBM Plex font source revisions, SHA-256 hashes, coverage, compatibility glyphs,
OFL notices and deterministic generation are documented in
[font provenance](../assets/fonts/README.md). Prepared brand images generate the
current icons and embedded splash through `tools/generate_brand_assets.py`.
The Tanmatsu function-key artwork retains `components/chronos_ui/assets/LICENSE.launcher`.

`components/nokia5110` contains the native controller, transport, canvas/font and
tests from the supplied Nokia 5110 module 1.0.0, with its MIT license. The unused
reference archive, example firmware and launcher/plugin packages are not retained.
The VFD adapter derives from the supplied Nicolai Electronics I2C VFD example and
NE-HCS12SS59T-R1 documentation; current wiring/protocol details are retained in
[the external-display guide](external-display.md).

The five bundled WAVs use the previous package's audio samples. TF022, TF023,
TF034, TF048 and TF050 map respectively to alarm1–alarm5. The source distribution
omits TF050's engineer metadata field and retains its audio and loop data. Current sources and generated
resources remain checked in; obsolete reference copies and migration reports are
removed from the working tree. Git retains the project history.
