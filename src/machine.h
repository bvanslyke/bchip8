
#ifndef BCHIP_MACHINE_H_
#define BCHIP_MACHINE_H_

#include <stdint.h>

#include "bchip.h"

int machine_cycle(struct machine *machine);
int machine_dispatch(struct machine *mac, uint16_t op);

#endif
