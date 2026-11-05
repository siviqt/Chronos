#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "chronos_settings.h"

/* Match the ASCII grammar used by host names and POSIX timezone rules. */
static bool alpha(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Parse a bounded decimal field without accepting missing digits. */
static bool number(const char** text, unsigned int low, unsigned int high) {
    const char*  p     = *text;
    unsigned int value = 0;
    if (*p < '0' || *p > '9') return false;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (unsigned int)(*p++ - '0');
        if (value > high) return false;
    }
    *text = p;
    return value >= low;
}

/* Parse an optional sign and timezone or transition hour/minute/second value. */
static bool offset(const char** text, unsigned int max_hour) {
    if (**text == '+' || **text == '-') ++*text;
    if (!number(text, 0, max_hour)) return false;
    if (**text == ':') {
        ++*text;
        if (!number(text, 0, 59)) return false;
        if (**text == ':') {
            ++*text;
            if (!number(text, 0, 59)) return false;
        }
    }
    return true;
}

/* Parse a POSIX abbreviation, including numeric names enclosed in angle brackets. */
static bool zone_name(const char** text) {
    const char* start;
    if (**text == '<') {
        start = ++*text;
        while (alpha((unsigned char)**text) || (**text >= '0' && **text <= '9') || **text == '+' || **text == '-')
            ++*text;
        if (*text - start < 3 || **text != '>') return false;
        ++*text;
    } else {
        start = *text;
        while (alpha((unsigned char)**text)) ++*text;
        if (*text - start < 3) return false;
    }
    return true;
}

/* Parse month/week/day, Julian, or ordinal DST rules with optional transition times. */
static bool transition(const char** text) {
    if (**text == 'M') {
        ++*text;
        if (!number(text, 1, 12) || **text != '.') return false;
        ++*text;
        if (!number(text, 1, 5) || **text != '.') return false;
        ++*text;
        if (!number(text, 0, 6)) return false;
    } else if (**text == 'J') {
        ++*text;
        if (!number(text, 1, 365)) return false;
    } else if (!number(text, 0, 365))
        return false;
    if (**text == '/') {
        ++*text;
        return offset(text, 167);
    }
    return true;
}

/* Require explicit DST rules so behavior does not depend on host timezone databases. */
static bool timezone_valid(const char* text) {
    if (!chronos_text_valid(text, CHRONOS_TIMEZONE_BYTES, CHRONOS_TIMEZONE_BYTES - 1)) return false;
    if (!zone_name(&text) || !offset(&text, 24)) return false;
    if (!*text) return true;
    if (!zone_name(&text)) return false;
    if (*text != ',' && !offset(&text, 24)) return false;
    if (*text != ',') return false;
    ++text;
    if (!transition(&text) || *text != ',') return false;
    ++text;
    return transition(&text) && !*text;
}

/* Accept DNS/IPv4 server names; reject URLs, whitespace, and malformed labels. */
static bool server_valid(const char* server) {
    size_t length = 0, label_length = 0;
    if (!chronos_text_valid(server, 254, 253) || !server[0]) return false;
    while (server[length]) {
        unsigned char c = (unsigned char)server[length];
        if (c == '.') {
            if (!label_length || server[length - 1] == '-') return false;
            label_length = 0;
        } else {
            if (!alpha(c) && !(c >= '0' && c <= '9') && c != '-') return false;
            if ((!label_length && c == '-') || ++label_length > 63) return false;
        }
        ++length;
    }
    return label_length && server[length - 1] != '-';
}

/* Restrict custom sound lookup to a single filename within the dedicated directory. */
static bool sound_valid(const char* sound) {
    if (!chronos_text_valid(sound, 65, 64) || !strcmp(sound, ".") || !strcmp(sound, "..")) return false;
    for (size_t i = 0; sound[i]; ++i) {
        unsigned char c = (unsigned char)sound[i];
        if (!alpha(c) && !(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

/* Provide usable offline defaults, including the initial speaker volume. */
void chronos_settings_defaults(chronos_settings_t* settings) {
    *settings = (chronos_settings_t){.external = EXTERNAL_CONFIG_DEFAULT, .volume_percent    = 70,
                                     .font_size         = CHRONOS_FONT_LARGE,
                                     .ntp_enabled       = true,
                                     .use_24_hour       = true,
                                     .countdown_buzzer  = true,
                                     .keyboard_percent  = 25,
                                     .display_percent   = 80,
                                     .led_percent       = 25,
                                     .countdown_seconds = 300,
                                     .pacer_period_ms   = 1000};
    strcpy(settings->ntp_server, CHRONOS_DEFAULT_NTP_SERVER);
    strcpy(settings->timezone, "UTC0");
    const chronos_city_t cities[CHRONOS_CITY_LIMIT] = {
        {"London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
        {"New York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
        {"Tokyo", "JST-9"}, {"Sydney", "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
        {"Sofia", "EET-2EEST,M3.5.0/3,M10.5.0/4"}
    };
    settings->city_count = CHRONOS_CITY_LIMIT;
    memcpy(settings->cities, cities, sizeof(cities));
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) chronos_alarm_defaults(&settings->alarms[i]);
}

/* Validate the entire configuration before saving or applying decoded values. */
bool chronos_settings_valid(const chronos_settings_t* settings) {
    if (settings->time_source > CHRONOS_TIME_MANUAL || settings->font_size >= CHRONOS_FONT_SIZE_COUNT || (unsigned)settings->theme >= CHRONOS_THEME_COUNT || settings->volume_percent > 100 || settings->keyboard_percent < 5 || settings->keyboard_percent > 100 || settings->display_percent < 5 || settings->display_percent > 100 || settings->led_percent > 100 || settings->countdown_seconds == 0 ||
        settings->countdown_seconds > 3600 || settings->pacer_period_ms == 0 ||
        settings->city_count > CHRONOS_CITY_LIMIT || !server_valid(settings->ntp_server) ||
        !timezone_valid(settings->timezone) || !sound_valid(settings->alarm_sound))
        return false;
    for (size_t i = 0; i < CHRONOS_CITY_LIMIT; ++i) {
        if (!chronos_text_valid(settings->cities[i].name, sizeof(settings->cities[i].name), 39) ||
            !timezone_valid(settings->cities[i].timezone) || (i < settings->city_count && !settings->cities[i].name[0]))
            return false;
    }
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i)
        if (!chronos_alarm_config_valid(&settings->alarms[i])) return false;
    return true;
}

typedef struct {
    const uint8_t* input;
    uint8_t*       output;
    size_t         position, capacity;
    bool           valid;
} codec_t;

/* Transfer one byte with an explicit bounds check for both codec directions. */
static uint8_t byte(codec_t* codec, uint8_t value) {
    if (!codec->valid || codec->position >= codec->capacity) {
        codec->valid = false;
        return 0;
    }
    if (codec->input)
        value = codec->input[codec->position];
    else
        codec->output[codec->position] = value;
    ++codec->position;
    return value;
}

/* Transfer a fixed-width little-endian integer rather than native memory layout. */
static uint32_t integer(codec_t* codec, uint32_t value, unsigned int width) {
    uint32_t result = 0;
    for (unsigned int i = 0; i < width; ++i) result |= (uint32_t)byte(codec, (uint8_t)(value >> (i * 8))) << (i * 8);
    return result;
}

/* Reject noncanonical boolean bytes before converting them into C bool values. */
static bool boolean(codec_t* codec, bool value) {
    uint8_t encoded = byte(codec, value ? 1 : 0);
    if (encoded > 1) codec->valid = false;
    return encoded == 1;
}

/* Transfer a length-prefixed string with no embedded terminators or unused padding. */
static void string(codec_t* codec, char* text, size_t capacity) {
    size_t length = codec->input ? 0 : strlen(text);
    length        = integer(codec, (uint32_t)length, 2);
    if (length >= capacity) {
        codec->valid = false;
        return;
    }
    for (size_t i = 0; i < length; ++i) {
        uint8_t value = byte(codec, codec->input ? 0 : (uint8_t)text[i]);
        if (!value) codec->valid = false;
        text[i] = (char)value;
    }
    text[length] = '\0';
}

/* Define the version-one wire field order once for both encoding and decoding. */
static bool transfer(codec_t* codec, chronos_settings_t* settings) {
    settings->ntp_enabled      = boolean(codec, settings->ntp_enabled);
    settings->use_24_hour      = boolean(codec, settings->use_24_hour);
    settings->countdown_buzzer = boolean(codec, settings->countdown_buzzer);
    string(codec, settings->ntp_server, sizeof(settings->ntp_server));
    string(codec, settings->timezone, sizeof(settings->timezone));
    settings->keyboard_percent  = byte(codec, settings->keyboard_percent);
    settings->display_percent   = byte(codec, settings->display_percent);
    settings->countdown_seconds = integer(codec, settings->countdown_seconds, 4);
    settings->pacer_period_ms   = integer(codec, settings->pacer_period_ms, 4);
    settings->city_count        = byte(codec, settings->city_count);
    for (size_t i = 0; i < CHRONOS_CITY_LIMIT; ++i) {
        string(codec, settings->cities[i].name, sizeof(settings->cities[i].name));
        string(codec, settings->cities[i].timezone, sizeof(settings->cities[i].timezone));
    }
    for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i) {
        chronos_alarm_config_t* alarm = &settings->alarms[i];
        alarm->enabled                = boolean(codec, alarm->enabled);
        alarm->repeat                 = (chronos_alarm_repeat_t)byte(codec, (uint8_t)alarm->repeat);
        /* Consume obsolete version-one binary date bytes without using them. */
        (void)integer(codec, 1970, 2);
        (void)byte(codec, 1);
        (void)byte(codec, 1);
        alarm->hour                   = byte(codec, alarm->hour);
        alarm->minute                 = byte(codec, alarm->minute);
        alarm->snooze_minutes         = byte(codec, alarm->snooze_minutes);
        alarm->ring_seconds           = byte(codec, alarm->ring_seconds);
        string(codec, alarm->label, sizeof(alarm->label));
    }
    string(codec, settings->alarm_sound, sizeof(settings->alarm_sound));
    /* Optional version-one extension: older payloads retain Monday-Friday defaults. */
    if (!codec->input || codec->position < codec->capacity)
        for (size_t i = 0; i < CHRONOS_ALARM_LIMIT; ++i)
            settings->alarms[i].weekdays = byte(codec, settings->alarms[i].weekdays);
    /* Version-one tails: absent, weekdays only, or weekdays plus theme. */
    if (!codec->input || codec->position < codec->capacity)
        settings->theme = (chronos_theme_t)byte(codec, (uint8_t)settings->theme);
    return codec->valid;
}

/* Adapt typed defaults to the reusable settings schema callback. */
static void schema_defaults(void* object) {
    chronos_settings_defaults(object);
}

/* Adapt typed validation without leaking application rules into the storage component. */
static bool schema_validate(const void* object) {
    return chronos_settings_valid(object);
}

/* Encode through a heap copy to preserve const ownership and bound embedded stack use. */
static bool schema_encode(const void* object, uint8_t* bytes, size_t capacity, size_t* length) {
    if (!chronos_settings_valid(object)) return false;
    chronos_settings_t* copy = malloc(sizeof(*copy));
    if (!copy) return false;
    *copy          = *(const chronos_settings_t*)object;
    codec_t codec  = {.output = bytes, .capacity = capacity, .valid = true};
    bool    result = transfer(&codec, copy);
    *length        = codec.position;
    free(copy);
    return result;
}

static void normalize_loaded(chronos_settings_t* s) {
    if (s->keyboard_percent < 5) s->keyboard_percent = 5;
    if (s->keyboard_percent > 100) s->keyboard_percent = 100;
    if (s->display_percent < 5) s->display_percent = 5;
    if (s->display_percent > 100) s->display_percent = 100;
    if (s->led_percent > 100) s->led_percent = 100;
}

/* Decode exact-length payloads; validation rejects semantically invalid but checksummed data. */
static bool schema_decode(void* object, const uint8_t* bytes, size_t length) {
    chronos_settings_defaults(object);
    codec_t codec = {.input = bytes, .capacity = length, .valid = true};
    bool valid = transfer(&codec, object) && codec.position == length;
    ((chronos_settings_t*)object)->time_source = ((chronos_settings_t*)object)->ntp_enabled ? CHRONOS_TIME_NTP : CHRONOS_TIME_RTC;
    normalize_loaded(object);
    return valid && chronos_settings_valid(object);
}

const settings_schema_t chronos_settings_schema = {.version       = 1,
                                                   .payload_limit = 4096,
                                                   .defaults      = schema_defaults,
                                                   .validate      = schema_validate,
                                                   .encode        = schema_encode,
                                                   .decode        = schema_decode};

/* Load from the app's dedicated mounted directory, retaining defaults on any failure. */
settings_result_t chronos_settings_load(const char* directory, chronos_settings_t* settings) {
    settings_result_t result = settings_store_load(directory, CHRONOS_SETTINGS_FILENAME, &chronos_settings_text_schema, settings);
    if (result != SETTINGS_MISSING) return result;
    /* Never fall back over a corrupt text file: manual edits remain authoritative. */
    result = settings_store_load(directory, "settings.bin", &chronos_settings_schema, settings);
    if (result == SETTINGS_OK || result == SETTINGS_RECOVERED) {
        settings_result_t saved = chronos_settings_save(directory, settings);
        return saved == SETTINGS_OK ? SETTINGS_RECOVERED : saved;
    }
    return result;
}

/* Save a validated snapshot without persisting volatile alarm or lap-history state. */
settings_result_t chronos_settings_save(const char* directory, const chronos_settings_t* settings) {
    return settings_store_save(directory, CHRONOS_SETTINGS_FILENAME, &chronos_settings_text_schema, settings);
}

/* Supply actionable storage notices while the GUI integration is still pending. */
const char* chronos_settings_notice(settings_result_t result) {
    switch (result) {
        case SETTINGS_OK:
            return NULL;
        case SETTINGS_RECOVERED:
            return "Recovered or migrated settings.";
        case SETTINGS_MISSING:
            return "No saved settings. Using defaults.";
        case SETTINGS_UNAVAILABLE:
            return "SD card is required for saved configuration. Changes are session-only.";
        case SETTINGS_CORRUPT:
            return "Settings are damaged or unsupported. Using defaults.";
        case SETTINGS_IO_ERROR:
            return "Settings storage could not be accessed. Changes may not be saved.";
        case SETTINGS_NO_MEMORY:
            return "Not enough memory to access settings.";
        case SETTINGS_INVALID:
            return "Settings or storage path are invalid.";
    }
    return "Settings could not be loaded.";
}

/* Flat INI keys keep every persisted field visible without a parser dependency.
 * Missing fields use defaults; malformed/unknown/duplicate keys reject the file. */
typedef enum { FIELD_BOOL, FIELD_U8, FIELD_U16, FIELD_U32, FIELD_THEME, FIELD_REPEAT, FIELD_STRING } field_type_t;
typedef struct { const char* key; size_t offset, size; field_type_t type; } field_t;
#define FIELD(key, member, type) {key, offsetof(chronos_settings_t, member), sizeof(((chronos_settings_t*)0)->member), type}
static const field_t fields[] = {
    FIELD("external.enabled", external.enabled, FIELD_BOOL),
    FIELD("external.driver", external.driver, FIELD_U8),
    FIELD("external.content", external.content, FIELD_U8),
    FIELD("external.clk", external.clk, FIELD_U8),
    FIELD("external.din", external.din, FIELD_U8),
    FIELD("external.ce", external.ce, FIELD_U8),
    FIELD("external.dc", external.dc, FIELD_U8),
    FIELD("external.rst", external.rst, FIELD_U8),
    FIELD("external.contrast", external.contrast, FIELD_U8),
    FIELD("external.vfd_bus", external.vfd_bus, FIELD_U8),
    FIELD("external.vfd_address", external.vfd_address, FIELD_U8),
    FIELD("external.vfd_brightness", external.vfd_brightness, FIELD_U8),
    FIELD("external.world_city", external.world_city, FIELD_U8),

    FIELD("theme", theme, FIELD_THEME),
    FIELD("font_size", font_size, FIELD_U8),
    FIELD("wifi_enabled", wifi_enabled, FIELD_BOOL),
    FIELD("time_source", time_source, FIELD_U8),
    FIELD("ntp_enabled", ntp_enabled, FIELD_BOOL),
    FIELD("use_24_hour", use_24_hour, FIELD_BOOL),
    FIELD("countdown_buzzer", countdown_buzzer, FIELD_BOOL),
    FIELD("ntp_server", ntp_server, FIELD_STRING),
    FIELD("timezone", timezone, FIELD_STRING),
    FIELD("keyboard_percent", keyboard_percent, FIELD_U8),
    FIELD("display_percent", display_percent, FIELD_U8),
    FIELD("led_percent", led_percent, FIELD_U8),
    FIELD("volume_percent", volume_percent, FIELD_U8),
    FIELD("countdown_seconds", countdown_seconds, FIELD_U32),
    FIELD("pacer_period_ms", pacer_period_ms, FIELD_U32),
    FIELD("city_count", city_count, FIELD_U8),
    FIELD("alarm_sound", alarm_sound, FIELD_STRING),
    FIELD("city.1.name", cities[0].name, FIELD_STRING),
    FIELD("city.1.timezone", cities[0].timezone, FIELD_STRING),
    FIELD("alarm.1.enabled", alarms[0].enabled, FIELD_BOOL),
    FIELD("alarm.1.repeat", alarms[0].repeat, FIELD_REPEAT),
    FIELD("alarm.1.hour", alarms[0].hour, FIELD_U8),
    FIELD("alarm.1.minute", alarms[0].minute, FIELD_U8),
    FIELD("alarm.1.snooze_minutes", alarms[0].snooze_minutes, FIELD_U8),
    FIELD("alarm.1.ring_seconds", alarms[0].ring_seconds, FIELD_U8),
    FIELD("alarm.1.weekdays", alarms[0].weekdays, FIELD_U8),
    FIELD("alarm.1.label", alarms[0].label, FIELD_STRING),
    FIELD("city.2.name", cities[1].name, FIELD_STRING),
    FIELD("city.2.timezone", cities[1].timezone, FIELD_STRING),
    FIELD("alarm.2.enabled", alarms[1].enabled, FIELD_BOOL),
    FIELD("alarm.2.repeat", alarms[1].repeat, FIELD_REPEAT),
    FIELD("alarm.2.hour", alarms[1].hour, FIELD_U8),
    FIELD("alarm.2.minute", alarms[1].minute, FIELD_U8),
    FIELD("alarm.2.snooze_minutes", alarms[1].snooze_minutes, FIELD_U8),
    FIELD("alarm.2.ring_seconds", alarms[1].ring_seconds, FIELD_U8),
    FIELD("alarm.2.weekdays", alarms[1].weekdays, FIELD_U8),
    FIELD("alarm.2.label", alarms[1].label, FIELD_STRING),
    FIELD("city.3.name", cities[2].name, FIELD_STRING),
    FIELD("city.3.timezone", cities[2].timezone, FIELD_STRING),
    FIELD("alarm.3.enabled", alarms[2].enabled, FIELD_BOOL),
    FIELD("alarm.3.repeat", alarms[2].repeat, FIELD_REPEAT),
    FIELD("alarm.3.hour", alarms[2].hour, FIELD_U8),
    FIELD("alarm.3.minute", alarms[2].minute, FIELD_U8),
    FIELD("alarm.3.snooze_minutes", alarms[2].snooze_minutes, FIELD_U8),
    FIELD("alarm.3.ring_seconds", alarms[2].ring_seconds, FIELD_U8),
    FIELD("alarm.3.weekdays", alarms[2].weekdays, FIELD_U8),
    FIELD("alarm.3.label", alarms[2].label, FIELD_STRING),
    FIELD("city.4.name", cities[3].name, FIELD_STRING),
    FIELD("city.4.timezone", cities[3].timezone, FIELD_STRING),
    FIELD("alarm.4.enabled", alarms[3].enabled, FIELD_BOOL),
    FIELD("alarm.4.repeat", alarms[3].repeat, FIELD_REPEAT),
    FIELD("alarm.4.hour", alarms[3].hour, FIELD_U8),
    FIELD("alarm.4.minute", alarms[3].minute, FIELD_U8),
    FIELD("alarm.4.snooze_minutes", alarms[3].snooze_minutes, FIELD_U8),
    FIELD("alarm.4.ring_seconds", alarms[3].ring_seconds, FIELD_U8),
    FIELD("alarm.4.weekdays", alarms[3].weekdays, FIELD_U8),
    FIELD("alarm.4.label", alarms[3].label, FIELD_STRING),
    FIELD("city.5.name", cities[4].name, FIELD_STRING),
    FIELD("city.5.timezone", cities[4].timezone, FIELD_STRING),
    FIELD("alarm.5.enabled", alarms[4].enabled, FIELD_BOOL),
    FIELD("alarm.5.repeat", alarms[4].repeat, FIELD_REPEAT),
    FIELD("alarm.5.hour", alarms[4].hour, FIELD_U8),
    FIELD("alarm.5.minute", alarms[4].minute, FIELD_U8),
    FIELD("alarm.5.snooze_minutes", alarms[4].snooze_minutes, FIELD_U8),
    FIELD("alarm.5.ring_seconds", alarms[4].ring_seconds, FIELD_U8),
    FIELD("alarm.5.weekdays", alarms[4].weekdays, FIELD_U8),
    FIELD("alarm.5.label", alarms[4].label, FIELD_STRING),
};
#undef FIELD
#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))
static uint32_t field_number(const void* p, field_type_t type) {
    switch (type) {
        case FIELD_BOOL: return *(const bool*)p;
        case FIELD_U8: return *(const uint8_t*)p;
        case FIELD_U16: return *(const uint16_t*)p;
        case FIELD_U32: return *(const uint32_t*)p;
        case FIELD_THEME: return *(const chronos_theme_t*)p;
        case FIELD_REPEAT: return *(const chronos_alarm_repeat_t*)p;
        default: return 0;
    }
}
static bool text_encode(const void* object, uint8_t* bytes, size_t capacity, size_t* length) {
    if (!chronos_settings_valid(object)) return false;
    size_t used = 0;
    const char* header = "# CHRONOS settings: quoted UTF-8 strings; numeric booleans (0/1).\nformat=1\n";
    if (capacity <= strlen(header)) return false;
    memcpy(bytes, header, strlen(header)); used = strlen(header);
    for (size_t i = 0; i < FIELD_COUNT; ++i) {
        const field_t* f = &fields[i];
        const void* value = (const uint8_t*)object + f->offset;
        char line[768];
        int n = snprintf(line, sizeof(line), "%s=", f->key);
        if (f->type == FIELD_STRING) {
            line[n++] = '"';
            const char* text = value;
            for (size_t j = 0; text[j]; ++j) {
                if (text[j] == '"' || text[j] == '\\') line[n++] = '\\';
                line[n++] = text[j];
            }
            line[n++] = '"'; line[n++] = '\n';
        } else n += snprintf(line + n, sizeof(line) - (size_t)n, "%lu\n", (unsigned long)field_number(value, f->type));
        if ((size_t)n > capacity - used) return false;
        memcpy(bytes + used, line, (size_t)n); used += (size_t)n;
    }
    *length = used;
    return true;
}
static char* trim(char* text) {
    while (*text == ' ' || *text == '\t' || *text == '\r') ++text;
    size_t n = strlen(text);
    while (n && (text[n-1] == ' ' || text[n-1] == '\t' || text[n-1] == '\r')) text[--n] = 0;
    return text;
}
static bool text_value(void* object, const field_t* f, char* text) {
    void* value = (uint8_t*)object + f->offset;
    if (f->type == FIELD_STRING) {
        if (*text++ != '"') return false;
        size_t n = 0;
        char* out = value;
        while (*text && *text != '"') {
            if (*text == '\\') {
                ++text;
                if (*text != '\\' && *text != '"') return false;
            }
            if (n + 1 >= f->size) return false;
            out[n++] = *text++;
        }
        out[n] = 0;
        return *text == '"' && !text[1];
    }
    if (*text < '0' || *text > '9') return false;
    errno = 0;
    char* end;
    unsigned long number = strtoul(text, &end, 10);
    if (*end || errno || number > UINT32_MAX) return false;
    switch (f->type) {
        case FIELD_BOOL: if (number > 1) return false; *(bool*)value = number != 0; break;
        case FIELD_U8: if (number > UINT8_MAX) return false; *(uint8_t*)value = (uint8_t)number; break;
        case FIELD_U16: if (number > UINT16_MAX) return false; *(uint16_t*)value = (uint16_t)number; break;
        case FIELD_U32: *(uint32_t*)value = (uint32_t)number; break;
        case FIELD_THEME: if (number >= CHRONOS_THEME_COUNT) return false; *(chronos_theme_t*)value = (chronos_theme_t)number; break;
        case FIELD_REPEAT: if (number > 2) return false; *(chronos_alarm_repeat_t*)value = (chronos_alarm_repeat_t)number; break;
        default: return false;
    }
    return true;
}
static bool text_decode(void* object, const uint8_t* bytes, size_t length) {
    chronos_settings_defaults(object);
    bool seen[FIELD_COUNT] = {0}, version = false;
    size_t offset = 0;
    while (offset < length) {
        char line[768]; size_t n = 0;
        while (offset < length && bytes[offset] != '\n') {
            if (!bytes[offset] || n + 1 >= sizeof(line)) return false;
            line[n++] = (char)bytes[offset++];
        }
        if (offset < length) ++offset;
        line[n] = 0;
        char* key = trim(line);
        if (!*key || *key == '#' || *key == ';') continue;
        char* value = strchr(key, '=');
        if (!value) return false;
        *value++ = 0; key = trim(key); value = trim(value);
        if (!strcmp(key, "format")) {
            if (version || strcmp(value, "1")) return false;
            version = true; continue;
        }
        /* Legacy alarm dates are intentionally ignored, never re-emitted. */
        if (!strncmp(key, "alarm.", 6) && key[6] >= '1' && key[6] <= '5' &&
            (!strcmp(key + 7, ".date.year") || !strcmp(key + 7, ".date.month") || !strcmp(key + 7, ".date.day"))) continue;
        size_t i;
        for (i = 0; i < FIELD_COUNT && strcmp(key, fields[i].key); ++i) {}
        if (i == FIELD_COUNT || seen[i] || !text_value(object, &fields[i], value)) return false;
        seen[i] = true;
    }
    chronos_settings_t* loaded = object;
    bool source_seen = false;
    for (size_t i = 0; i < FIELD_COUNT; ++i)
        if (!strcmp(fields[i].key, "time_source")) source_seen = seen[i];
    if (!source_seen) loaded->time_source = loaded->ntp_enabled ? CHRONOS_TIME_NTP : CHRONOS_TIME_RTC;
    normalize_loaded(object);
    return version && chronos_settings_valid(object);
}
const settings_schema_t chronos_settings_text_schema = {
    .text = true, .version = 1, .payload_limit = 16384,
    .defaults = schema_defaults, .validate = schema_validate,
    .encode = text_encode, .decode = text_decode
};
bool chronos_settings_equal(const chronos_settings_t* a, const chronos_settings_t* b) {
    for (size_t i = 0; i < FIELD_COUNT; ++i) {
        const field_t* f = &fields[i];
        const void* left = (const uint8_t*)a + f->offset;
        const void* right = (const uint8_t*)b + f->offset;
        if (f->type == FIELD_STRING ? strcmp(left, right) != 0 : field_number(left, f->type) != field_number(right, f->type)) return false;
    }
    return true;
}

#include "timezone_catalogue.inc"
size_t chronos_zone_count(void) { return timezones_len; }
const chronos_city_t* chronos_zone(size_t index) { return index < timezones_len ? &timezones[index] : NULL; }
