#include <assert.h>
#include "libmcu/system.h"
#include "libmcu/logging.h"
#include "libmcu/compiler.h"
#include "libmcu/jobqueue.h"
#include "sleep.h"
#include "wifi.h"
#include "retry.h"

#define TEST_WIFI_SSID			""
#define TEST_WIFI_PASS			""

static struct {
	jobqueue_t *jobqueue;
	job_t wifi_reconncet_job;
} m;

static void wifi_reconnect(void LIBMCU_UNUSED *context)
{
	retry_t retry;
	retry_init(&retry, 10, 1000, 1000, 3600000);
	while (retry_backoff(&retry) != RETRY_EXHAUSTED) {
		if (wifiman_connect(&(const wifiman_network_profile_t) {
				.ssid = TEST_WIFI_SSID, },
				TEST_WIFI_PASS) == WIFIMAN_SUCCESS) {
			return;
		}
	}

	// TODO: wifi_reset();
}

static void initialize_wifi_reconncet_job(void)
{
	m.jobqueue = jobqueue_create(1);
	assert(m.jobqueue != NULL);
	job_error_t rc =
		job_init(m.jobqueue, &m.wifi_reconncet_job, wifi_reconnect, NULL);
	assert(rc == JOB_SUCCESS);
}

static void wifi_state_change_event(wifiman_event_t event,
		void LIBMCU_UNUSED *context)
{
	info("wifi state changed to %d: %s.", event,
			event == WIFIMAN_EVENT_CONNECTED?
			"connected" : "disconnected");
	if (event == WIFIMAN_EVENT_DISCONNECTED) {
		job_error_t rc = job_schedule(m.jobqueue, &m.wifi_reconncet_job);
		assert(rc == JOB_SUCCESS);
	}
}

extern void system_print_tasks_info(void);
void application(void);
void application(void)
{
	initialize_wifi_reconncet_job();

	uint8_t ip[4], mac[6] = { 0, };

	wifiman_on();
	wifiman_register_event_handler(WIFIMAN_EVENT_CONNECTED,
			wifi_state_change_event);
	wifiman_register_event_handler(WIFIMAN_EVENT_DISCONNECTED,
			wifi_state_change_event);
	wifiman_connect(&(const wifiman_network_profile_t) {
			.ssid = TEST_WIFI_SSID, },
			TEST_WIFI_PASS);
	wifiman_get_ip(ip);
	wifiman_get_mac(mac);
	info("IP %d.%d.%d.%d rssi %d",
			ip[0], ip[1], ip[2], ip[3],
			wifiman_get_rssi());
	info("MAC %02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	wifiman_network_profile_t scanlist[10];
	int n = wifiman_scan(scanlist, 10);
	debug("Scanned : %d", n);
	for (int i = 0; i < n; i++) {
		info("%d: %s %d", i+1, scanlist[i].ssid, scanlist[i].rssi);
	}

	while (1) {
		system_print_tasks_info();
		debug("%s", system_get_reboot_reason_string());
		debug("heap: %d", system_get_free_heap_bytes());
		debug("rssi %d", wifiman_get_rssi());
		sleep_ms(10000);
		wifiman_disconnect();
	}
}
