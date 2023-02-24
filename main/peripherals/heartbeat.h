#ifndef HEARTBEAT_H_INCLUDED
#define HEARTBEAT_H_INCLUDED


#include <stdlib.h>


void heartbeat_init(void);
void heartbeat_update_green(uint8_t value);
void heartbeat_update_red(uint8_t value);


#endif