# CHRONOS typography

CHRONOS uses PAX graphics **2.2.2**, not LVGL. The requested font hierarchy is
implemented as native PAX bitmap fonts without adding another UI framework.
Normal ESP-IDF and host builds compile the committed C resources. They do not
need Python, source fonts, a font converter, network access, or manual regeneration.

## Sources and licenses

The unmodified TTF files in `source/` come from the official [IBM Plex repository](https://github.com/IBM/plex)
at commit `78cd4223d8de9fcb78cba84eadecb269c56093c5`:

- `packages/plex-sans/fonts/complete/ttf/IBMPlexSans-{Regular,Medium,SemiBold}.ttf`
- `packages/plex-mono/fonts/complete/ttf/IBMPlexMono-{Medium,SemiBold}.ttf`
- `packages/plex-sans-thai/fonts/complete/ttf/IBMPlexSansThai-Regular.ttf`

`LICENSE.txt` contains IBM's SIL OFL 1.1 copyright/license notice, with only
line endings and trailing whitespace normalized. Source
SHA-256 hashes are recorded in `manifest.json`. Converted fonts have internal
names beginning “CHRONOS” to avoid reusing the reserved font name for modified
font software; filenames retain the source family for maintainability.

The previous font exposed 727 printable code points, including extended Latin,
Vietnamese, Thai, combining marks, currency, mathematical symbols and private-use
symbols. English is the current built-in UI language; alarm labels accept UTF-8.
`coverage.json` records the previous exact ranges (the old CR control entry is
not generated; input validation already excludes controls).

The 14px Regular and 16px Regular text fonts preserve all 727
printable code points. IBM Plex Sans supplies its supported glyphs, with IBM Plex
Sans Thai Regular filling the Thai ranges. The 79 glyphs missing from both Plex
sources remain as a **coverage-only compatibility subset**, extracted from the
previous launcher-derived Chakra Petch bitmap at CHRONOS commit
`147cf78a912b3c7ef4c41c63bdf82952abf5bcdb`. Their original 2-bpp coverage and
metrics are preserved in `compatibility-glyphs.json`, then resized and packed
into the same generated font resources. There is no second runtime font path
or full legacy font. `LICENSE.compatibility` retains the Chakra Petch OFL notice
from [its Google Fonts distribution](https://github.com/google/fonts/tree/main/ofl/chakrapetch).
PAX's existing lack of complex-script shaping is unchanged.

Both font licenses are copied into `make package-appfs` output. Include both
notices when redistributing the firmware separately as well.

## Native resources and roles

| Family | Weight | Size | Semantic role | Coverage |
|---|---|---|---|---|
| Sans | Regular 400 | 12 | CAPTION | Printable ASCII |
| Sans | Regular 400 | 14 | SECONDARY | 727 existing printable code points |
| Sans | Regular 400 | 16 | BODY | Same |
| Sans | Medium 500 | 16 | BUTTON | Printable ASCII (static navigation/buttons) |
| Sans | SemiBold 600 | 18 | SECTION | Printable ASCII |
| Sans | Medium 500 | 20 | DATE | Printable ASCII |
| Sans | SemiBold 600 | 24 | TITLE | Printable ASCII |
| Mono | Medium 500 | 16 | TIME_SMALL | Space, `+-.0123456789:%` |
| Mono | Medium 500 | 24 | TIME_NORMAL | `0123456789:` |
| Mono | Medium 500 | 28 | TIME_MEDIUM | `0123456789:` |
| Mono | SemiBold 600 | 56 | TIME_LARGE | `.0123456789:` |
| Mono | SemiBold 600 | 72 | CLOCK_MAIN | `-0123456789:` |

All aliases have the `CHRONOS_FONT_` prefix. `CLOCK_ALT` aliases `TIME_LARGE`:
there is no alternate large-clock screen in the application, so no unused 64px
resource is embedded. A future alternate clock should add 64px to the generator
and build lists when actually needed. The 72px hyphen is used by the unavailable
clock placeholder `--:--`. AM/PM and units use Sans, outside numeric subsets.

Generated filenames are `ibm_plex_<family>_<weight>_<size>.c`, under
`components/chronos_ui/assets/fonts/`. `manifest.json` lists every exact source,
weight, size, coverage, packed bitmap byte count, baseline and line height.
No unused weights, italics, or full-size large-font alphabets are embedded.

## Deterministic regeneration

Use Python with **Pillow 12.3.0**, bundled **FreeType 2.14.3**, and
**fonttools 4.61.1**. For example, in an isolated environment:

```sh
python -m pip install Pillow==12.3.0 fonttools==4.61.1
python tools/generate_fonts.py
```

The generator checks Pillow/FreeType versions and uses the BASIC rasterizer,
native integer pixel sizes and rounded integer advances. Each glyph is packed
as 4-bit grayscale, low nibble first, with byte-padded rows as required by PAX.
There is no runtime enlargement, second antialiasing pass or hidden host-font
lookup. The generated header supplies semantic aliases and actual baseline/line
height metrics; tall accents and Thai marks determine the full line boxes.

The only generation inputs are the committed TTFs, coverage manifest,
compatibility glyph data and script. Two consecutive generations must produce
identical C/header/manifest bytes. Add new resources explicitly to both the
component CMake source list and the host render-test source list.

## Layout and validation

The 72px clock and 56px timers fit without reducing the requested large sizes.
Clock centering uses the fixed widths of HH:MM and seconds together; their
baselines align by generated metrics. 28px world times fit in the 43px rows
with separate Sans suffixes and metadata; metadata placement accounts for the
full 14px text line box. Alarm times use 24px Mono beside baseline-aligned Sans
descriptions. Rows remain 36px high. Tabs/buttons are vertically centered by
line height; notifications and footer hints use 14px text and adequate line
spacing. Long active text edits scroll horizontally to retain the caret instead
of shrinking the native font; blinking does not change the viewport.

`make test-render` uses the real renderer to check every second in both clock
formats and all themes, stable punctuation for 00:00/11:11/18:58/23:59,
fixed advances, glyph line-box bounds, 4-bpp resources, maximum elapsed time,
button/city/time widths, solid/antialiased timer strokes and long-edit caret
stability. `make test-host` covers existing application behavior. Rendered
previews cover clock, world selection, timers, editor, alarms, settings,
activity and notifications. Physical checks for the current 1.2.0 revision 6 release passed on
2026-11-02; see [acceptance](../../docs/ACCEPTANCE.md).
