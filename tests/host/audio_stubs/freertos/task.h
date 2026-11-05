#pragma once
#include <stdint.h>
int xTaskCreate(void (*fn)(void*), const char*, unsigned, void*, unsigned, void*);
void vTaskDelay(uint32_t);
void vTaskDelete(void*);
