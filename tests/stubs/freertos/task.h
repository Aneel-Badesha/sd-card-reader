#pragma once

#include "FreeRTOS.h"

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack_depth, void *params, UBaseType_t priority,
                       TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t ticks_to_wait);
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *higher_priority_task_woken);
