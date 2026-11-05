#pragma once
#include <stdint.h>
#include <sys/time.h>
#define SNTP_OPMODE_POLL 0
void esp_sntp_stop(void);
void esp_sntp_init(void);
void esp_sntp_setoperatingmode(int mode);
void esp_sntp_setservername(int index, const char* name);
void esp_sntp_set_time_sync_notification_cb(void (*cb)(struct timeval*));
void esp_sntp_set_sync_interval(uint32_t ms);
