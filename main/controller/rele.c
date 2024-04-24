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
#include "event_log.h"


typedef enum {
    RELE_SM_STATE_OFF = 0,
    RELE_SM_STATE_OFF_WAITING_FB,
    RELE_SM_STATE_ON,
    RELE_SM_STATE_ON_WAITING_FB,
    RELE_SM_STATE_ERROR,
} rele_sm_state_t;


typedef enum {
    RELE_EVENT_REFRESH,
    RELE_EVENT_OFF,
    RELE_EVENT_ON,
    RELE_EVENT_CHECK_FEEDBACK,
    RELE_EVENT_RETRY,
} rele_event_t;


DEFINE_STATE_MACHINE(rele, rele_event_t, model_t);


static int     on_event_manager(model_t *pmodel, rele_event_t event);
static int     on_waiting_fb_event_manager(model_t *pmodel, rele_event_t event);
static int     off_event_manager(model_t *pmodel, rele_event_t event);
static int     off_waiting_fb_event_manager(model_t *pmodel, rele_event_t event);
static void    timer_callback(gel_timer_t *timer, void *arg, void *code);
static int     error_event_manager(model_t *pmodel, rele_event_t event);
static int     turn_on(model_t *pmodel);
static void    turn_off(model_t *pmodel);
static uint8_t can_turn_on(model_t *pmodel);

static inline __attribute__((always_inline)) void set_rele(uint8_t value) {
    digout_update(DIGOUT_RELE, value);
}


static const char   *TAG       = "Rele";
static unsigned long timestamp = 0;


static rele_event_manager_t managers[] = {
    [RELE_SM_STATE_OFF] = off_event_manager,     [RELE_SM_STATE_OFF_WAITING_FB] = off_waiting_fb_event_manager,
    [RELE_SM_STATE_ON] = on_event_manager,       [RELE_SM_STATE_ON_WAITING_FB] = on_waiting_fb_event_manager,
    [RELE_SM_STATE_ERROR] = error_event_manager,
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
    return sm.state != RELE_SM_STATE_OFF;
}


void rele_refresh(model_t *pmodel) {
    rele_sm_send_event(&sm, pmodel, RELE_EVENT_REFRESH);
}


void rele_manage(model_t *pmodel) {
    gel_timer_manage_callbacks(&check_timer, 1, get_millis(), pmodel);
    gel_timer_manage_callbacks(&retry_timer, 1, get_millis(), pmodel);
}


static int on_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_OFF:
            turn_off(pmodel);
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_REFRESH:
            if (can_turn_on(pmodel)) {
            } else {
                turn_off(pmodel);
                ESP_LOGW(TAG, "Safety signal off; going to error state");
                return RELE_SM_STATE_ERROR;
            }

            if (model_get_feedback_enabled(pmodel) && digin_get(DIGIN_SIGNAL) != model_get_feedback_direction(pmodel)) {
                turn_off(pmodel);
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
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_REFRESH:
            if (can_turn_on(pmodel)) {
            } else {
                set_rele(0);
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
                set_rele(0);
                gel_timer_activate(&retry_timer, model_get_feedback_delay(pmodel) * 1000, get_millis(), timer_callback,
                                   (void *)(uintptr_t)RELE_EVENT_RETRY);
                return RELE_SM_STATE_OFF_WAITING_FB;
            } else {
                model_set_output_attempts_exceeded(pmodel, 1);
                set_rele(0);
                ESP_LOGI(TAG, "No more attempts");
                return RELE_SM_STATE_OFF;
            }

        default:
            return -1;
    }
}


static int off_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_ON:
            if (can_turn_on(pmodel)) {
                return turn_on(pmodel);
            } else {
                // Go to error state
                ESP_LOGW(TAG, "Safety signal off, cannot turn on; going to error state");
                return RELE_SM_STATE_ERROR;
            }

        case RELE_EVENT_REFRESH:
            // Change safety signal only if there is no standing error
            if (!model_get_output_attempts_exceeded(pmodel)) {
                if (can_turn_on(pmodel)) {
                } else {
                }
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

        case RELE_EVENT_REFRESH:
            if (can_turn_on(pmodel)) {
            } else {
                return RELE_SM_STATE_OFF;
            }
            return -1;

        case RELE_EVENT_OFF:
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_RETRY:
            ESP_LOGI(TAG, "Retrying");
            set_rele(1);
            gel_timer_activate(&check_timer, model_get_feedback_delay(pmodel) * 1000UL, get_millis(), timer_callback,
                               (void *)(uintptr_t)RELE_EVENT_CHECK_FEEDBACK);
            return RELE_SM_STATE_ON_WAITING_FB;

        default:
            return -1;
    }
}


static int error_event_manager(model_t *pmodel, rele_event_t event) {
    switch (event) {
        case RELE_EVENT_OFF:
            // Just go to off state
            return RELE_SM_STATE_OFF;

        case RELE_EVENT_ON:
        case RELE_EVENT_REFRESH:
            if (can_turn_on(pmodel)) {
                // Safety is OK again, turn on
                return turn_on(pmodel);
            } else {
                return -1;
            }

        default:
            return -1;
    }
}


static void timer_callback(gel_timer_t *timer, void *arg, void *code) {
    rele_sm_send_event(&sm, arg, (rele_event_t)(uintptr_t)code);
}


static int turn_on(model_t *pmodel) {
    set_rele(1);
    model_set_output_attempts_exceeded(pmodel, 0);

    switch (CLASS_GET_MODE(model_get_class(pmodel))) {
        case DEVICE_MODE_UVC:
        case DEVICE_MODE_ESF:
            timestamp = get_millis();
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
}


static uint8_t can_turn_on(model_t *pmodel) {
    switch (CLASS_GET_MODE(model_get_class(pmodel))) {
        case DEVICE_MODE_UVC:
        case DEVICE_MODE_ESF:
            //printf("%i %i\n", safety_ok(), model_get_safety_bypass(pmodel));
            return (safety_ok() || model_get_safety_bypass(pmodel)) && !model_get_missing_heartbeat(pmodel);

        default:
            return 1;
    }
}


static void turn_off(model_t *pmodel) {
    set_rele(0);
    if (timestamp != 0) {
        model_increase_work_seconds(pmodel, time_interval(timestamp, get_millis()) / 1000UL);
        //model_increase_work_seconds(pmodel, 60UL * 60UL * 100UL);
        timestamp = 0;
    }
}
