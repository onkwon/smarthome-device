#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	MQTT_SUCCESS			= 0,
	MQTT_ERROR,
	MQTT_ERROR_SOCKET,
	MQTT_ERROR_SESSION,
	MQTT_ERROR_CONNECT,
	MQTT_ERROR_SUBSCRIBE,
	MQTT_NO_ROOM_FOR_SUBSCRIPTION,
	MQTT_ERROR_PUBLISH,
	MQTT_ERROR_MAX,
} mqtt_error_t;

typedef enum {
	MQTT_QOS_0			= 0,
	MQTT_QOS_1,
	MQTT_QOS_2,
	MQTT_QOS_MAX,
} mqtt_qos_t;

typedef struct {
	const char *url;
	const char *client_id;
	uint16_t keepalive_sec;
	uint16_t port;
	bool clean_session;
	bool persistent;
	bool reconnect;
	struct {
		const void *ca_cert;
		size_t ca_cert_len;
		const void *client_cert;
		size_t client_cert_len;
		const void *client_key;
		size_t client_key_len;
		const char *username;
		const char *password;
	} credential;
} mqtt_connect_t;

typedef struct {
	mqtt_qos_t qos;
	bool retain;
	uint32_t retry_ms;
	uint8_t retry_limit;
	const char *topic;
	size_t topic_len;
	const uint8_t *payload;
	size_t payload_size;
} mqtt_message_t;

typedef struct {
	void (*run)(void * const context, const mqtt_message_t * const message);
	void *context;
} mqtt_subscribe_callback_t;

typedef struct {
	mqtt_qos_t qos;
	const char *topic_filter;
	mqtt_subscribe_callback_t callback;
} mqtt_subscribe_t;

typedef struct mqtt_s mqtt_t;

mqtt_t *mqtt_new(void);
void mqtt_destroy(mqtt_t * const self);
mqtt_error_t mqtt_connect(mqtt_t * const self, const mqtt_connect_t * const conf);
mqtt_error_t mqtt_disconnect(mqtt_t * const self);
mqtt_error_t mqtt_set_lwt(mqtt_t * const self, const mqtt_message_t * const lwt);
mqtt_error_t mqtt_subscribe(mqtt_t * const self,
		const mqtt_subscribe_t * const sub);
mqtt_error_t mqtt_unsubscribe(mqtt_t * const self,
		const mqtt_subscribe_t * const sub);
mqtt_error_t mqtt_publish(mqtt_t * const self, const mqtt_message_t * const pub);

#endif /* MQTT_H */
