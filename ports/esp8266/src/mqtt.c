#include "include/mqtt.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "libmcu/logging.h"
#include "libmcu/compiler.h"
#include "mqtt_client.h"
#include "jobpool.h"

#if !defined(MQTT_MAX_SUBSCRIPTIONS)
#define MQTT_MAX_SUBSCRIPTIONS		10
#endif
#if !defined(MQTT_NETWORK_BUFSIZE)
#define MQTT_NETWORK_BUFSIZE		1024
#endif

struct mqtt_server_info {
	esp_mqtt_client_handle_t handle;
	esp_mqtt_client_config_t conf;
	bool connected;
};

struct mqtt_s {
	pthread_mutex_t lock;
	struct mqtt_server_info server;
	mqtt_message_t lwt;
	struct {
		mqtt_subscribe_t pool[MQTT_MAX_SUBSCRIPTIONS];
		uint8_t length;
	} subscription;
};

static inline bool is_subscription_unused(const mqtt_subscribe_t * const p)
{
	return p->topic_filter == NULL;
}

static inline void set_subscription_unused(mqtt_subscribe_t * const p)
{
	if (p != NULL) {
		p->topic_filter = NULL;
	}
}

static inline uint8_t get_subscription_count(const mqtt_t * const mqtt)
{
	uint8_t n = 0;
	for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
		const mqtt_subscribe_t *p = &mqtt->subscription.pool[i];
		if (!is_subscription_unused(p)) {
			n++;
		}
	}
	return n;
}

static inline mqtt_subscribe_t *get_unused_subscription(mqtt_t * const mqtt)
{
	for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
		mqtt_subscribe_t *p = &mqtt->subscription.pool[i];
		if (is_subscription_unused(p)) {
			return p;
		}
	}
	return NULL;
}

static void get_next_topic_word(const char **s)
{
	while (**s != '/' && **s != '\0') {
		(*s)++;
	}
}

static bool is_topic_matched(const char *filter, const char *topic)
{
	if (filter == NULL || topic == NULL) {
		return false;
	}

	while (*filter != '\0' && *topic != '\0') {
		if (*filter == '#') {
			return true;
		}
		if (*filter == '+') {
			get_next_topic_word(&filter);
			get_next_topic_word(&topic);
			continue;
		}

		if (*filter != *topic) {
			return false;
		}

		filter++;
		topic++;
	}

	if (*filter != '\0') {
		return false;
	}
	if (*topic == '\0') {
		return true;
	}

	return false;
}

static const mqtt_subscribe_t *get_subscription_by_topic(
		const mqtt_t * const mqtt, const char *topic)
{
	for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
		const mqtt_subscribe_t *p = &mqtt->subscription.pool[i];
		if (is_subscription_unused(p)) {
			continue;
		}
		if (is_topic_matched(p->topic_filter, topic)) {
			return p;
		}
	}
	return NULL;
}

static mqtt_error_t mqtt_subscribe_internal(mqtt_t * const self,
		const mqtt_subscribe_t * const sub)
{
	if (esp_mqtt_client_subscribe(self->server.handle,
				sub->topic_filter, sub->qos) <= 0) {
		error("Subscription to %s failed.", sub->topic_filter);
		return MQTT_ERROR_SUBSCRIBE;
	}

	return MQTT_SUCCESS;
}

static void resubscribe_topics(void *context)
{
	mqtt_t *mqtt = (mqtt_t *)context;
	uint8_t len = get_subscription_count(mqtt);

	if (len == mqtt->subscription.length) {
		return;
	}

	for (uint8_t i = 0; len > 0 && i < MQTT_MAX_SUBSCRIPTIONS; i++) {
		mqtt_subscribe_t *p = &mqtt->subscription.pool[i];
		if (!is_subscription_unused(p)) {
			mqtt_error_t err = mqtt_subscribe_internal(mqtt, p);
			assert(err == MQTT_SUCCESS);
			len--;
		}
	}
	assert(len == 0);
}

static void process_incoming_publish(const mqtt_t * const mqtt,
		const esp_mqtt_event_handle_t event)
{
	debug("Incoming message(%u) to %.*s",
			event->data_len, event->topic_len, event->topic);

	const mqtt_subscribe_t *sub = get_subscription_by_topic(mqtt,
			event->topic);
	if (sub != NULL) {
		mqtt_message_t t = {
			.topic = event->topic,
			.topic_len = event->topic_len,
			.payload = (const uint8_t *)event->data,
			.payload_size = event->data_len,
		};
		sub->callback.run(sub->callback.context, &t);
	}
}

static void process_response(const mqtt_t * const mqtt,
		const esp_mqtt_event_handle_t event)
{
	unused(mqtt);

	switch (event->event_id) {
	case MQTT_EVENT_SUBSCRIBED:
		info("Subscribed to %.*s", event->topic_len, event->topic);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		info("Unsubscribed from %.*s", event->topic_len, event->topic);
		break;
	case MQTT_EVENT_PUBLISHED:
		debug("Published to %.*s", event->topic_len, event->topic);
		break;
	default:
		error("Unknown event id: %d.", event->event_id);
		break;
	}
}

static void mqtt_event_handler(void *context,
		esp_event_base_t base, int32_t event_id, void *event_data)
{
	mqtt_t *mqtt = (mqtt_t *)context;
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
	esp_mqtt_client_handle_t client = event->client;

	unused(client);
	unused(base);
	unused(event_id);

	switch (event->event_id) {
	case MQTT_EVENT_DATA:
		process_incoming_publish(mqtt, event);
		break;
	case MQTT_EVENT_SUBSCRIBED:
	case MQTT_EVENT_UNSUBSCRIBED:
	case MQTT_EVENT_PUBLISHED:
		process_response(mqtt, event);
		break;
	case MQTT_EVENT_CONNECTED:
		mqtt->server.connected = true;
		bool scheduled = jobpool_schedule(resubscribe_topics, mqtt);
		assert(scheduled == true);
		info("Connected to the broker.");
		break;
	case MQTT_EVENT_DISCONNECTED:
		mqtt->server.connected = false;
		mqtt->subscription.length = 0;
		info("Disconnected from the broker.");
		break;
	case MQTT_EVENT_ERROR:
		error("type: 0x%x", event->error_handle->error_type);
		//MQTT_ERROR_TYPE_ESP_TLS
		//MQTT_ERROR_TYPE_CONNECTION_REFUSED
		break;
	default:
		error("Unknown event id: %d.", event->event_id);
		break;
	}
}

static void set_server_information(struct mqtt_server_info *p,
		const mqtt_connect_t * const conf)
{
	*p = (struct mqtt_server_info) {
		.conf = {
			.uri = conf->url,
			.cert_pem = (const char *)conf->credential.ca_cert,
			.client_cert_pem = (const char *)
				conf->credential.client_cert,
			.client_key_pem = (const char *)
				conf->credential.client_key,
			.buffer_size = MQTT_NETWORK_BUFSIZE,
		},
	};
}

static bool set_mqtt_context(mqtt_t *self, const mqtt_connect_t *conf)
{
	self->server.conf.client_id = conf->client_id;
	self->server.conf.disable_auto_reconnect = !conf->reconnect;
	self->server.conf.keepalive = conf->keepalive_sec;
	self->server.conf.disable_clean_session = !conf->clean_session;

	if (self->lwt.topic != NULL && self->lwt.payload != NULL) {
		self->server.conf.lwt_qos = self->lwt.qos;
		self->server.conf.lwt_topic = self->lwt.topic;
		self->server.conf.lwt_msg = (const char *)self->lwt.payload;
		self->server.conf.lwt_msg_len = self->lwt.payload_size;
	}

	self->server.handle = esp_mqtt_client_init(&self->server.conf);
	if (self->server.handle == NULL) {
		goto out_err;
	}
	if (esp_mqtt_client_register_event(self->server.handle,
				ESP_EVENT_ANY_ID, mqtt_event_handler, self)
			!= ESP_OK) {
		goto out_free_handle;
	}

	return true;
out_free_handle:
	esp_mqtt_client_destroy(self->server.handle);
out_err:
	return false;
}

mqtt_error_t mqtt_subscribe(mqtt_t * const self,
		const mqtt_subscribe_t * const sub)
{
	mqtt_error_t err = MQTT_NO_ROOM_FOR_SUBSCRIPTION;

	pthread_mutex_lock(&self->lock);
	{
		mqtt_subscribe_t *p = get_unused_subscription(self);

		if (p != NULL) {
			if ((err = mqtt_subscribe_internal(self, sub))
					== MQTT_SUCCESS) {
				memcpy(p, sub, sizeof(*p));
				self->subscription.length++;
			}
		}
	}
	pthread_mutex_unlock(&self->lock);

	return err;
}

mqtt_error_t mqtt_unsubscribe(mqtt_t * const self,
		const mqtt_subscribe_t * const sub)
{
	mqtt_error_t err = MQTT_SUCCESS;

	pthread_mutex_lock(&self->lock);
	{
		if (esp_mqtt_client_unsubscribe(self->server.handle,
					sub->topic_filter) <= 0) {
			err = MQTT_ERROR_SUBSCRIBE;
			error("Failed to unsubscribe");
		}
		set_subscription_unused((mqtt_subscribe_t *)
				get_subscription_by_topic(self, sub->topic_filter));
	}
	pthread_mutex_unlock(&self->lock);

	return err;
}

mqtt_error_t mqtt_publish(mqtt_t * const self, const mqtt_message_t * const msg)
{
	mqtt_error_t err = MQTT_SUCCESS;

	pthread_mutex_lock(&self->lock);
	{
		if (esp_mqtt_client_publish(self->server.handle,
					msg->topic, (const char *)msg->payload,
					msg->payload_size, msg->qos, msg->retain)
				<= 0) {
			err = MQTT_ERROR_PUBLISH;
			error("Publish to %s failed.", msg->topic);
		}
	}
	pthread_mutex_unlock(&self->lock);

	return err;
}

mqtt_error_t mqtt_set_lwt(mqtt_t * const self, const mqtt_message_t * const lwt)
{
	pthread_mutex_lock(&self->lock);
	{
		memcpy(&self->lwt, lwt, sizeof(*lwt));
	}
	pthread_mutex_unlock(&self->lock);
	return MQTT_SUCCESS;
}

mqtt_error_t mqtt_connect(mqtt_t * const self, const mqtt_connect_t * const conf)
{
	mqtt_error_t err = MQTT_SUCCESS;

	pthread_mutex_lock(&self->lock);
	{
		set_server_information(&self->server, conf);

		if (!set_mqtt_context(self, conf)) {
			err = MQTT_ERROR_SOCKET;
		}
		if (err != MQTT_ERROR_SOCKET
				&& esp_mqtt_client_start(self->server.handle)
				!= ESP_OK) {
			esp_mqtt_client_destroy(self->server.handle);
			err = MQTT_ERROR_CONNECT;
		}
	}
	pthread_mutex_unlock(&self->lock);

	return err;
}

mqtt_error_t mqtt_disconnect(mqtt_t * const self)
{
	mqtt_error_t err = MQTT_SUCCESS;
	pthread_mutex_lock(&self->lock);
	{
		if (esp_mqtt_client_stop(self->server.handle) != ESP_OK) {
			err = MQTT_ERROR_CONNECT;
			error("Failed to disconnect from broker");
		}
	}
	pthread_mutex_unlock(&self->lock);
	return err;
}

mqtt_t *mqtt_new(void)
{
	mqtt_t *obj = calloc(1, sizeof(*obj));
	if (!obj) {
		goto out;
	}

	if (pthread_mutex_init(&obj->lock, NULL)) {
		goto out_free;
	}

	return obj;
out_free:
	free(obj);
out:
	return NULL;
}

void mqtt_destroy(mqtt_t * const self)
{
	pthread_mutex_lock(&self->lock);
	if (self->server.connected) {
		if (esp_mqtt_client_stop(self->server.handle) != ESP_OK) {
			error("Failed to disconnect from broker");
		}
	}
	if (esp_mqtt_client_destroy(self->server.handle) != ESP_OK) {
		error("Failed to destroy mqtt");
	}
	free(self);
}
