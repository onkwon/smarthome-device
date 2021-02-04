#include "relay.h"
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// 2, 3, 5, 10, 15
#define RELAY1_PIN			GPIO_NUM_15
#define RELAY1_BIT			GPIO_Pin_15
#define RELAY2_PIN			GPIO_NUM_10
#define RELAY2_BIT			GPIO_Pin_10

void relay_1_toggle(void)
{
	gpio_set_level(RELAY1_PIN, gpio_get_level(RELAY1_PIN) ^ 1);
}

void relay_2_toggle(void)
{
	gpio_set_level(RELAY2_PIN, gpio_get_level(RELAY2_PIN) ^ 1);
}

void relay_init(void)
{
	gpio_config_t out_conf = {
		.pin_bit_mask = RELAY1_BIT | RELAY2_BIT,
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
	};
	gpio_config(&out_conf);
}
