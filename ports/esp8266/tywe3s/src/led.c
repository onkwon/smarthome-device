#include "led.h"
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define LED1_PIN			GPIO_NUM_4
#define LED1_BIT			GPIO_Pin_4
#define LED2_PIN			GPIO_NUM_13
#define LED2_BIT			GPIO_Pin_13
#define LED_WIFI_PIN			GPIO_NUM_16
#define LED_WIFI_BIT			GPIO_Pin_16

void led_1_toggle(void)
{
	gpio_set_level(LED1_PIN, gpio_get_level(LED1_PIN) ^ 1);
}

void led_2_toggle(void)
{
	gpio_set_level(LED2_PIN, gpio_get_level(LED2_PIN) ^ 1);
}

void led_wifi_toggle(void)
{
	gpio_set_level(LED_WIFI_PIN, gpio_get_level(LED_WIFI_PIN) ^ 1);
}

void led_init(void)
{
	gpio_config_t out_conf = {
		.pin_bit_mask = LED1_BIT | LED2_BIT | LED_WIFI_BIT,
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
	};
	gpio_config(&out_conf);
}
