# Nokia 5110 component

Native driver, canvas/font and portable controller tests from the supplied
Nokia 5110 module 1.0.0 (MIT). Controller and GPIO transport sources are unchanged;
its MIT license is retained here. Unused reference archives and example firmware
are not required by this application.

CHRONOS lifecycle, content selection, configuration validation and rendering
live in `chronos_app` and `external_display`. See
[the external-display guide](../../docs/external-display.md) for wiring and behavior.
Run controller/transport tests with `make test-external` at the project root.
