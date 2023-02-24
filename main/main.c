#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "model/model.h"
#include "controller/controller.h"
#include "peripherals/system.h"
#include "peripherals/digin.h"
#include "peripherals/digout.h"
#include "peripherals/storage.h"
#include "peripherals/heartbeat.h"
#include "peripherals/rs485.h"
#include "peripherals/hardwareprofile.h"
#include "easyconnect_interface.h"
#include "event_log.h"


static const char *TAG = "Main";


void app_main(void) {
    model_t model;

    system_random_init();
    storage_init();
    rs485_init(EASYCONNECT_BAUDRATE);
    digin_init();
    digout_init();
    heartbeat_init();
    event_log_init();

    model_init(&model);
    controller_init(&model);

    ESP_LOGI(TAG, "Begin main loop");
    for (;;) {
        controller_manage(&model);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
