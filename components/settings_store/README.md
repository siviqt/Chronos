# settings_store

Reusable synchronous C persistence with caller-supplied directory, filename, and
schema. The schema owns defaults, validation, encoding, and decoding. Binary schemas use a
versioned, bounded, CRC-protected envelope; text schemas store their bounded payload directly; no native struct layout is persisted.

The caller must provide an existing directory on mounted storage and serialize
all access. Loading failures leave defaults. Saves use temporary/backup files to
support FAT rename semantics, with rollback and missing-primary backup recovery.
No automatic directory creation or guaranteed sudden-power-loss durability.

Public API: `include/settings_store.h`. See ARCHITECTURE.md for result handling,
file format, memory ownership, and recovery details. CHRONOS-specific fields and
limits live in `chronos_app/features/settings.c`. Run `make test-host` at the root.
