#ifndef LED_H
#define LED_H

#include <stdbool.h>

void led_init(void);
void led_1_set(bool on);
void led_2_set(bool on);
void led_wifi_set(bool on);

#endif /* LED_H */
