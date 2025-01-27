#ifndef GLOBAL_H
#define GLOBAL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SAVE_QUEUE_LENGTH 10

extern QueueHandle_t save_queue;

void init_save_task(void);

#endif // GLOBAL_H