#ifndef MODEL_H_INCLUDED
#define MODEL_H_INCLUDED

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "easyconnect_interface.h"


#define EASYCONNECT_DEFAULT_MINION_ADDRESS       1
#define EASYCONNECT_DEFAULT_MINION_SERIAL_NUMBER 2
#define EASYCONNECT_DEFAULT_DEVICE_CLASS         CLASS(DEVICE_MODE_LIGHT, DEVICE_GROUP_1)
#define EASYCONNECT_DEFAULT_FEEDBACK_LEVEL       0x0
#define EASYCONNECT_DEFAULT_ACTIVATE_ATTEMPTS    1
#define EASYCONNECT_DEFAULT_FEEDBACK_DELAY       4

#define EASYCONNECT_PARAMETER_MAX_FEEDBACK_DIRECTION  1
#define EASYCONNECT_PARAMETER_MAX_ACTIVATION_ATTEMPTS 8
#define EASYCONNECT_PARAMETER_MAX_FEEDBACK_DELAY      8


#define GETTER_UNSAFE(name, field)                                                                                     \
    static inline __attribute__((always_inline)) typeof(((model_t *)0)->field) model_get_##name(model_t *pmodel) {     \
        assert(pmodel != NULL);                                                                                        \
        typeof(((model_t *)0)->field) res = pmodel->field;                                                             \
        return res;                                                                                                    \
    }

#define SETTER_UNSAFE(name, field)                                                                                     \
    static inline                                                                                                      \
        __attribute__((always_inline)) void model_set_##name(model_t *pmodel, typeof(((model_t *)0)->field) value) {   \
        assert(pmodel != NULL);                                                                                        \
        pmodel->field = value;                                                                                         \
    }

#define GETTER(type, name, field)                                                                                      \
    static inline __attribute__((always_inline)) typeof(((model_t *)0)->field) model_get_##name(type *arg) {           \
        model_t *pmodel = arg;                                                                                         \
        assert(pmodel != NULL);                                                                                        \
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);                                                                    \
        typeof(((model_t *)0)->field) res = pmodel->field;                                                             \
        xSemaphoreGive(pmodel->sem);                                                                                   \
        return res;                                                                                                    \
    }


#define SETTER(type, name, field)                                                                                      \
    static inline                                                                                                      \
        __attribute__((always_inline)) void model_set_##name(type *arg, typeof(((model_t *)0)->field) value) {         \
        model_t *pmodel = arg;                                                                                         \
        assert(pmodel != NULL);                                                                                        \
        xSemaphoreTake(pmodel->sem, portMAX_DELAY);                                                                    \
        pmodel->field = value;                                                                                         \
        xSemaphoreGive(pmodel->sem);                                                                                   \
    }

#define GETTER_GENERIC(name, field) GETTER(void, name, field)
#define SETTER_GENERIC(name, field) SETTER(void, name, field)

#define GETTER_MODEL(name, field) GETTER(model_t, name, field)
#define SETTER_MODEL(name, field) SETTER(model_t, name, field)

#define GETTERNSETTER_GENERIC(name, field)                                                                             \
    GETTER_GENERIC(name, field)                                                                                        \
    SETTER_GENERIC(name, field)

#define GETTERNSETTER_UNSAFE(name, field)                                                                              \
    GETTER_UNSAFE(name, field)                                                                                         \
    SETTER_UNSAFE(name, field)

#define GETTERNSETTER(name, field)                                                                                     \
    GETTER_MODEL(name, field)                                                                                          \
    SETTER_MODEL(name, field)


typedef struct {
    StaticSemaphore_t semaphore_buffer;
    SemaphoreHandle_t sem;

    uint16_t address;
    uint16_t class;
    uint32_t serial_number;
    char     safety_message[EASYCONNECT_MESSAGE_SIZE + 1];
    char     feedback_message[EASYCONNECT_MESSAGE_SIZE + 1];

    uint8_t feedback_enabled;
    uint8_t feedback_direction;
    uint8_t output_attempts;
    uint8_t feedback_delay;
    uint8_t missing_heartbeat;

    uint8_t output_attempts_exceeded;
} model_t;


void     model_init(model_t *model);
uint16_t model_get_class(void *arg);
int      model_set_class(void *arg, uint16_t class, uint16_t *out_class);
void     model_get_safety_message(void *args, char *string);
void     model_set_safety_message(model_t *pmodel, const char *string);
void     model_get_feedback_message(void *args, char *string);
void     model_set_feedback_message(model_t *pmodel, const char *string);

GETTERNSETTER_GENERIC(address, address);
GETTERNSETTER_GENERIC(serial_number, serial_number);
GETTERNSETTER_GENERIC(missing_heartbeat, missing_heartbeat);
GETTERNSETTER(feedback_enabled, feedback_enabled);
GETTERNSETTER(feedback_direction, feedback_direction);
GETTERNSETTER(output_attempts, output_attempts);
GETTERNSETTER(output_attempts_exceeded, output_attempts_exceeded);
GETTERNSETTER(feedback_delay, feedback_delay);

#endif