#include "switch.h"

#include <time.h>

#include "libmcu/button.h"
#include "libmcu/logging.h"

#include "light.h"
#include "relay.h"
#include "led.h"
#include "sleep.h"

static void button1_pressed(const struct button_data *btn, void *context);
static void button1_released(const struct button_data *btn, void *context);
static void button2_pressed(const struct button_data *btn, void *context);
static void button2_released(const struct button_data *btn, void *context);

static struct {
	struct light light1;
	struct light light2;

	button_handlers_t button1;
	button_handlers_t button2;

	void (*callback)(int n, const struct light *light);
} m = {
	.button1.pressed = button1_pressed,
	.button1.released = button1_released,
	.button2.pressed = button2_pressed,
	.button2.released = button2_released,
};

static void set_switch(int n, bool state)
{
	struct light *light;

	switch (n) {
	case 1:
		light = &m.light1;
		relay_1_set(state);
		break;
	case 2:
		light = &m.light2;
		relay_2_set(state);
		break;
	default:
		return;
	}

	light->state = state;
	m.callback(n, light);
}

static void button1_pressed(const struct button_data *btn, void *context)
{
	(void)context;
	debug("button#1 pressed %d, %x", btn->time_pressed, btn->history);
}

static void button1_released(const struct button_data *btn, void *context)
{
	set_switch(1, !m.light1.state);

	(void)context;
	debug("button#1 released %d, %x", btn->time_released, btn->history);
}

static void button2_pressed(const struct button_data *btn, void *context)
{
	(void)context;
	debug("button#2 pressed %d, %x", btn->time_pressed, btn->history);
}

static void button2_released(const struct button_data *btn, void *context)
{
	set_switch(1, !m.light2.state);

	(void)context;
	debug("button#2 released %d, %x", btn->time_released, btn->history);
}

static unsigned int get_time_ms(void)
{
	time_t t = time(NULL);
	return (unsigned int)t;
}

static void set_initial_state(void)
{
	if (switch1_get_state()) {
		m.light1.state = true;
	}
	if (switch2_get_state()) {
		m.light2.state = true;
	}

	relay_1_set(m.light1.state);
	relay_2_set(m.light2.state);
}

void switch_set(int n, bool state)
{
	set_switch(n, state);
}

void switch_init(void (*callback)(int n, const struct light *light))
{
	m.callback = callback;

	led_init();
	relay_init();
	button_init(get_time_ms, sleep_ms);

	button_register(&m.button1, switch1_get_state);
	button_register(&m.button2, switch2_get_state);

	set_initial_state();
}
