#ifndef SWITCH_H
#define SWITCH_H

#include "light.h"

extern int switch1_get_state(void);
extern int switch2_get_state(void);

void switch_set(int n, bool state);
void switch_init(void (*callback)(int n, const struct light *light));

#endif /* SWITCH_H */
