#ifndef CHRONOS_SETTINGS_H
#define CHRONOS_SETTINGS_H

#include "chronos_alarms.h"
#include "settings_store.h"
#include "external_display.h"

#define CHRONOS_CITY_LIMIT         5
#define CHRONOS_TIMEZONE_BYTES     128
#define CHRONOS_SETTINGS_FILENAME  "settings.ini"
#define CHRONOS_DEFAULT_NTP_SERVER "pool.ntp.org"

typedef enum {
    /* Stored value zero is Vector; retain the legacy symbol for compatibility. */
    CHRONOS_THEME_MATRIX, CHRONOS_THEME_DISTRICT, CHRONOS_THEME_OPERATOR, CHRONOS_THEME_COUNT
} chronos_theme_t;

typedef enum { CHRONOS_FONT_DEFAULT, CHRONOS_FONT_LARGE, CHRONOS_FONT_SIZE_COUNT } chronos_font_option_t;

typedef struct {
    char name[CHRONOS_ALARM_LABEL_BYTES];
    /* POSIX TZ rule, resolved by the clock adapter; not an IANA zone ID. */
    char timezone[CHRONOS_TIMEZONE_BYTES];
} chronos_city_t;
typedef enum { CHRONOS_TIME_NTP, CHRONOS_TIME_RTC, CHRONOS_TIME_MANUAL } chronos_time_source_t;

typedef struct {
    external_config_t external;
    bool wifi_enabled;
    uint8_t time_source;
    chronos_theme_t        theme;
    uint8_t                font_size;
    bool                   ntp_enabled, use_24_hour, countdown_buzzer;
    char                   ntp_server[254];
    char                   timezone[CHRONOS_TIMEZONE_BYTES];
    uint8_t                keyboard_percent, display_percent, led_percent, volume_percent;
    uint32_t               countdown_seconds, pacer_period_ms;
    uint8_t                city_count;
    chronos_city_t         cities[CHRONOS_CITY_LIMIT];
    chronos_alarm_config_t alarms[CHRONOS_ALARM_LIMIT];
    /* A plain filename beneath the app's SD directory; empty selects built-in audio. */
    char                   alarm_sound[65];
} chronos_settings_t;

/* Binary schema is retained only for backwards-compatible migration. */
extern const settings_schema_t chronos_settings_schema;
extern const settings_schema_t chronos_settings_text_schema;
size_t chronos_zone_count(void);
const chronos_city_t* chronos_zone(size_t index);
bool chronos_settings_equal(const chronos_settings_t* a, const chronos_settings_t* b);
void                           chronos_settings_defaults(chronos_settings_t* settings);
bool                           chronos_settings_valid(const chronos_settings_t* settings);
settings_result_t              chronos_settings_load(const char* directory, chronos_settings_t* settings);
settings_result_t              chronos_settings_save(const char* directory, const chronos_settings_t* settings);
/* A stable message for a future GUI banner, or NULL after successful loading. */
const char*                    chronos_settings_notice(settings_result_t result);

#endif
