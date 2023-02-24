#include "hal/gpio_types.h"
#include "peripherals/hardwareprofile.h"
#include "peripherals/digin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "gel/debounce/debounce.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "digin.h"


#define EVENT_NEW_INPUT 0x01


static debounce_filter_t  filter;
static SemaphoreHandle_t  sem;
static EventGroupHandle_t events;


static void periodic_read(TimerHandle_t timer);


void digin_init(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type     = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask  = BIT64(HAP_SAFETY) | BIT64(HAP_SIGNAL);
    io_conf.mode          = GPIO_MODE_INPUT;
    io_conf.pull_down_en  = 0;
    io_conf.pull_up_en    = 0;
    gpio_config(&io_conf);

    static StaticEventGroup_t event_group_buffer;
    events = xEventGroupCreateStatic(&event_group_buffer);

    debounce_filter_init(&filter);
    static StaticSemaphore_t semaphore_buffer;
    sem = xSemaphoreCreateMutexStatic(&semaphore_buffer);

    static StaticTimer_t timer_buffer;
    TimerHandle_t        timer =
        xTimerCreateStatic("timerInput", pdMS_TO_TICKS(10), pdTRUE, NULL, periodic_read, &timer_buffer);
    xTimerStart(timer, portMAX_DELAY);
}


int digin_get(digin_t digin) {
    int res = 0;
    xSemaphoreTake(sem, portMAX_DELAY);
    res = debounce_read(&filter, digin);
    xSemaphoreGive(sem);
    return res;
}


int digin_take_reading(void) {
    unsigned int input = 0;
    input |= !gpio_get_level(HAP_SAFETY);
    input |= (!gpio_get_level(HAP_SIGNAL)) << 1;
    return debounce_filter(&filter, input, 5);
}


unsigned int digin_get_inputs(void) {
    return debounce_value(&filter);
}


uint8_t digin_is_value_ready(void) {
    uint8_t res = xEventGroupGetBits(events) & EVENT_NEW_INPUT;
    xEventGroupClearBits(events, EVENT_NEW_INPUT);
    return res > 0;
}


static void periodic_read(TimerHandle_t timer) {
    (void)timer;
    xSemaphoreTake(sem, portMAX_DELAY);
    if (digin_take_reading()) {
        xEventGroupSetBits(events, EVENT_NEW_INPUT);
    }
    xSemaphoreGive(sem);
}
