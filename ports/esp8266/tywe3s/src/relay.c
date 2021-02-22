#include "relay.h"
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define RELAY1_PIN			GPIO_NUM_4
#define RELAY1_BIT			GPIO_Pin_4
#define RELAY2_PIN			GPIO_NUM_13
#define RELAY2_BIT			GPIO_Pin_13

void relay_1_set(bool on)
{
	gpio_set_level(RELAY1_PIN, on);
}

void relay_2_set(bool on)
{
	gpio_set_level(RELAY2_PIN, on);
}

void relay_init(void)
{
	gpio_config_t out_conf = {
		.pin_bit_mask = RELAY1_BIT | RELAY2_BIT,
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
	};
	gpio_config(&out_conf);

	relay_1_set(0);
	relay_2_set(0);
}
