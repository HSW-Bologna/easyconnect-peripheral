#ifndef RELE_H_INCLUDED
#define RELE_H_INCLUDED


#include <stdint.h>
#include "model/model.h"


int             rele_update(model_t *pmodel, uint8_t value);
uint8_t         rele_is_on(void);
void            rele_refresh(model_t *pmodel);
void            rele_manage(model_t *pmodel);


#endif