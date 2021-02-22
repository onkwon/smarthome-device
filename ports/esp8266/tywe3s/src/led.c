#include "led.h"
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define LED_WIFI_PIN			GPIO_NUM_16
#define LED_WIFI_BIT			GPIO_Pin_16

void led_1_set(bool on)
{
	(void)on;
}

void led_2_set(bool on)
{
	(void)on;
}

void led_wifi_set(bool on)
{
	//gpio_set_level(LED_WIFI_PIN, gpio_get_level(LED_WIFI_PIN) ^ 1);
	gpio_set_level(LED_WIFI_PIN, !on);
}

void led_init(void)
{
	gpio_config_t out_conf = {
		.pin_bit_mask = LED_WIFI_BIT,
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
	};
	gpio_config(&out_conf);

	led_wifi_set(true);
}
