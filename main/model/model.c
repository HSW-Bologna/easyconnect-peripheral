#include <string.h>
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
    pmodel->work_seconds       = 0;
    pmodel->work_time_to_save  = 0;

    pmodel->output_attempts_exceeded = 0;
    pmodel->missing_heartbeat        = 0;
    pmodel->safety_bypass            = 0;

    memset(pmodel->safety_message, 0, sizeof(pmodel->safety_message));
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


void model_get_safety_message(void *args, char *string) {
    model_t *pmodel = args;
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    strcpy(string, pmodel->safety_message);
    xSemaphoreGive(pmodel->sem);
}


void model_set_safety_message(model_t *pmodel, const char *string) {
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    snprintf(pmodel->safety_message, sizeof(pmodel->safety_message), "%s", string);
    xSemaphoreGive(pmodel->sem);
}


void model_get_feedback_message(void *args, char *string) {
    model_t *pmodel = args;
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    strcpy(string, pmodel->feedback_message);
    xSemaphoreGive(pmodel->sem);
}


void model_set_feedback_message(model_t *pmodel, const char *string) {
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    snprintf(pmodel->feedback_message, sizeof(pmodel->feedback_message), "%s", string);
    xSemaphoreGive(pmodel->sem);
}


const char *model_default_safety_message(model_t *pmodel) {
    switch (CLASS_GET_MODE(pmodel->class)) {
        case DEVICE_MODE_LIGHT:
            return "Warning: open door";

        case DEVICE_MODE_UVC:

        default:
            return "ERROR";
    }
}


void model_increase_work_seconds(model_t *pmodel, uint32_t seconds) {
    if (seconds > 0) {
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);
        pmodel->work_seconds += seconds;
        pmodel->work_time_to_save = 1;
        xSemaphoreGive(pmodel->sem);
    }
}


void model_reset_work_seconds(model_t *pmodel) {
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    pmodel->work_seconds      = 0;
    pmodel->work_time_to_save = 1;
    xSemaphoreGive(pmodel->sem);
}


uint16_t model_get_work_hours(model_t *pmodel) {
    uint16_t result = 0;
    xSemaphoreTake(pmodel->sem, portMAX_DELAY);
    result = pmodel->work_seconds / (60UL * 60UL);
    xSemaphoreGive(pmodel->sem);
    return result;
}


uint8_t model_is_safety_mode(model_t *model) {
    return CLASS_GET_MODE(model->class) == DEVICE_MODE_SAFETY;
}


static uint8_t valid_mode(uint16_t mode) {
    switch (mode) {
        case DEVICE_MODE_LIGHT:
        case DEVICE_MODE_UVC:
        case DEVICE_MODE_ESF:
        case DEVICE_MODE_GAS:
        case DEVICE_MODE_SAFETY:
            return 1;
        default:
            return 0;
    }
}
