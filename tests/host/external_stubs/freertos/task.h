#pragma once
#include "FreeRTOS.h"
int  xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* arg, unsigned priority, void* handle);
void vTaskDelay(TickType_t delay);
