#include <stdint.h>
#include <stdbool.h>
#include "libmcu/logging.h"
#include "jobpool.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define USER_LED		GPIO_Pin_16
#define ESP12_LED		GPIO_Pin_2

void led_init(void);

static void led_toggle_user(void)
{
	gpio_set_level(GPIO_NUM_16, gpio_get_level(GPIO_NUM_16) ^ 1);
}

static void led_toggle_system(void)
{
	gpio_set_level(GPIO_NUM_2, gpio_get_level(GPIO_NUM_2) ^ 1);
}

void led_init(void)
{
	gpio_config_t out_conf = {
		.pin_bit_mask = USER_LED | ESP12_LED,
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
	};
	gpio_config(&out_conf);

	led_toggle_user();
	led_toggle_system();
}
