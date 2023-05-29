#ifndef CONFIGURATION_H_INCLUDED
#define CONFIGURATION_H_INCLUDED


#include <stdint.h>
#include "model/model.h"


void configuration_init(model_t *pmodel);
void configuration_save_serial_number(void *args, uint32_t value);
int  configuration_save_class(void *args, uint16_t value);
void configuration_save_address(void *args, uint16_t value);
void configuration_save_feedback_direction(void *args, uint8_t value);
void configuration_save_activation_attempts(void *args, uint8_t value);
void configuration_save_feedback_delay(void *args, uint8_t value);
void configuration_save_feedback_enable(void *args, uint8_t value);
void configuration_save_safety_message(void *args, const char *string);
void configuration_save_feedback_message(void *args, const char *string);
void configuration_save_work_seconds(uint32_t value);


#endif