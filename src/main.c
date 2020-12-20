#include <assert.h>
#include "libmcu/system.h"
#include "libmcu/logging.h"
#include "libmcu/compiler.h"
#include "libmcu/retry.h"
#include "sleep.h"
#include "wifi.h"
#include "jobpool.h"

#define TEST_WIFI_SSID			""
#define TEST_WIFI_PASS			""

static void wifi_reconnect(void LIBMCU_UNUSED *context)
{
	retry_t retry = {
		.max_attempts = 10,
		.max_backoff_ms = 3600000,
		.min_backoff_ms = 5000,
		.max_jitter_ms = 5000,
		.sleep = sleep_ms,
	};
	do {
		if (wifiman_connect(&(const wifiman_network_profile_t) {
				.ssid = TEST_WIFI_SSID, },
				TEST_WIFI_PASS) == WIFIMAN_SUCCESS) {
			return;
		}
	} while (retry_backoff(&retry) != RETRY_EXHAUSTED);

	// TODO: wifi_reset();
}

static void wifi_state_change_event(wifiman_event_t event,
		void LIBMCU_UNUSED *context)
{
	info("wifi state changed to %d: %s.", event,
			event == WIFIMAN_EVENT_CONNECTED?
			"connected" : "disconnected");
	if (event == WIFIMAN_EVENT_DISCONNECTED) {
		bool scheduled = jobpool_schedule(wifi_reconnect, NULL);
		assert(scheduled == true);
		info("Try to reconnect");
	}
}

#include "mqtt.h"

#define MQTT_BROKER_ENDPOINT		""
extern const uint8_t x509_ca_cert[] asm("_binary_AmazonRootCA1_pem_start");
extern const uint8_t x509_device_cert[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t x509_device_key[] asm("_binary_private_pem_key_start");

extern void system_print_tasks_info(void);
void application(void);

static void sub_test_callback(void * const context,
		const mqtt_message_t * const msg)
{
	debug("App received message %.*s", msg->payload_size, msg->payload);
	unused(context);
}

void application(void)
{
	bool initialized = jobpool_init();
	assert(initialized == true);

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

	mqtt_t *mqtt = mqtt_new();
	mqtt_set_lwt(mqtt, &(mqtt_message_t) {
			.qos = MQTT_QOS_1,
			.topic = "will",
			.payload = (const uint8_t *)"lost",
			.payload_size = 4, });
	mqtt_connect(mqtt, &(mqtt_connect_t) {
			.credential = {
				.ca_cert = x509_ca_cert,
				.client_cert = x509_device_cert,
				.client_key = x509_device_key, },
			.url = MQTT_BROKER_ENDPOINT,
			.client_id = "mydevice",
			.reconnect = true,
			.clean_session = true, });
	mqtt_subscribe(mqtt, &(mqtt_subscribe_t) {
			.topic_filter = "abc/#",
			.qos = MQTT_QOS_1,
			.callback = {
				.run = sub_test_callback,
				.context = NULL, }, });

	while (1) {
		system_print_tasks_info();
		debug("%s", system_get_reboot_reason_string());
		debug("heap: %d", system_get_free_heap_bytes());
		debug("rssi %d", wifiman_get_rssi());

		sleep_ms(20000);
		mqtt_publish(mqtt, &(mqtt_message_t) {
				.qos = MQTT_QOS_1,
				.topic = "msg",
				.payload = (const uint8_t *)"Hello",
				.payload_size = 5,
				});

		sleep_ms(10000);
		wifiman_disconnect();
	}
}

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
size_t logging_save(const logging_t type, const void *pc, const void *lr, ...)
{
	int len = printf("%lu: [%s] <%p,%p> ", (unsigned long)time(0),
			type == 0? "VERBOSE" :
			type == 1? "DEBUG" :
			type == 2? "INFO" :
			type == 3? "NOTICE" :
			type == 4? "WARN" :
			type == 5? "ERROR" :
			type == 6? "ALERT" : "UNKNOWN",
			pc, lr);
	va_list ap;
	va_start(ap, lr);
	const char *fmt = va_arg(ap, char *);
	if (fmt) {
		len += vfprintf(stdout, fmt, ap);
		len += printf("\n");
	}
	return (size_t)len;
}
