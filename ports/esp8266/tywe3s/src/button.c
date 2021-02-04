#include "libmcu/button.h"
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "libmcu/logging.h"
#include "jobpool.h"
#include "relay.h"

#define BUTTON1_BIT				GPIO_Pin_14
#define BUTTON1_PIN				GPIO_NUM_14
#define BUTTON2_BIT				GPIO_Pin_12
#define BUTTON2_PIN				GPIO_NUM_12

static void button1_pressed(const struct button_data *btn, void *context)
{
	(void)context;
	debug("button#1 pressed %d, %x", btn->time_pressed, btn->history);
}

static void button1_released(const struct button_data *btn, void *context)
{
	(void)context;
	relay_1_toggle();
	debug("button#1 released %d, %x", btn->time_released, btn->history);
}

static int get_button1_state(void)
{
	return !gpio_get_level(BUTTON1_PIN);
}

static void button2_pressed(const struct button_data *btn, void *context)
{
	(void)context;
	debug("button#2 pressed %d, %x", btn->time_pressed, btn->history);
}

static void button2_released(const struct button_data *btn, void *context)
{
	(void)context;
	relay_2_toggle();
	debug("button#2 released %d, %x", btn->time_released, btn->history);
}

static int get_button2_state(void)
{
	return !gpio_get_level(BUTTON2_PIN);
}

static void enable_button_interrupt(void)
{
	gpio_set_intr_type(BUTTON1_PIN, GPIO_INTR_NEGEDGE);
	gpio_set_intr_type(BUTTON2_PIN, GPIO_INTR_NEGEDGE);
}

static void disable_button_interrupt(void)
{
	gpio_set_intr_type(BUTTON1_PIN, GPIO_INTR_DISABLE);
	gpio_set_intr_type(BUTTON2_PIN, GPIO_INTR_DISABLE);
}

static void button_task(void *context)
{
	button_poll(context);
	enable_button_interrupt();
}

static void isr_button(void *arg)
{
	jobpool_schedule(button_task, arg);
	disable_button_interrupt();
}

void button_hw_init(void)
{
	gpio_config_t io_conf = {
		.pin_bit_mask = BUTTON1_BIT | BUTTON2_BIT,
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
	};
	gpio_config(&io_conf);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(BUTTON1_PIN, isr_button, (void *)BUTTON1_PIN);
	gpio_isr_handler_add(BUTTON2_PIN, isr_button, (void *)BUTTON2_PIN);

	static button_handlers_t button1 = {
		.pressed = button1_pressed,
		.released = button1_released,
	};
	static button_handlers_t button2 = {
		.pressed = button2_pressed,
		.released = button2_released,
	};
	button_register(&button1, get_button1_state);
	button_register(&button2, get_button2_state);
}
