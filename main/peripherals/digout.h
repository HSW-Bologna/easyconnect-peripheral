#ifndef DIGOUT_H_INCLUDED
#define DIGOUT_H_INCLUDED

#include <string.h>
#include <stdint.h>


typedef enum {
    DIGOUT_RELE = 0,
} digout_t;


void    digout_init(void);
void    digout_update(digout_t digout, uint8_t val);
uint8_t digout_get(void);

#endif