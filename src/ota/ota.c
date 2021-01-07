#include "ota/ota.h"

#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <assert.h>

#include "libmcu/logging.h"
#include "libmcu/compiler.h"

#include "jobpool.h"
#include "timext.h"
#include "../topic.h"

#define DEFAULT_FILE_CHUNK_SIZE		128
#define OTA_TIMEOUT_SEC			300 // 5-min
#define RTT_TIMEOUT_MSEC		5000
#define PAYLOAD_BUFSIZE			80

static struct {
	pthread_mutex_t lock;
	bool active;
	ota_request_t target;

	struct {
		const ota_protocol_t *ops;
		void *handle;
	} protocol;

	const ota_parser_t *parser;
} m;

static size_t get_downloaded_size(void)
{
	return (size_t)(m.target.file_chunk_index-1) * m.target.file_chunk_size;
}

static bool is_ota_done(void)
{
	int index = m.target.file_chunk_index;
	return (index > 0) && (get_downloaded_size() >= m.target.file_size);
}

static void file_chunk_arrived(void *context, const void *data, size_t datasize)
{
	sem_t *next_chunk = (sem_t *)context;
	ota_chunk_t chunk;

	if (!m.parser->decode(&chunk, data, datasize)) {
		error("cannot decode");
		goto out;

	}
	if (chunk.index != m.target.file_chunk_index) {
		error("wrong index %d, %d expected",
				chunk.index, m.target.file_chunk_index);
		goto out;
	}
	if (chunk.data_size < m.target.file_chunk_size
			&& chunk.index < (int)(m.target.file_size
				/ m.target.file_chunk_size - 1)) {
		error("wrong data chunk #%d, size %d",
				chunk.index, chunk.data_size);
		goto out;
	}

	debug("file chunk arrived %.*s", chunk.data_size, chunk.data);

	// 1. validate
	// 2. flash temporal
	m.target.file_chunk_index++;
out:
	sem_post(next_chunk);
}

static bool send_request(void *handle, const ota_protocol_t *protocol,
		const ota_parser_t *parser)
{
	if (m.target.file_chunk_size == 0) {
		m.target.file_chunk_size = DEFAULT_FILE_CHUNK_SIZE;
	}
	m.target.file_chunk_index = 1;

	uint8_t buf[PAYLOAD_BUFSIZE];
	size_t len = parser->encode(buf, sizeof(buf), &m.target);
	return protocol->request(handle, buf, len);
}

static bool proceed_next(void *handle, const ota_protocol_t *protocol,
		const ota_parser_t *parser)
{
	uint8_t buf[PAYLOAD_BUFSIZE];
	size_t len = parser->encode(buf, sizeof(buf), &m.target);
	return protocol->request(handle, buf, len);
}

static bool send_current_version(void *handle, const ota_protocol_t *protocol,
		const ota_parser_t *parser)
{
	ota_request_t ota = { 0, };
	uint8_t buf[PAYLOAD_BUFSIZE];

	strcpy(ota.version, &def2str(VERSION_TAG)[1]);
	size_t len = parser->encode(buf, sizeof(buf), &ota);
	return protocol->report(handle, buf, len);
}

static bool report_failure(void *handle, const ota_protocol_t *protocol,
		const ota_parser_t *parser)
{
	return send_current_version(handle, protocol, parser);
}

static bool ota_run(void *handle, const ota_protocol_t *protocol,
		const ota_parser_t *parser)
{
	sem_t next_chunk;
	bool rc = false;

	if (sem_init(&next_chunk, 0, 0) != 0) {
		return rc;
	}
	if (!protocol->prepare(handle, file_chunk_arrived, &next_chunk)) {
		return rc;
	}
	if (!send_request(handle, protocol, parser)) {
		return rc;
	}

	unsigned int tout = timeout_set(OTA_TIMEOUT_SEC * 1000U);
	while (!timeout_is_expired(tout)) {
		if (sem_timedwait(&next_chunk, RTT_TIMEOUT_MSEC) != 0) {
			error("timed out");
		}
		if (is_ota_done()) {
			// validate
			// reboot
			info("OTA done");
			rc = true;
			break;
		}
		if (!proceed_next(handle, protocol, parser)) {
			// error
		}

		info("%u/%u downloaded",
				get_downloaded_size(), m.target.file_size);
	}

	protocol->finish(handle);

	return rc;
}

static void ota_task(void *context)
{
	if (!ota_run(context, m.protocol.ops, m.parser)) {
		report_failure(context, m.protocol.ops, m.parser);
	}

	pthread_mutex_lock(&m.lock);
	{
		m.active = false;
	}
	pthread_mutex_unlock(&m.lock);
}

void ota_start(void *context, const void *msg, size_t msgsize)
{
	const char *version = &def2str(VERSION_TAG)[1];

	ota_request_t target = { 0, };
	if (!m.parser->decode(&target, msg, msgsize)) {
		error("Invalid message");
		return;
	}

	if (strcmp(version, target.version) == 0) {
		warn("same version requested: %s", version);
		report_failure(context, m.protocol.ops, m.parser);
		return;
	}

	bool already_in_progress;
	pthread_mutex_lock(&m.lock);
	{
		already_in_progress = m.active;
		if (!already_in_progress) {
			m.active = true;
		}
	}
	pthread_mutex_unlock(&m.lock);

	if (already_in_progress) {
		warn("already in progress");
		return;
	}

	memcpy(&m.target, &target, sizeof(m.target));
	jobpool_schedule(ota_task, context);
}

void ota_init(void *handle, const ota_protocol_t *protocol,
		const ota_parser_t *parser)
{
	pthread_mutex_init(&m.lock, NULL);
	m.protocol.handle = handle;
	m.protocol.ops = protocol;
	m.parser = parser;

	assert(handle != NULL);

	assert(protocol != NULL);
	assert(protocol->request != NULL);
	assert(protocol->report != NULL);
	assert(protocol->prepare != NULL);
	assert(protocol->finish != NULL);

	assert(parser != NULL);
	assert(parser->encode != NULL);
	assert(parser->decode != NULL);

	send_current_version(handle, protocol, parser);
}
