#pragma once
#include <stdint.h>
typedef void* QueueHandle_t;
int xQueueReceive(QueueHandle_t queue, void* event, uint32_t ticks);
