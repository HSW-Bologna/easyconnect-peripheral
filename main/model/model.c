#include "model.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "easyconnect.h"
#include "easyconnect_interface.h"
#include "app_config.h"
#include "esp_log.h"


static uint8_t valid_mode(uint16_t mode);


static const char *TAG = "Model";


void model_init(model_t *pmodel) {
    (void)TAG;
    pmodel->sem = xSemaphoreCreateMutexStatic(&pmodel->semaphore_buffer);

    pmodel->address            = EASYCONNECT_DEFAULT_MINION_ADDRESS;
    pmodel->serial_number      = EASYCONNECT_DEFAULT_MINION_SERIAL_NUMBER;
    pmodel->class              = EASYCONNECT_DEFAULT_DEVICE_CLASS;
    pmodel->feedback_enabled   = 1;
    pmodel->feedback_direction = EASYCONNECT_DEFAULT_FEEDBACK_LEVEL;
    pmodel->output_attempts    = EASYCONNECT_DEFAULT_ACTIVATE_ATTEMPTS;
    pmodel->feedback_delay     = EASYCONNECT_DEFAULT_FEEDBACK_DELAY;
}


uint16_t model_get_class(void *arg) {
    assert(arg != NULL);
    model_t *pmodel = arg;

    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    uint16_t result = (pmodel->class & CLASS_CONFIGURABLE_MASK) | (APP_CONFIG_HARDWARE_MODEL << 12);
    xSemaphoreGive(pmodel->sem);

    return result;
}


int model_set_class(void *arg, uint16_t class, uint16_t *out_class) {
    assert(arg != NULL);
    model_t *pmodel = arg;

    uint16_t corrected = class & CLASS_CONFIGURABLE_MASK;
    uint16_t mode      = CLASS_GET_MODE(corrected | (APP_CONFIG_HARDWARE_MODEL << 12));

    if (valid_mode(mode)) {
        if (out_class != NULL) {
            *out_class = corrected;
        }
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);
        pmodel->class = corrected;
        xSemaphoreGive(pmodel->sem);
        return 0;
    } else {
        return -1;
    }
}


static uint8_t valid_mode(uint16_t mode) {
    switch (mode) {
        case DEVICE_MODE_LIGHT:
        case DEVICE_MODE_UVC:
        case DEVICE_MODE_ESF:
        case DEVICE_MODE_GAS:
            return 1;
        default:
            return 0;
    }
}