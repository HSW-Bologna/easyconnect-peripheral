#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "peripherals/digout.h"
#include "safety.h"
#include "model/model.h"
#include "easyconnect_interface.h"
#include "utils/utils.h"
#include "gel/timer/timecheck.h"
#include "peripherals/heartbeat.h"
#include "peripherals/digin.h"
#include "rele.h"
#include "gel/state_machine/state_machine.h"
#include "gel/timer/timer.h"


typedef enum {
    RELE_SM_STATE_OFF = 0,
    RELE_SM_STATE_OFF_WAITING_FB,
    RELE_SM_STATE_ON,
    RELE_SM_STATE_ON_WAITING_FB,
} rele_sm_state_t;


typedef enum {
    RELE_EVENT_NEW_INPUT,
    RELE_EVENT_OFF,
    RELE_EVENT_ON,
    RELE_EVENT_CHECK_FEEDBACK,
    RELE_EVENT_RETRY,
} rele_event_t;


DEFINE_STATE_MACHINE(rele, rele_event_t, model_t);


static int  on_event_manager(model_t *pmodel, rele_event_t event);
static int  on_waiting_fb_event_manager(model_t *pmodel, rele_event_t event);
static int  off_event_manager(model_t *pmodel, rele_event_t event);
static int  off_waiting_fb_event_manager(model_t *pmodel, rele_event_t event);
static void timer_callback(gel_timer_t *timer, void *arg, void *code);

static inline __attribute__((always_inline)) void set_rele(uint8_t value) {
    digout_update(DIGOUT_RELE, value);
}

static inline __attribute__((always_inline)) void set_light(uint8_t value) {
    digout_update(DIGOUT_LIGHT_SIGNAL, !value);
}


static const char *TAG = "Rele";


static rele_event_manager_t managers[] = {
    [RELE_SM_STATE_OFF]            = off_event_manager,
    [RELE_SM_STATE_OFF_WAITING_FB] = off_waiting_fb_event_manager,
    [RELE_SM_STATE_ON]             = on_event_manager,
    [RELE_SM_STATE_ON_WAITING_FB]  = on_waiting_fb_event_manager,
};

static rele_state_machine_t sm = {
    .state    = RELE_SM_STATE_OFF,
    .managers = managers,
};

static gel_timer_t check_timer = GEL_TIMER_NULL;
static gel_timer_t retry_timer = GEL_TIMER_NULL;
static uint8_t     attempts    = 0;


int rele_update(model_t *pmodel, uint8_t value) {
    return rele_sm_send_event(&sm, pmodel, value ? RELE_EVENT_ON : RELE_EVENT_OFF) ? 0 : -1;
}


uint8_t rele_is_on(void) {
    return digout_get();
}


void rele_refresh(model_t *pmodel) {
    rele_sm_send_event(&sm, pmodel, RELE_EVENT_NEW_INPUT);
}


void rele_manage(model_t *pmodel) {
    gel_timer_manage_callbacks(&check_timer, 1, get_millis(), pmodel);
    gel_timer_manage_callbacks(&retry_timer, 1, get_millis(), pmodel);
}


static int on_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_OFF:
            set_rele(0);
            set_light(0);
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_NEW_INPUT:
            if (safety_ok()) {
                heartbeat_set_state(HEARTBEAT_STATE_OK);
                set_light(rele_is_on());
            } else {
                heartbeat_set_state(HEARTBEAT_STATE_KO);
                set_rele(0);
                set_light(0);
                return RELE_SM_STATE_OFF;
            }

            if (model_get_feedback_enabled(pmodel) && digin_get(DIGIN_SIGNAL) != model_get_feedback_direction(pmodel)) {
                set_rele(0);
                set_light(0);
                gel_timer_activate(&retry_timer, model_get_feedback_delay(pmodel) * 1000UL, get_millis(),
                                   timer_callback, (void *)(uintptr_t)RELE_EVENT_RETRY);
                return RELE_SM_STATE_OFF_WAITING_FB;
            }

            return -1;

        default:
            return -1;
    }
}


static int on_waiting_fb_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_OFF:
            set_rele(0);
            set_light(0);
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_NEW_INPUT:
            if (safety_ok()) {
                heartbeat_set_state(HEARTBEAT_STATE_OK);
                set_light(rele_is_on());
            } else {
                heartbeat_set_state(HEARTBEAT_STATE_KO);
                set_rele(0);
                set_light(0);
                return RELE_SM_STATE_OFF;
            }

            return -1;

        case RELE_EVENT_CHECK_FEEDBACK:
            if (digin_get(DIGIN_SIGNAL) == model_get_feedback_direction(pmodel)) {
                attempts = 0;
                return RELE_SM_STATE_ON;
            } else if (attempts < model_get_output_attempts(pmodel) - 1) {
                attempts++;
                ESP_LOGI(TAG, "Feedback invalid, attempt %i", attempts);
                heartbeat_signal(3);
                set_rele(0);
                set_light(0);
                gel_timer_activate(&retry_timer, model_get_feedback_delay(pmodel) * 1000, get_millis(), timer_callback,
                                   (void *)(uintptr_t)RELE_EVENT_RETRY);
                return RELE_SM_STATE_OFF_WAITING_FB;
            } else {
                ESP_LOGI(TAG, "No more attempts");
                set_rele(0);
                set_light(0);
                return RELE_SM_STATE_OFF;
            }

        default:
            return -1;
    }
}


static int off_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_ON:
            if (safety_ok()) {
                set_rele(1);
                set_light(1);

                switch (CLASS_GET_MODE(model_get_class(pmodel))) {
                    case DEVICE_MODE_UVC:
                    case DEVICE_MODE_ESF:
                        if (model_get_feedback_enabled(pmodel)) {
                            gel_timer_activate(&check_timer, model_get_feedback_delay(pmodel) * 1000UL, get_millis(),
                                               timer_callback, (void *)(uintptr_t)RELE_EVENT_CHECK_FEEDBACK);
                            attempts = 0;
                            return RELE_SM_STATE_ON_WAITING_FB;
                        } else {
                            return RELE_SM_STATE_ON;
                        }
                        break;

                    // Gas e Luci
                    default:
                        return RELE_SM_STATE_ON;
                        break;
                }
            } else {
                return -1;
            }

        case RELE_EVENT_NEW_INPUT:
            if (safety_ok()) {
                heartbeat_set_state(HEARTBEAT_STATE_OK);
            } else {
                heartbeat_set_state(HEARTBEAT_STATE_KO);
            }
            return -1;

        default:
            return -1;
    }
}


static int off_waiting_fb_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_ON:
            attempts = 0;
            return -1;

        case RELE_EVENT_NEW_INPUT:
            if (safety_ok()) {
                heartbeat_set_state(HEARTBEAT_STATE_OK);
            } else {
                heartbeat_set_state(HEARTBEAT_STATE_KO);
                return RELE_SM_STATE_OFF;
            }
            return -1;

        case RELE_EVENT_OFF:
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_RETRY:
            ESP_LOGI(TAG, "Retrying");
            set_rele(1);
            set_light(1);
            gel_timer_activate(&check_timer, model_get_feedback_delay(pmodel) * 1000UL, get_millis(), timer_callback,
                               (void *)(uintptr_t)RELE_EVENT_CHECK_FEEDBACK);
            return RELE_SM_STATE_ON_WAITING_FB;

        default:
            return -1;
    }
}

static void timer_callback(gel_timer_t *timer, void *arg, void *code) {
    rele_sm_send_event(&sm, arg, (rele_event_t)(uintptr_t)code);
}