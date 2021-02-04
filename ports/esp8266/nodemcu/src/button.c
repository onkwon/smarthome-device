#include "libmcu/button.h"
#include <stdint.h>
#include <stdbool.h>
#include "libmcu/logging.h"
#include "jobpool.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT0_BIT			GPIO_Pin_0
#define BOOT0_PIN			GPIO_NUM_0

static void boot0_pressed(const struct button_data *btn, void *context)
{
	(void)context;
	debug("boot0 pressed %d, %x", btn->time_pressed, btn->history);
}

static void boot0_released(const struct button_data *btn, void *context)
{
	(void)context;
	debug("boot0 released %d, %x", btn->time_released, btn->history);
}

static int get_boot0_state(void)
{
	return !gpio_get_level(BOOT0_PIN);
}

static void enable_boot0_interrupt(void)
{
	gpio_set_intr_type(BOOT0_PIN, GPIO_INTR_NEGEDGE);
}

static void disable_boot0_interrupt(void)
{
	gpio_set_intr_type(BOOT0_PIN, GPIO_INTR_DISABLE);
}

static void button_task(void *context)
{
	button_poll(context);
	enable_boot0_interrupt();
}

static void isr_boot0(void *arg)
{
	jobpool_schedule(button_task, arg);
	disable_boot0_interrupt();
}

void button_hw_init(void)
{
	gpio_config_t io_conf = {
		.pin_bit_mask = BOOT0_BIT,
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
	};
	gpio_config(&io_conf);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(BOOT0_PIN, isr_boot0, (void *)BOOT0_PIN);

	static button_handlers_t handlers = {
		.pressed = boot0_pressed,
		.released = boot0_released,
	};
	button_register(&handlers, get_boot0_state);
}
