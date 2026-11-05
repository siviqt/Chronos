#pragma once
#include "FreeRTOS.h"
typedef void* SemaphoreHandle_t;
int           xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t wait);
int           xSemaphoreGive(SemaphoreHandle_t semaphore);
