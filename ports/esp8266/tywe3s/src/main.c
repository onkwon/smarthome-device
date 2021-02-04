#include <assert.h>
#include <time.h>

#include "libmcu/logging.h"
#include "libmcu/system.h"
#include "libmcu/pubsub.h"
#include "libmcu/button.h"

#include "sleep.h"
#include "jobpool.h"

#include "dfu/dfu.h"
#include "dfu/flash.h"

#include "ota/ota.h"
#include "ota/protocol/mqtt.h"
#include "ota/format/json.h"

#include "provisioning.h"
#include "reporter.h"
#include "relay.h"
#include "led.h"

extern void system_print_tasks_info(void);
extern void mdns_test(void);
extern void sntp_test(void);

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

static unsigned int get_time_ms(void)
{
	time_t t = time(NULL);
	return (unsigned int)t;
}

int main(void)
{
	static uint8_t logbuf[1024];
	logging_init(memory_storage_init(logbuf, sizeof(logbuf)));
	pubsub_init();

	bool initialized = jobpool_init();
	assert(initialized == true);

	led_init();
	relay_init();
	button_init(get_time_ms, sleep_ms);

	//mdns_test();
	sntp_test();

	info("%s rebooting from %s", def2str(VERSION_TAG),
			system_get_reboot_reason_string());

	if (!reporter_is_enabled()) {
		if (provisioning_start()) {
			system_reboot();
		}
	}

	reporter_t *reporter = reporter_new(system_get_serial_number_string());
	assert(reporter != NULL);

	dfu_init(dfu_flash());
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
