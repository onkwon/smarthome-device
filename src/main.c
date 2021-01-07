#include <assert.h>
#include "libmcu/logging.h"
#include "libmcu/system.h"
#include "sleep.h"
#include "jobpool.h"
#include "reporter.h"
#include "ota/ota.h"
#include "ota/protocol/mqtt.h"
#include "ota/format/json.h"

extern void system_print_tasks_info(void);
void application(void);

#include <stdio.h>
#include "memory_storage.h"
#include "libmcu/compiler.h"
void memory_storage_write_hook(const void *data, size_t size)
{
	unused(size);
	char buf[LOGGING_MESSAGE_MAXLEN + 32];
	puts(logging_stringify(buf, sizeof(buf), data));
	puts("\n");
}

void application(void)
{
	static uint8_t logbuf[1024];
	logging_init(memory_storage_init(logbuf, sizeof(logbuf)));

	bool initialized = jobpool_init();
	assert(initialized == true);

	reporter_t *reporter = reporter_new(system_get_serial_number_string());
	assert(reporter != NULL);
	ota_init(reporter, ota_mqtt(), ota_json_parser());

	while (1) {
		system_print_tasks_info();
		debug("%s", system_get_reboot_reason_string());
		debug("heap: %d", system_get_free_heap_bytes());

		sleep_ms(20000);
		reporter_send("Hello", 5);
		sleep_ms(10000);
		//wifiman_disconnect();
	}
}
