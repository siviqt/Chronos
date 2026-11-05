#pragma once
#include "FreeRTOS.h"
typedef void* QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
void          vQueueDelete(QueueHandle_t queue);
int           xQueueReceive(QueueHandle_t queue, void* item, TickType_t wait);
int           xQueueOverwrite(QueueHandle_t queue, const void* item);
