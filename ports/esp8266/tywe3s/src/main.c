#include <assert.h>

#include "libmcu/logging.h"
#include "libmcu/system.h"
#include "libmcu/pubsub.h"
#include "libmcu/button.h"
#include "libmcu/compiler.h"
#include "libmcu/metrics.h"

#include "sleep.h"
#include "jobpool.h"
#include "memory_storage.h"

#include "dfu/dfu.h"
#include "dfu/flash.h"

#include "ota/ota.h"
#include "ota/protocol/mqtt.h"
#include "ota/format/json.h"

#include "provisioning.h"
#include "reporter.h"
#include "switch.h"

extern void system_print_tasks_info(void);
extern void mdns_test(void);
extern void sntp_test(void);

static void send_logs(void)
{
	uint8_t buf[LOGGING_MESSAGE_MAXLEN + 32];
	size_t bytes_read;

	if (logging_count() &&
			(bytes_read = logging_read(buf, sizeof(buf)) > 0)) {
		reporter_send(REPORT_LOGGING, buf, bytes_read);
	}
}

#include <time.h>
#include "wifi.h"

#define METRICS_REPORT_INTERVAL_SEC			3600 /* 1 hour */

static void send_metrics(bool force)
{
	static int32_t stamp;
	int32_t now = (int32_t)time(NULL);
	int32_t elapsed = now - stamp;

	if (force && elapsed < METRICS_REPORT_INTERVAL_SEC) {
		return;
	}

	metrics_set(HeapHighWaterMark, system_get_heap_watermark());
	metrics_set(StackHighWaterMark, system_get_current_stack_watermark());
	metrics_set(WifiRssi, wifiman_get_rssi());

	metrics_increase_by(ReportInterval, elapsed);
	stamp = now;

	uint8_t buf[32];
	size_t size_to_send = metrics_get_encoded(buf, sizeof(buf));
	if (size_to_send > 0 && force) {
		reporter_send(REPORT_HEARTBEAT, buf, size_to_send);
	}

	metrics_reset();
}

static void report_room(int n, const struct light *light)
{
	reporter_send_event(REPORT_ROOM, &light->state, sizeof(light->state));
	unused(n);
}

int main(void)
{
	static uint8_t logbuf[1024];
	logging_init(memory_storage_init(logbuf, sizeof(logbuf)));
	pubsub_init();
	metrics_init();

	bool initialized = jobpool_init();
	assert(initialized == true);

	switch_init(report_room);

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

	send_metrics(false);

	while (1) {
		send_logs();
		send_metrics(true);
		sleep_ms(100);
	}
}
