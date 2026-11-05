#pragma once
#include <stdint.h>
#include <stddef.h>
typedef void* TaskHandle_t;
int xTaskCreate(void (*fn)(void*), const char*, unsigned, void*, unsigned, TaskHandle_t*);
void vTaskDelay(uint32_t ticks);
void vTaskDelete(TaskHandle_t task);
