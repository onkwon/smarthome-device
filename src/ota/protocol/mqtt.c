#include "ota/protocol/mqtt.h"
#include "mqtt.h"
#include "../../topic.h"

static struct {
	void (*ota_data_handler)(void *context,
			const void *data, size_t datasize);
	mqtt_subscribe_t data_topic;
} m;

static bool publish_to(void *context, const char *topic,
		const void *data, size_t datasize)
{
	mqtt_t *mqtt = (mqtt_t *)context;
	if (mqtt_publish(mqtt, &(mqtt_message_t) {
			.qos = MQTT_QOS_1,
			.topic = topic,
			.payload = (const uint8_t *)data,
			.payload_size = datasize, }) != MQTT_SUCCESS) {
		return false;
	}
	return true;
}

static bool request(void *context, const void *data, size_t datasize)
{
	return publish_to(context, &TOPICS[TOPIC_SUB_VERSION_DATA][4],
			data, datasize);
}

static bool report(void *context, const void *data, size_t datasize)
{
	return publish_to(context, &TOPICS[TOPIC_SUB_VERSION][4],
			data, datasize);
}

static void mqtt_message_handler(void * const context,
		const mqtt_message_t * const msg)
{
	if (m.ota_data_handler == NULL) {
		return;
	}

	m.ota_data_handler(context, msg->payload, msg->payload_size);
}

static bool prepare(void *context, void (*ota_data_handler)(void *context,
			const void *data, size_t datasize), sem_t *next_chunk)
{
	mqtt_t *mqtt = (mqtt_t *)context;

	m.ota_data_handler = ota_data_handler;
	m.data_topic = (mqtt_subscribe_t) {
		.topic_filter = TOPICS[TOPIC_SUB_VERSION_DATA],
		.qos = MQTT_QOS_1,
		.callback = {
			.run = mqtt_message_handler,
			.context = next_chunk,
		},
	};
	if (mqtt_subscribe(mqtt, &m.data_topic) != MQTT_SUCCESS) {
		return false;
	}

	return true;
}

static bool finish(void *context)
{
	mqtt_t *mqtt = (mqtt_t *)context;
	if (mqtt_unsubscribe(mqtt, &m.data_topic) != MQTT_SUCCESS) {
		return false;
	}
	return true;
}

const ota_protocol_t *ota_mqtt(void)
{
	static ota_protocol_t mqtt = {
		.name = "mqtt",
		.request = request,
		.report = report,
		.prepare = prepare,
		.finish = finish,
	};

	return &mqtt;
}
