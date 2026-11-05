#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "chronos_settings.h"

/* Emulate FAT's refusal to overwrite and inject commit/rollback failures. */
static unsigned int fail_rename_calls;
int                 __real_rename(const char* source, const char* destination);
int                 __wrap_rename(const char* source, const char* destination) {
    if (fail_rename_calls && (strstr(source, ".tmp") || strstr(source, ".bak"))) {
        --fail_rename_calls;
        errno = EIO;
        return -1;
    }
    if (access(destination, F_OK) == 0) {
        errno = EEXIST;
        return -1;
    }
    return __real_rename(source, destination);
}

/* Compare canonical payloads rather than compiler-dependent struct padding. */
static void assert_equal(const chronos_settings_t* a, const chronos_settings_t* b) {
    uint8_t first[4096], second[4096];
    size_t  first_length, second_length;
    assert(chronos_settings_schema.encode(a, first, sizeof(first), &first_length));
    assert(chronos_settings_schema.encode(b, second, sizeof(second), &second_length));
    assert(first_length == second_length && !memcmp(first, second, first_length));
}

/* Confirm a failed load replaced the entire object with usable defaults. */
static void assert_defaults(const chronos_settings_t* settings) {
    chronos_settings_t defaults;
    chronos_settings_defaults(&defaults);
    assert_equal(settings, &defaults);
}

/* Populate every persisted field, including all five alarm and city slots. */
static chronos_settings_t custom_settings(void) {
    chronos_settings_t settings;
    chronos_settings_defaults(&settings);
    settings.theme = CHRONOS_THEME_OPERATOR;
    settings.ntp_enabled      = false;
    settings.time_source = CHRONOS_TIME_RTC;
    settings.use_24_hour      = false;
    settings.countdown_buzzer = false;
    strcpy(settings.ntp_server, "time.example.org");
    strcpy(settings.timezone, "EET-2EEST,M3.5.0/3,M10.5.0/4");
    settings.keyboard_percent  = 50;
    settings.display_percent   = 100;
    settings.countdown_seconds = 3600;
    settings.pacer_period_ms   = 500;
    settings.city_count        = 5;
    strcpy(settings.alarm_sound, "wake-up.wav");
    for (size_t i = 0; i < 5; ++i) {
        snprintf(settings.cities[i].name, sizeof(settings.cities[i].name), "City %zu", i);
        strcpy(settings.cities[i].timezone, "EST5EDT,M3.2.0,M11.1.0");
        settings.alarms[i].enabled        = true;
        settings.alarms[i].repeat         = (chronos_alarm_repeat_t)(i % 3);
        settings.alarms[i].hour           = (uint8_t)(6 + i);
        settings.alarms[i].minute         = (uint8_t)(10 + i);
        settings.alarms[i].snooze_minutes = 15;
        settings.alarms[i].ring_seconds   = 60;
        strcpy(settings.alarms[i].label, "Wake \xf0\x9f\x94\x94");
    }
    return settings;
}

/* Verify configuration boundaries and reject invalid timezone/server/path syntax. */
static void test_validation(void) {
    chronos_settings_t settings;
    chronos_settings_defaults(&settings);
    assert(chronos_settings_valid(&settings));
    assert(!strcmp(settings.ntp_server, "pool.ntp.org"));
    assert(settings.alarms[0].ring_seconds == 15);
    const char* zones[] = {"UTC0", "<+03>-3", "EET-2EEST,M3.5.0/3,M10.5.0/4", "EST5EDT4,J60/2,J300/2",
                           "ABC0DEF,60/-2,300/25"};
    for (size_t i = 0; i < sizeof(zones) / sizeof(zones[0]); ++i) {
        strcpy(settings.timezone, zones[i]);
        assert(chronos_settings_valid(&settings));
    }
    const char* bad_zones[] = {"",
                               "Europe/Sofia",
                               "UTC",
                               "UT0",
                               "UTC25",
                               "EST5EDT",
                               "ABC0DEF,M13.1.0,M10.1.0",
                               "ABC0DEF,M3.0.0,M10.1.0",
                               "ABC0DEF,M3.1.7,M10.1.0",
                               "ABC0DEF,J0,J366",
                               "ABC0DEF,1/168,300",
                               "ABC0DEF,1/2:60,300",
                               "UTC0garbage"};
    for (size_t i = 0; i < sizeof(bad_zones) / sizeof(bad_zones[0]); ++i) {
        strcpy(settings.timezone, bad_zones[i]);
        assert(!chronos_settings_valid(&settings));
    }
    chronos_settings_defaults(&settings);
    const char* bad_servers[] = {"", "https://time.example.org", "bad host", "-host", "host-", "a..b"};
    for (size_t i = 0; i < sizeof(bad_servers) / sizeof(bad_servers[0]); ++i) {
        strcpy(settings.ntp_server, bad_servers[i]);
        assert(!chronos_settings_valid(&settings));
    }
    chronos_settings_defaults(&settings);
    settings.keyboard_percent = 101;
    assert(!chronos_settings_valid(&settings));
    settings.keyboard_percent = 0;
    settings.display_percent  = 101;
    assert(!chronos_settings_valid(&settings));
    settings.display_percent = 0;
    assert(!chronos_settings_valid(&settings));
    settings.display_percent = settings.keyboard_percent = 5;
    assert(chronos_settings_valid(&settings));
    settings.city_count = 6;
    assert(!chronos_settings_valid(&settings));
    settings.city_count = 1;
    settings.cities[0].name[0] = 0;
    assert(!chronos_settings_valid(&settings));
    settings.city_count        = 0;
    settings.countdown_seconds = 3601;
    assert(!chronos_settings_valid(&settings));
    settings.countdown_seconds = 0;
    assert(!chronos_settings_valid(&settings));
    settings.countdown_seconds = 1;
    settings.pacer_period_ms   = 0;
    assert(!chronos_settings_valid(&settings));
    settings.pacer_period_ms = 1;
    strcpy(settings.alarm_sound, "../sound.wav");
    assert(!chronos_settings_valid(&settings));
    strcpy(settings.alarm_sound, "sound.wav");
    assert(chronos_settings_valid(&settings));
}

/* Reject every truncation, extra bytes, invalid booleans, and oversized string lengths. */
static void test_codec(void) {
    chronos_settings_t settings = custom_settings(), decoded;
    uint8_t            bytes[4096];
    size_t             length;
    assert(chronos_settings_schema.encode(&settings, bytes, sizeof(bytes), &length));
    for (size_t i = 0; i < length; ++i)
        if (i != length - CHRONOS_ALARM_LIMIT - 1 && i != length - 1) assert(!chronos_settings_schema.decode(&decoded, bytes, i));
    assert(chronos_settings_schema.decode(&decoded, bytes, length - CHRONOS_ALARM_LIMIT - 1));
    assert(decoded.alarms[0].weekdays == 0x1f);
    assert(decoded.theme == CHRONOS_THEME_MATRIX);
    assert(chronos_settings_schema.decode(&decoded, bytes, length - 1));
    assert(decoded.theme == CHRONOS_THEME_MATRIX);
    for (unsigned theme = 0; theme < CHRONOS_THEME_COUNT; ++theme) {
        bytes[length - 1] = theme;
        assert(chronos_settings_schema.decode(&decoded, bytes, length));
        assert((unsigned)decoded.theme == theme);
    }
    bytes[length - 1] = 3;
    assert(!chronos_settings_schema.decode(&decoded, bytes, length));
    bytes[length - 1] = settings.theme;
    assert(chronos_settings_schema.decode(&decoded, bytes, length));
    assert_equal(&settings, &decoded);
    settings.alarms[0].weekdays = 0x60;
    assert(chronos_settings_schema.encode(&settings, bytes, sizeof(bytes), &length));
    assert(chronos_settings_schema.decode(&decoded, bytes, length));
    assert(decoded.alarms[0].weekdays == 0x60);
    bytes[length] = 0;
    assert(!chronos_settings_schema.decode(&decoded, bytes, length + 1));
    bytes[0] = 2;
    assert(!chronos_settings_schema.decode(&decoded, bytes, length));
    bytes[0] = 0;
    bytes[3] = 255;
    bytes[4] = 255;
    assert(!chronos_settings_schema.decode(&decoded, bytes, length));
    assert(!chronos_settings_schema.encode(&settings, bytes, 1, &length));
}

/* Read a saved envelope to create realistic damage fixtures. */
static size_t read_file(const char* path, uint8_t* bytes, size_t capacity) {
    FILE* file = fopen(path, "rb");
    assert(file);
    size_t length = fread(bytes, 1, capacity, file);
    assert(!ferror(file) && feof(file));
    assert(fclose(file) == 0);
    return length;
}

/* Write a bounded corruption fixture without relying on the production encoder. */
static void write_file(const char* path, const uint8_t* bytes, size_t length) {
    FILE* file = fopen(path, "wb");
    assert(file);
    assert(fwrite(bytes, 1, length, file) == length);
    assert(fclose(file) == 0);
}

/* Exercise missing storage, persistent round trips, damaged files, and safe failed saves. */
static void test_storage(void) {
    char directory[] = "/tmp/chronos-settings-XXXXXX";
    assert(mkdtemp(directory));
    char path[256], temporary[260], backup[260], missing[256];
    snprintf(path, sizeof(path), "%s/%s", directory, "settings.bin");
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    snprintf(missing, sizeof(missing), "%s/absent", directory);
    chronos_settings_t settings = custom_settings(), loaded;
    loaded                      = settings;
    assert(settings_store_load(missing, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_UNAVAILABLE);
    assert_defaults(&loaded);
    assert(access(missing, F_OK) != 0); /* Loading never creates a fake mountpoint. */
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_MISSING);
    assert_defaults(&loaded);
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &settings) == SETTINGS_OK);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_OK);
    assert_equal(&settings, &loaded);
    /* Replacement must work when the destination already exists. */
    settings.alarms[2].hour = 23;
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &settings) == SETTINGS_OK);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_OK);
    assert_equal(&settings, &loaded);
    /* Failed commit restores the old file; failed rollback remains recoverable. */
    chronos_settings_t changed = settings;
    changed.display_percent    = 42;
    fail_rename_calls          = 1;
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &changed) == SETTINGS_IO_ERROR);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_OK);
    assert_equal(&settings, &loaded);
    fail_rename_calls = 2;
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &changed) == SETTINGS_IO_ERROR);
    assert(access(path, F_OK) != 0 && access(backup, F_OK) == 0);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_RECOVERED);
    assert_equal(&settings, &loaded);
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &settings) == SETTINGS_OK);
    assert(access(backup, F_OK) != 0);
    loaded                  = settings;
    loaded.keyboard_percent = 101;
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_INVALID);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_OK);
    assert_equal(&settings, &loaded);
    /* A blocked temporary file simulates an unwritable save path. */
    assert(mkdir(temporary, 0700) == 0);
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &settings) == SETTINGS_IO_ERROR);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_OK);
    assert_equal(&settings, &loaded);
    assert(rmdir(temporary) == 0);
    uint8_t bytes[4096], damaged[4096];
    size_t  length = read_file(path, bytes, sizeof(bytes));
    assert(length > 16 && !memcmp(bytes, "CSET\1\0\0\0", 8));
    /* Header truncation, payload truncation, magic, schema, length, CRC, and trailing data. */
    const size_t truncated[] = {0, 1, 15, 16};
    for (size_t i = 0; i < sizeof(truncated) / sizeof(truncated[0]); ++i) {
        write_file(path, bytes, truncated[i]);
        assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_CORRUPT);
        assert_defaults(&loaded);
    }
    write_file(path, bytes, length - 1);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_CORRUPT);
    assert_defaults(&loaded);
    const size_t mutations[] = {0, 4, 8, 12, 20};
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        memcpy(damaged, bytes, length);
        damaged[mutations[i]] ^= 0x80;
        write_file(path, damaged, length);
        loaded = settings;
        assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_CORRUPT);
        assert_defaults(&loaded);
    }
    memcpy(damaged, bytes, length);
    damaged[length] = 0;
    write_file(path, damaged, length + 1);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_CORRUPT);
    assert_defaults(&loaded);
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &settings) == SETTINGS_OK);
    assert(settings_store_load(directory, "settings.bin", &chronos_settings_schema, &loaded) == SETTINGS_OK);
    assert_equal(&settings, &loaded);
    assert(settings_store_load(directory, "../outside", &chronos_settings_schema, &loaded) == SETTINGS_INVALID);
    assert_defaults(&loaded);
    assert(chronos_settings_notice(SETTINGS_OK) == NULL);
    assert(strstr(chronos_settings_notice(SETTINGS_UNAVAILABLE), "SD card is required"));
    assert(unlink(path) == 0);
    /* A directory at the destination forces rename failure after a complete write. */
    assert(mkdir(path, 0700) == 0);
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &settings) == SETTINGS_IO_ERROR);
    assert(access(temporary, F_OK) != 0);
    assert(rmdir(path) == 0);
    assert(rmdir(directory) == 0);
}

/* Supply a second, unrelated schema to verify storage has no CHRONOS assumptions. */
static void small_defaults(void* object) {
    *(uint8_t*)object = 7;
}

/* Accept a deliberately narrow application value range. */
static bool small_validate(const void* object) {
    return *(const uint8_t*)object <= 10;
}

/* Allow an invalid value only when constructing a checksummed corruption fixture. */
static bool fixture_validate(const void* object) {
    (void)object;
    return true;
}

/* Serialize a one-byte application object. */
static bool small_encode(const void* object, uint8_t* bytes, size_t capacity, size_t* length) {
    if (capacity < 1) return false;
    bytes[0] = *(const uint8_t*)object;
    *length  = 1;
    return true;
}

/* Deliberately leave semantic validation to the store's post-decode check. */
static bool small_decode(void* object, const uint8_t* bytes, size_t length) {
    if (length != 1) return false;
    *(uint8_t*)object = bytes[0];
    return true;
}

/* Verify schema independence and reset after semantically invalid partial decoding. */
static void test_generic_schema(void) {
    char directory[] = "/tmp/chronos-schema-XXXXXX";
    assert(mkdtemp(directory));
    settings_schema_t schema = {.version       = 42,
                                .payload_limit = 1,
                                .defaults      = small_defaults,
                                .validate      = small_validate,
                                .encode        = small_encode,
                                .decode        = small_decode};
    uint8_t           value  = 9;
    assert(settings_store_save(directory, "other.bin", &schema, &value) == SETTINGS_OK);
    value = 0;
    assert(settings_store_load(directory, "other.bin", &schema, &value) == SETTINGS_OK);
    assert(value == 9);
    settings_schema_t fixture = schema;
    fixture.validate          = fixture_validate;
    value                     = 200;
    assert(settings_store_save(directory, "other.bin", &fixture, &value) == SETTINGS_OK);
    assert(settings_store_load(directory, "other.bin", &schema, &value) == SETTINGS_CORRUPT);
    assert(value == 7);
    char path[256];
    snprintf(path, sizeof(path), "%s/other.bin", directory);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
}


static void test_text_settings(void) {
    char directory[] = "/tmp/CHRONOS-text-XXXXXX";
    assert(mkdtemp(directory));
    char path[256], binary[256], backup[260];
    snprintf(path, sizeof(path), "%s/%s", directory, CHRONOS_SETTINGS_FILENAME);
    snprintf(binary, sizeof(binary), "%s/settings.bin", directory);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    chronos_settings_t original = custom_settings(), loaded;
    original.volume_percent = 35;
    original.led_percent = 83;
    original.alarms[0].snooze_minutes = 37;
    strcpy(original.alarms[0].label, "Bell \\\" # = UTF-8 \xe2\x99\xaa");
    assert(chronos_settings_save(directory, &original) == SETTINGS_OK);
    assert(chronos_settings_load(directory, &loaded) == SETTINGS_OK);
    assert(chronos_settings_equal(&original, &loaded));
    uint8_t bytes[16384];
    size_t length = read_file(path, bytes, sizeof(bytes));
    assert(length < sizeof(bytes)); bytes[length] = 0;
    assert(strstr((char*)bytes, "volume_percent=35\n"));
    assert(strstr((char*)bytes, "city.1.name=\"City 0\""));
    assert(!chronos_settings_equal(&original, &(chronos_settings_t){0}));
    /* Default absent fields, accept CRLF and whitespace around keys. */
    const char* partial = "# edited on a computer\r\nformat=1\r\n volume_percent = 0\r\ncity.1.name=\"Edited city\"\r\n";
    write_file(path, (const uint8_t*)partial, strlen(partial));
    assert(chronos_settings_load(directory, &loaded) == SETTINGS_OK);
    assert(loaded.volume_percent == 0 && loaded.display_percent == 80 && loaded.led_percent == 25);
    assert(loaded.font_size == CHRONOS_FONT_LARGE);
    assert(!strcmp(loaded.cities[0].name, "Edited city"));
    const char* invalid[] = {"", "format=2\n", "format=1\nvolume_percent=101\n", "format=1\nvolume_percent=-1\n",
        "format=1\nvolume_percent=1\nvolume_percent=2\n", "format=1\nunknown=1\n",
        "format=1\ncity.1.name=\"unclosed\n", "format=1\ncountdown_seconds=4294967296\n",
        "format=1\nfont_size=2\n", "format=1\nalarm.1.snooze_minutes=61\n", "format=1\nntp_enabled=2\n", "format=1\ntimezone=\"bad\"\n"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        write_file(path, (const uint8_t*)invalid[i], strlen(invalid[i]));
        assert(chronos_settings_load(directory, &loaded) == SETTINGS_CORRUPT);
        assert_defaults(&loaded); assert(loaded.volume_percent == 70);
    }
    /* Same FAT-safe replacement and recovery apply to text files. */
    assert(chronos_settings_save(directory, &original) == SETTINGS_OK);
    chronos_settings_t changed = original; changed.volume_percent = 90;
    fail_rename_calls = 2;
    assert(chronos_settings_save(directory, &changed) == SETTINGS_IO_ERROR);
    assert(chronos_settings_load(directory, &loaded) == SETTINGS_RECOVERED);
    assert(chronos_settings_equal(&original, &loaded));
    assert(chronos_settings_save(directory, &original) == SETTINGS_OK);
    assert(unlink(path) == 0);
    /* Legacy binary remains untouched; migration creates editable text. */
    assert(settings_store_save(directory, "settings.bin", &chronos_settings_schema, &original) == SETTINGS_OK);
    assert(chronos_settings_load(directory, &loaded) == SETTINGS_RECOVERED);
    assert(loaded.volume_percent == 70);
    assert(access(binary, F_OK) == 0 && access(path, F_OK) == 0);
    assert(chronos_settings_load(directory, &loaded) == SETTINGS_OK);
    write_file(path, (const uint8_t*)"invalid", 7);
    assert(chronos_settings_load(directory, &loaded) == SETTINGS_CORRUPT);
    assert_defaults(&loaded); /* Never mask invalid manual edits with old binary settings. */
    assert(unlink(binary) == 0); assert(unlink(path) == 0); assert(rmdir(directory) == 0);
}
static void test_hardware_migration(void) {
    chronos_settings_t loaded, restarted;
    uint8_t bytes[16384];
    size_t length = read_file("tests/host/fixtures/hardware-settings.ini", bytes, sizeof(bytes));
    assert(chronos_settings_text_schema.decode(&loaded, bytes, length));
    assert(loaded.theme == 1 && loaded.font_size == 1 && loaded.ntp_enabled && loaded.use_24_hour);
    assert(!loaded.wifi_enabled && loaded.time_source == CHRONOS_TIME_NTP);
    assert(!strcmp(loaded.alarm_sound, "TF048.WAV"));
    assert(loaded.alarms[0].enabled && loaded.alarms[0].hour == 8 && loaded.alarms[0].minute == 30);
    assert(loaded.alarms[0].weekdays == 42 && !strcmp(loaded.alarms[0].label, "Example alarm"));
    assert(chronos_settings_text_schema.encode(&loaded, bytes, sizeof(bytes)-1, &length));
    bytes[length] = 0;
    assert(!strstr((char*)bytes, ".date."));
    assert(chronos_settings_text_schema.decode(&restarted, bytes, length));
    assert(chronos_settings_equal(&loaded, &restarted));
    const char* legacy = "format=1\nkeyboard_percent=0\ndisplay_percent=4\nled_percent=0\nntp_enabled=0\nalarm.1.date.year=0\n";
    assert(chronos_settings_text_schema.decode(&loaded, (const uint8_t*)legacy, strlen(legacy)));
    assert(loaded.keyboard_percent == 5 && loaded.display_percent == 5 && loaded.led_percent == 0);
    assert(loaded.time_source == CHRONOS_TIME_RTC);
    legacy = "format=1\nkeyboard_percent=255\ndisplay_percent=101\nled_percent=255\nwifi_enabled=1\ntime_source=2\n";
    assert(chronos_settings_text_schema.decode(&loaded, (const uint8_t*)legacy, strlen(legacy)));
    assert(loaded.keyboard_percent == 100 && loaded.display_percent == 100 && loaded.led_percent == 100);
    assert(loaded.wifi_enabled && loaded.time_source == CHRONOS_TIME_MANUAL);
}
static void test_catalogue(void) {
    assert(chronos_zone_count() > 400);
    assert(!chronos_zone(chronos_zone_count()));
    chronos_settings_t settings;
    chronos_settings_defaults(&settings);
    for (size_t i = 0; i < chronos_zone_count(); ++i) {
        settings.cities[0] = *chronos_zone(i);
        strcpy(settings.timezone, chronos_zone(i)->timezone);
        if (!chronos_settings_valid(&settings)) fprintf(stderr, "Invalid SDK zone: %s %s\n", settings.cities[0].name, settings.timezone);
        assert(chronos_settings_valid(&settings));
    }
}

/* Run portable configuration and filesystem regression coverage. */
static void test_font_preference(void) {
    char directory[] = "/tmp/chronos-font-XXXXXX";
    assert(mkdtemp(directory));
    chronos_settings_t settings, loaded;
    chronos_settings_defaults(&settings);
    assert(settings.font_size == CHRONOS_FONT_LARGE);
    for (unsigned size = 0; size < CHRONOS_FONT_SIZE_COUNT; ++size) {
        settings.font_size = size;
        assert(chronos_settings_save(directory, &settings) == SETTINGS_OK);
        memset(&loaded, 0xff, sizeof(loaded));
        assert(chronos_settings_load(directory, &loaded) == SETTINGS_OK);
        assert(loaded.font_size == size && chronos_settings_equal(&loaded, &settings));
    }
    settings.font_size = CHRONOS_FONT_SIZE_COUNT;
    assert(!chronos_settings_valid(&settings));
    char path[256];
    snprintf(path, sizeof(path), "%s/settings.ini", directory);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
}
int main(void) {
    test_hardware_migration();
    test_font_preference();
    test_validation();
    test_codec();
    test_storage();
    test_generic_schema();
    test_text_settings();
    test_catalogue();
    puts("settings: all tests passed");
    return 0;
}
