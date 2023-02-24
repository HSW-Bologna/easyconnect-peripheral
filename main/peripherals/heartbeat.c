#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hardwareprofile.h"
#include "heartbeat.h"


static const char *TAG = "Heartbeat";

void heartbeat_init(void) {
    (void)TAG;

    gpio_config_t config = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(HAP_LED_COMM) | BIT64(HAP_LED_ACTIVITY),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(HAP_LED_COMM, 0);
    gpio_set_level(HAP_LED_ACTIVITY, 0);
}


void heartbeat_update_green(uint8_t value) {
    gpio_set_level(HAP_LED_COMM, !value);
}


void heartbeat_update_red(uint8_t value) {
    gpio_set_level(HAP_LED_ACTIVITY, !value);
}