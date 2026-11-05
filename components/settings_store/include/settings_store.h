#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Caller-supplied schema keeps application policy out of this storage module. */
typedef struct {
    bool     text; /* Store the encoded payload directly, without a binary envelope. */
    uint32_t version;
    size_t   payload_limit;
    void (*defaults)(void* object);
    bool (*validate)(const void* object);
    bool (*encode)(const void* object, uint8_t* bytes, size_t capacity, size_t* length);
    bool (*decode)(void* object, const uint8_t* bytes, size_t length);
} settings_schema_t;
typedef enum {
    SETTINGS_OK,
    SETTINGS_RECOVERED,
    SETTINGS_MISSING,
    SETTINGS_UNAVAILABLE,
    SETTINGS_CORRUPT,
    SETTINGS_IO_ERROR,
    SETTINGS_INVALID,
    SETTINGS_NO_MEMORY
} settings_result_t;

/* The directory must already exist on mounted storage. No mountpoint or directory
 * is created. Calls are synchronous and must have a single serialized owner.
 * Load installs defaults on every failure for valid schema/object arguments.
 * Save writes a sibling .tmp file, moves an existing file to .bak, then installs
 * the new file. Load recovers .bak if the primary file is missing. These suffixes
 * are reserved for this module. Neither operation promises power-loss durability
 * on FAT; a failed rollback retains .bak for recovery on the next load. */
settings_result_t settings_store_load(const char* directory, const char* filename, const settings_schema_t* schema,
                                      void* object);
settings_result_t settings_store_save(const char* directory, const char* filename, const settings_schema_t* schema,
                                      const void* object);

#endif
