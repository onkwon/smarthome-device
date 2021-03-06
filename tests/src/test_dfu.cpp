#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"

#include <semaphore.h>
#include "libmcu/logging.h"

extern "C" {
#include "ota/ota.h"
#include "ota/format/json.h"
#include "ota/protocol/mqtt.h"
#include "mqtt.h"
#include "timext.h"
}

extern const char *TOPICS[1];
const char *TOPICS[1] = { NULL, };

static mqtt_subscribe_callback_t msg_callback;
static int expired;
static char received[128];
static int received_count;

mqtt_error_t mqtt_unsubscribe(mqtt_t * const self,
		const mqtt_subscribe_t * const sub)
{
	return MQTT_SUCCESS;
}

mqtt_error_t mqtt_subscribe(mqtt_t * const self,
		const mqtt_subscribe_t * const sub)
{
	memcpy(&msg_callback, &sub->callback, sizeof(msg_callback));
	return MQTT_SUCCESS;
}

static void send_next(void)
{
	static int index = 0;
	char buf[64];
	sprintf(buf, "{\"index\":%d,\"data\":\"dummy\"}", ++index);
	mqtt_message_t msg = {
		.payload = (const uint8_t *)buf,
		.payload_size = strlen(buf),
	};
	if (msg_callback.run != NULL) {
		msg_callback.run(msg_callback.context, &msg);
	}
}

mqtt_error_t mqtt_publish(mqtt_t * const self, const mqtt_message_t * const msg)
{
	info("%.*s", msg->payload_size, msg->payload);
	memcpy(received, msg->payload, msg->payload_size);
	received[msg->payload_size] = '\0';
	received_count++;

	send_next();

	return MQTT_SUCCESS;
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

unsigned int timeout_set(unsigned int msec)
{
	return 0;
}

bool timeout_is_expired(unsigned int goal)
{
	if (expired <= 0) {
		return false;
	}
	expired--;
	return true;
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
	return 0;
}

int sem_timedwait(sem_t *sem, unsigned int timeout_ms)
{
	return 0;
}

TEST_GROUP(ota) {
	void setup(void) {
		expired = 0;
		received_count = 0;
		ota_init(ota_json_parser());
		ota_init(reporter, ota_mqtt(), ota_json_parser());
	}
	void teardown() {
	}

	void send_message(const char *s) {
		mqtt_message_t msg = {
			.payload = (const uint8_t *)s,
			.payload_size = strlen(s),
		};

		msg_callback.run(msg_callback.context, &msg);
	}
};

TEST(ota, start_ShouldStop_WhenSameVersionRequested) {
	const char *fixed_request =
		"{\"version\":\"1.2.3\",\"size\":12345,\"force\":false}";
	const char *expected = "{\"version\":\"1.2.3\"}";
	mqtt_message_t msg = {
		.payload = (const uint8_t *)fixed_request,
		.payload_size = strlen(fixed_request),
	};
	ota_start_mqtt(NULL, &msg);
	STRCMP_EQUAL(expected, received);
}

TEST(ota, start_ShouldBeIgnored_WhenInvalidRequestGiven) {
	const char *fixed_request =
		"{\"ersion\":\"1.2.3\",\"size\":12345,\"force\":false}";
	mqtt_message_t msg = {
		.payload = (const uint8_t *)fixed_request,
		.payload_size = strlen(fixed_request),
	};
	ota_start_mqtt(NULL, &msg);
	LONGS_EQUAL(received_count, 0);
}

TEST(ota, start_ShouldBeIgnored_WhenSizeNotGiven) {
	const char *fixed_request =
		"{\"version\":\"1.2.3\",\"force\":false}";
	mqtt_message_t msg = {
		.payload = (const uint8_t *)fixed_request,
		.payload_size = strlen(fixed_request),
	};
	ota_start_mqtt(NULL, &msg);
	LONGS_EQUAL(received_count, 0);
}

TEST(ota, start_ShouldBeIgnored_WhenForceNotGiven) {
	const char *fixed_request =
		"{\"version\":\"1.2.3\",\"size\":12345,\"}";
	mqtt_message_t msg = {
		.payload = (const uint8_t *)fixed_request,
		.payload_size = strlen(fixed_request),
	};
	ota_start_mqtt(NULL, &msg);
	LONGS_EQUAL(received_count, 0);
}

#if 0
TEST(ota, start_Should) {
	const char *fixed_request =
		"{\"version\":\"1.2.4\",\"size\":12345,\"force\":false}";
	mqtt_message_t msg = {
		.payload = (const uint8_t *)fixed_request,
		.payload_size = strlen(fixed_request),
	};
	ota_start_mqtt(NULL, &msg);
}
#endif
