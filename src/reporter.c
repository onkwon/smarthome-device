#include "reporter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libmcu/logging.h"
#include "libmcu/retry.h"
#include "libmcu/compiler.h"

#include "sleep.h"
#include "jobpool.h"

#include "wifi.h"
#include "mqtt.h"

#define TEST_WIFI_SSID			""
#define TEST_WIFI_PASS			""

#define DEFAULT_MQTT_BROKER_ENDPOINT	""
#define DEFAULT_TOPIC_PREFIX		"all/hello"

extern const uint8_t x509_ca_cert[] asm("_binary_ca_crt_start");
extern const uint8_t x509_device_cert[] asm("_binary_device_crt_start");
extern const uint8_t x509_device_key[] asm("_binary_device_key_start");

static struct {
	volatile bool reconnecting;
	mqtt_t *mqtt;

	struct {
		const char *version;
		const char *event;
		const char *logging;
		const char *report;
		const char *heartbeat;
		const char *will;
	} topic;
} m;

static void wifi_reconnect(void LIBMCU_UNUSED *context)
{
	static const char *ssid = TEST_WIFI_SSID;
	static const char *pass = TEST_WIFI_PASS;

	retry_t retry = {
		.max_attempts = 10,
		.max_backoff_ms = 3600000,
		.min_backoff_ms = 5000,
		.max_jitter_ms = 5000,
		.sleep = sleep_ms,
	};
	do {
		wifiman_network_profile_t profile = { 0, };
		strncpy(profile.ssid, ssid, WIFIMAN_SSID_MAXLEN);
		if (wifiman_connect(&profile, pass) == WIFIMAN_SUCCESS) {
			m.reconnecting = false;
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
	if (event == WIFIMAN_EVENT_DISCONNECTED
			&& m.reconnecting == false) {
		bool scheduled = jobpool_schedule(wifi_reconnect, NULL);
		m.reconnecting = true;
		assert(scheduled == true);
		info("Try to reconnect");
	}
}

static bool network_interface_init(void)
{
	if (!wifiman_on()) {
		return false;
	}
	if (wifiman_register_event_handler(WIFIMAN_EVENT_CONNECTED,
				wifi_state_change_event) != WIFIMAN_SUCCESS) {
		return false;
	}
	if (wifiman_register_event_handler(WIFIMAN_EVENT_DISCONNECTED,
				wifi_state_change_event) != WIFIMAN_SUCCESS) {
		return false;
	}
	// TODO: support mutiple APs
	if (wifiman_connect(&(const wifiman_network_profile_t) {
				.ssid = TEST_WIFI_SSID, }, TEST_WIFI_PASS)
			!= WIFIMAN_SUCCESS) {
		return false;
	}

	return true;
}

static const char *get_topic_path_allocated(const char *client_id,
		const char *sub_topic)
{
	size_t len = sizeof(DEFAULT_TOPIC_PREFIX)
		+ strlen(client_id)
		+ strlen(sub_topic) + 2/*delimiter*/ + 1/*null*/;
	char *topic = (char *)malloc(len);

	if (topic != NULL) {
		snprintf(topic, len-1,
			DEFAULT_TOPIC_PREFIX"/%s/%s", client_id, sub_topic);
		topic[len-1] = '\0';
	}

	return topic;
}

static bool open_mqtt_connection(const char *client_id)
{
	m.mqtt = mqtt_new();

	if (mqtt_set_lwt(m.mqtt, &(mqtt_message_t) {
				.qos = MQTT_QOS_1,
				.topic = m.topic.will,
				.payload = (const uint8_t *)"lost",
				.payload_size = 4, })
			!= MQTT_SUCCESS) {
		return false;
	}
	if (mqtt_connect(m.mqtt, &(mqtt_connect_t) {
				.credential = {
					.ca_cert = x509_ca_cert,
					.client_cert = x509_device_cert,
					.client_key = x509_device_key, },
				.url = DEFAULT_MQTT_BROKER_ENDPOINT,
				.client_id = client_id,
				.reconnect = true,
				.clean_session = true, })
			!= MQTT_SUCCESS) {
		return false;
	}

	return true;
}

static void message_received(void * const context,
		const mqtt_message_t * const msg)
{
	debug("App received message %.*s", msg->payload_size, msg->payload);
	unused(context);
}

static void version_received(void * const context,
		const mqtt_message_t * const msg)
{
	unused(context);
	debug("VERSION: %s, %.*s", def2str(VERSION),
			msg->payload_size, msg->payload);
}

static bool subscribe_topics(void)
{
	mqtt_subscribe_t version = {
		.topic_filter = m.topic.version,
		.qos = MQTT_QOS_1,
		.callback = {
			.run = version_received,
			.context = NULL,
		},
	};
	mqtt_subscribe_t event = {
		.topic_filter = m.topic.event,
		.qos = MQTT_QOS_1,
		.callback = {
			.run = message_received,
			.context = NULL,
		},
	};
	mqtt_subscribe_t logging = {
		.topic_filter = m.topic.logging,
		.qos = MQTT_QOS_1,
		.callback = {
			.run = message_received,
			.context = NULL,
		},
	};

	if (mqtt_subscribe(m.mqtt, &version) != MQTT_SUCCESS
			|| mqtt_subscribe(m.mqtt, &event) != MQTT_SUCCESS
			|| mqtt_subscribe(m.mqtt, &logging) != MQTT_SUCCESS) {
		return false;
	}

	return true;
}

static bool module_init(const char *reporter_name)
{
	m.topic.version = get_topic_path_allocated(reporter_name, "version");
	m.topic.event = get_topic_path_allocated(reporter_name, "event");
	m.topic.report = get_topic_path_allocated(reporter_name, "report");
	m.topic.logging = get_topic_path_allocated(reporter_name, "logging");
	m.topic.heartbeat = get_topic_path_allocated(reporter_name, "heartbeat");
	m.topic.will = get_topic_path_allocated(reporter_name, "will");

	if (m.topic.version == NULL
			|| m.topic.event == NULL
			|| m.topic.report == NULL
			|| m.topic.logging == NULL
			|| m.topic.heartbeat == NULL
			|| m.topic.will == NULL) {
		return false;
	}

	return true;
}

bool reporter_send(const void *data, size_t data_size)
{
	// TODO: queue
	debug("rssi %d", wifiman_get_rssi());
	return mqtt_publish(m.mqtt, &(mqtt_message_t) {
			.qos = MQTT_QOS_1,
			.topic = m.topic.report,
			.payload = (const uint8_t *)data,
			.payload_size = data_size, })
		== MQTT_SUCCESS;
}

bool reporter_send_event(const void *data, size_t data_size)
{
	unused(data);
	unused(data_size);
	return false;
}

bool reporter_collect(const void *data, size_t data_size)
{
	unused(data);
	unused(data_size);
	return false;
}

bool reporter_start(void)
{
	return false;
}

bool reporter_init(const char *reporter_name)
{
	if (!module_init(reporter_name)) {
		return false;
	}
	if (!network_interface_init()) {
		return false;
	}
	if (!open_mqtt_connection(reporter_name)) {
		return false;
	}
	if (!subscribe_topics()) {
		return false;
	}
#if 1
	uint8_t ip[4], mac[6] = { 0, };
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
#endif

	return true;
}
