#include "libmcu/system.h"
#include <stdio.h>
#include "sleep.h"
#include "libmcu/logging.h"

#include "logging/fake_storage.h"
static void fake_logging_init(void)
{
	logging_init(logging_fake_storage_init(puts));
}

extern void system_print_tasks_info(void);
void application(void);
void application(void)
{
	fake_logging_init();

	while (1) {
		system_print_tasks_info();
		debug("%s", system_get_reboot_reason_string());
		debug("heap: %d", system_get_free_heap_bytes());
		sleep_ms(5000);
	}
}
