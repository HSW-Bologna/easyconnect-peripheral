#include <assert.h>
#include <stdio.h>
#include "esp_log.h"
#include "model/model.h"
#include "peripherals/storage.h"
#include "easyconnect_interface.h"
#include "configuration.h"


#define ADDRESS_KEY             "indirizzo"
#define SERIAL_NUM_KEY          "numeroseriale"
#define MODEL_KEY               "CLASS"
#define FEEDBACK_DIRECTION_KEY  "FBDIR"
#define ACTIVATION_ATTEMPTS_KEY "ACTATT"
#define FEEDBACK_DELAY_KEY      "FBDELAY"
#define FEEDBACK_ENABLE_KEY     "FBENABLE"


void configuration_init(model_t *pmodel) {
    uint16_t value = 0;

    if (load_uint16_option(&value, ADDRESS_KEY) == 0) {
        model_set_address(pmodel, value);
    }
    if (load_uint16_option(&value, SERIAL_NUM_KEY) == 0) {
        model_set_serial_number(pmodel, value);
    }
    if (load_uint16_option(&value, MODEL_KEY) == 0) {
        model_set_class(pmodel, value, NULL);
    }

    uint8_t uint8_value = 0;
    if (load_uint8_option(&uint8_value, FEEDBACK_ENABLE_KEY) == 0) {
        model_set_feedback_enabled(pmodel, uint8_value);
    }

    if (load_uint8_option(&uint8_value, FEEDBACK_DIRECTION_KEY) == 0) {
        model_set_feedback_direction(pmodel, uint8_value);
    }
    if (load_uint8_option(&uint8_value, ACTIVATION_ATTEMPTS_KEY) == 0) {
        model_set_output_attempts(pmodel, uint8_value);
    }
    if (load_uint8_option(&uint8_value, FEEDBACK_DELAY_KEY) == 0) {
        model_set_feedback_delay(pmodel, uint8_value);
    }
}


void configuration_save_serial_number(void *args, uint16_t value) {
    save_uint16_option(&value, SERIAL_NUM_KEY);
    model_set_serial_number(args, value);
}


int configuration_save_class(void *args, uint16_t value) {
    uint16_t corrected;
    if (model_set_class(args, value, &corrected) == 0) {
        save_uint16_option(&corrected, MODEL_KEY);
        return 0;
    } else {
        return -1;
    }
}


void configuration_save_address(void *args, uint16_t value) {
    save_uint16_option(&value, ADDRESS_KEY);
    model_set_address(args, value);
}


void configuration_save_feedback_direction(void *args, uint8_t value) {
    save_uint8_option(&value, FEEDBACK_DIRECTION_KEY);
    model_set_feedback_direction(args, value);
}


void configuration_save_activation_attempts(void *args, uint8_t value) {
    save_uint8_option(&value, ACTIVATION_ATTEMPTS_KEY);
    model_set_output_attempts(args, value);
}


void configuration_save_feedback_delay(void *args, uint8_t value) {
    save_uint8_option(&value, FEEDBACK_DELAY_KEY);
    model_set_feedback_delay(args, value);
}


void configuration_save_feedback_enable(void *args, uint8_t value) {
    save_uint8_option(&value, FEEDBACK_ENABLE_KEY);
    model_set_feedback_enabled(args, value);
}