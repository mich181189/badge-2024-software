#ifndef _DMX_H_
#define _DMX_H_

#include <stdint.h>

struct dmx_struct_t;
typedef struct dmx_struct_t dmx_t;

dmx_t* dmx_create(int uart, int tx_pin, int rx_pin, int rts_pin);
void dmx_destroy(dmx_t* dmx);
void dmx_start(dmx_t* dmx);
void dmx_stop(dmx_t* dmx);

uint8_t dmx_get_value(dmx_t* dmx, int channel);

#endif
