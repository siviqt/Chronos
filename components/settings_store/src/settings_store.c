#include "settings_store.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STORE_PATH_CAPACITY 512
#define STORE_PAYLOAD_LIMIT 65536
#define STORE_HEADER_SIZE   16

/* Read a portable little-endian envelope field. */
static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Write envelope fields without native struct layout or endianness. */
static void write_u32(uint8_t* p, uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8 * i));
}

/* Detect accidental payload corruption with CRC-32/ISO-HDLC. */
static uint32_t checksum(const uint8_t* bytes, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~crc;
}

/* Bound allocations and require the entire serialization contract. */
static bool schema_valid(const settings_schema_t* schema) {
    return schema && schema->payload_limit > 0 && schema->payload_limit <= STORE_PAYLOAD_LIMIT && schema->defaults &&
           schema->validate && schema->encode && schema->decode;
}

/* Resolve a plain filename beneath an existing caller-owned directory. */
static settings_result_t resolve_path(const char* directory, const char* filename, char* path) {
    if (!directory || !directory[0] || !filename || !filename[0] || strchr(filename, '/') || strchr(filename, '\\') ||
        !strcmp(filename, ".") || !strcmp(filename, ".."))
        return SETTINGS_INVALID;
    int count = snprintf(path, STORE_PATH_CAPACITY, "%s/%s", directory, filename);
    if (count < 0 || count >= STORE_PATH_CAPACITY - 4) return SETTINGS_INVALID;
    struct stat info;
    if (stat(directory, &info) != 0)
        return errno == ENOENT || errno == ENOTDIR ? SETTINGS_UNAVAILABLE : SETTINGS_IO_ERROR;
    return S_ISDIR(info.st_mode) ? SETTINGS_OK : SETTINGS_UNAVAILABLE;
}

/* Load a bounded, checksummed schema payload; discard all partial decode results on failure. */
settings_result_t settings_store_load(const char* directory, const char* filename, const settings_schema_t* schema,
                                      void* object) {
    if (!schema_valid(schema) || !object) return SETTINGS_INVALID;
    schema->defaults(object);
    char              path[STORE_PATH_CAPACITY];
    settings_result_t result = resolve_path(directory, filename, path);
    if (result != SETTINGS_OK) return result;
    FILE* file      = fopen(path, "rb");
    bool  recovered = false;
    if (!file && errno == ENOENT) {
        char backup[STORE_PATH_CAPACITY + 4];
        snprintf(backup, sizeof(backup), "%s.bak", path);
        file      = fopen(backup, "rb");
        recovered = file != NULL;
    }
    if (!file) return errno == ENOENT ? SETTINGS_MISSING : SETTINGS_IO_ERROR;
    uint8_t  header[STORE_HEADER_SIZE];
    uint8_t* payload = NULL;
    result           = SETTINGS_CORRUPT;
    if (schema->text) {
        payload = malloc(schema->payload_limit + 1);
        if (!payload) { result = SETTINGS_NO_MEMORY; goto done; }
        size_t length = fread(payload, 1, schema->payload_limit + 1, file);
        if (length > schema->payload_limit || ferror(file)) goto done;
        if (!schema->decode(object, payload, length) || !schema->validate(object)) goto done;
        result = SETTINGS_OK;
        goto done;
    }
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) goto done;
    if (memcmp(header, "CSET", 4) || read_u32(header + 4) != schema->version) goto done;
    size_t length = read_u32(header + 8);
    if (length > schema->payload_limit) goto done;
    payload = malloc(length ? length : 1);
    if (!payload) {
        result = SETTINGS_NO_MEMORY;
        goto done;
    }
    if (fread(payload, 1, length, file) != length || fgetc(file) != EOF || ferror(file)) goto done;
    if (checksum(payload, length) != read_u32(header + 12)) goto done;
    if (!schema->decode(object, payload, length) || !schema->validate(object)) goto done;
    result = SETTINGS_OK;
done:
    if (ferror(file)) result = SETTINGS_IO_ERROR;
    if (fclose(file) != 0) result = SETTINGS_IO_ERROR;
    free(payload);
    if (result != SETTINGS_OK) schema->defaults(object);
    return result == SETTINGS_OK && recovered ? SETTINGS_RECOVERED : result;
}

/* Install without overwrite-rename, which ESP-IDF's FAT VFS does not support. */
static bool install_file(const char* path, const char* temporary) {
    char backup[STORE_PATH_CAPACITY + 4];
    snprintf(backup, sizeof(backup), "%s.bak", path);
    struct stat info;
    bool        had_previous = stat(path, &info) == 0;
    if (!had_previous && errno != ENOENT) return false;
    if (had_previous) {
        if (!S_ISREG(info.st_mode)) return false;
        if (remove(backup) != 0 && errno != ENOENT) return false;
        if (rename(path, backup) != 0) return false;
    }
    if (rename(temporary, path) != 0) {
        if (had_previous) (void)rename(backup, path);
        return false;
    }
    /* A leftover backup is harmless once the complete primary file exists. */
    (void)remove(backup);
    return true;
}

/* Replace settings only after complete encoding, writing, flushing, and closing succeeds. */
settings_result_t settings_store_save(const char* directory, const char* filename, const settings_schema_t* schema,
                                      const void* object) {
    if (!schema_valid(schema) || !object || !schema->validate(object)) return SETTINGS_INVALID;
    char              path[STORE_PATH_CAPACITY], temporary[STORE_PATH_CAPACITY + 4];
    settings_result_t result = resolve_path(directory, filename, path);
    if (result != SETTINGS_OK) return result;
    uint8_t* payload = malloc(schema->payload_limit);
    if (!payload) return SETTINGS_NO_MEMORY;
    size_t length = 0;
    if (!schema->encode(object, payload, schema->payload_limit, &length) || length > schema->payload_limit) {
        free(payload);
        return SETTINGS_INVALID;
    }
    uint8_t header[STORE_HEADER_SIZE] = {'C', 'S', 'E', 'T'};
    write_u32(header + 4, schema->version);
    write_u32(header + 8, (uint32_t)length);
    write_u32(header + 12, checksum(payload, length));
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE* file = fopen(temporary, "wb");
    if (!file) {
        free(payload);
        return SETTINGS_IO_ERROR;
    }
    bool written =
        (schema->text || fwrite(header, 1, sizeof(header), file) == sizeof(header)) &&
        fwrite(payload, 1, length, file) == length;
    if (fflush(file) != 0) written = false;
    if (fclose(file) != 0) written = false;
    free(payload);
    if (!written || !install_file(path, temporary)) {
        (void)remove(temporary);
        return SETTINGS_IO_ERROR;
    }
    return SETTINGS_OK;
}
