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
#include "ota/ota.h"
#include "topic.h"

#include "wifi.h"
#include "mqtt.h"

#define MAX(a, b)				((a) < (b)? (b) : (a))

#define DEFAULT_MQTT_BROKER_ENDPOINT		""

#if !defined(DEFAULT_HOSTNAME)
#define DEFAULT_HOSTNAME			"smartswitch"
#endif

#define x509_ca_cert				NULL
#define x509_device_cert			NULL
#define x509_device_key				NULL

const char *TOPICS[TOPIC_MAX];

static struct {
	volatile bool reconnecting;
	mqtt_t *mqtt;
} m;

static bool connect_to_network(void)
{
	uint8_t n = WIFIMAN_MAX_NETWORK_PROFILES;

	for (uint8_t i = 0; i < n; i++) {
		uint8_t buf[sizeof(wifiman_network_profile_t)
			+ WIFIMAN_PASS_MAXLEN] = { 0, };
		wifiman_network_profile_t *profile = (void *)buf;

		if (!wifiman_get_network(profile, i)) {
			continue;
		}

		info("connecting to %s", profile->ssid);

		if (wifiman_connect(profile) == WIFIMAN_SUCCESS) {
			return true;
		}
	}

	return false;
}

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
		if (connect_to_network()) {
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
		m.reconnecting = true;
		bool scheduled = jobpool_schedule(wifi_reconnect, NULL);
		assert(scheduled == true);
		info("Try to reconnect");
	}
}

static bool network_interface_init(void)
{
	if (!wifiman_on()) {
		return false;
	}
	if (!wifiman_start_station()) {
		goto out_err_wifi;
	}
	if (!wifiman_set_hostname(DEFAULT_HOSTNAME)) {
		goto out_err_wifi_station;
	}
	if (wifiman_register_event_handler(WIFIMAN_EVENT_CONNECTED,
				wifi_state_change_event) != WIFIMAN_SUCCESS) {
		goto out_err_wifi_station;
	}
	if (wifiman_register_event_handler(WIFIMAN_EVENT_DISCONNECTED,
				wifi_state_change_event) != WIFIMAN_SUCCESS) {
		goto out_err_wifi_station;
	}
	if (!connect_to_network()) {
		goto out_err_wifi_station;
	}

	return true;

out_err_wifi_station:
	wifiman_stop_station();
out_err_wifi:
	wifiman_off();

	return false;
}

static const char *get_topic_path_allocated(bool subscribe,
		const char *client_id, const char *sub_topic)
{
	size_t len = sizeof(TOPIC_DEFAULT_SUBSCRIBE_PREFIX)
		+ strlen(client_id) + 2/*delimiter*/ + 1/*null*/;
	char base[sizeof(TOPIC_DEFAULT_SUBSCRIBE_PREFIX)+1]
		= TOPIC_DEFAULT_SUBSCRIBE_PREFIX;
	if (!subscribe) {
		len -= sizeof(TOPIC_DEFAULT_SUBSCRIBE_PREFIX)
			- sizeof(TOPIC_DEFAULT_PREFIX);
		strcpy(base, TOPIC_DEFAULT_PREFIX);
	}
	if (sub_topic != NULL) {
		len += strlen(sub_topic);
	}

	char *topic = (char *)malloc(len);
	if (topic != NULL) {
		int written = snprintf(topic, len-1, "%s/%s", base, client_id);
		if (written <= 0) {
			free(topic);
			return NULL;
		}
		snprintf(&topic[written], len-(size_t)written-1,
				"/%s", sub_topic);
		topic[len-1] = '\0';
	}

	return topic;
}

static bool open_mqtt_connection(const char *client_id)
{
	m.mqtt = mqtt_new();

	if (mqtt_set_lwt(m.mqtt, &(mqtt_message_t) {
				.qos = MQTT_QOS_1,
				.topic = TOPICS[TOPIC_PUB_WILL],
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
	ota_start(context, msg->payload, msg->payload_size);
}

// TODO: Remove dependency
#include "switch.h"
static void room_received(void * const context,
		const mqtt_message_t * const msg)
{
	debug("topic => %.*s", msg->topic_len, msg->topic);
	debug("room message %.*s", msg->payload_size, msg->payload);
	unused(context);

	switch (msg->payload[0]) {
	case '1':
		switch_set(1, true);
		break;
	case '0':
		switch_set(1, false);
		break;
	default:
		break;
	}
}

static bool subscribe_topics(void *context)
{
	mqtt_subscribe_t version = {
		.topic_filter = TOPICS[TOPIC_SUB_VERSION],
		.qos = MQTT_QOS_1,
		.callback = {
			.run = version_received,
			.context = context,
		},
	};
	mqtt_subscribe_t logging = {
		.topic_filter = TOPICS[TOPIC_SUB_LOGGING],
		.qos = MQTT_QOS_1,
		.callback = {
			.run = message_received,
			.context = context,
		},
	};
	mqtt_subscribe_t room = {
		.topic_filter = TOPICS[TOPIC_SUB_ROOM],
		.qos = MQTT_QOS_1,
		.callback = {
			.run = room_received,
			.context = context,
		},
	};

	if (mqtt_subscribe(m.mqtt, &version) != MQTT_SUCCESS
			|| mqtt_subscribe(m.mqtt, &logging) != MQTT_SUCCESS
			|| mqtt_subscribe(m.mqtt, &room) != MQTT_SUCCESS) {
		return false;
	}

	return true;
}

static bool module_topic_init(const char *reporter_name)
{
	TOPICS[TOPIC_SUB_VERSION]
		= get_topic_path_allocated(1, reporter_name, "version");
	TOPICS[TOPIC_SUB_VERSION_DATA]
		= get_topic_path_allocated(1, reporter_name, "version/data");
	TOPICS[TOPIC_SUB_LOGGING]
		= get_topic_path_allocated(1, reporter_name, "logging");
	TOPICS[TOPIC_SUB_ROOM]
		= get_topic_path_allocated(1, reporter_name, "room");
	TOPICS[TOPIC_PUB_HEARTBEAT]
		= get_topic_path_allocated(0, reporter_name, "heartbeat");
	TOPICS[TOPIC_PUB_WILL]
		= get_topic_path_allocated(0, reporter_name, "will");

	for (int i = 0; i < TOPIC_MAX; i++) {
		if (TOPICS[i] == NULL) {
			return false;
		}
	}

	return true;
}

static const char *get_report_topic_from_type(report_t type)
{
	switch (type) {
	case REPORT_HEARTBEAT:
		return TOPICS[TOPIC_PUB_HEARTBEAT];
	case REPORT_LOGGING:
		return TOPIC_SUB2PUB(TOPIC_SUB_LOGGING);
	case REPORT_ROOM:
		return TOPIC_SUB2PUB(TOPIC_SUB_ROOM);
	case REPORT_EVENT: /* fall through */
	case REPORT_DATA: /* fall through */
	default:
		return NULL;
	}
}

bool reporter_send(report_t type, const void *data, size_t data_size)
{
	const char *topic = get_report_topic_from_type(type);

	if (topic == NULL) {
		return false;
	}

	return mqtt_publish(m.mqtt, &(mqtt_message_t) {
			.qos = MQTT_QOS_1,
			.topic = topic,
			.payload = (const uint8_t *)data,
			.payload_size = data_size, })
		== MQTT_SUCCESS;
}

bool reporter_send_event(report_t type, const void *data, size_t data_size)
{
	const char *topic = get_report_topic_from_type(type);

	if (topic == NULL) {
		return false;
	}

	return mqtt_publish(m.mqtt, &(mqtt_message_t) {
			.qos = MQTT_QOS_1,
			.retain = true,
			.topic = topic,
			.payload = (const uint8_t *)data,
			.payload_size = data_size, })
		== MQTT_SUCCESS;
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

bool reporter_is_enabled(void)
{
	return wifiman_count_networks() != 0;
}

reporter_t *reporter_new(const char *reporter_name)
{
	if (!module_topic_init(reporter_name)) {
		return NULL;
	}
	if (!network_interface_init()) {
		return NULL;
	}
	if (!open_mqtt_connection(reporter_name)) {
		return NULL;
	}
	if (!subscribe_topics(m.mqtt)) {
		return NULL;
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

	return (reporter_t *)m.mqtt;
}
