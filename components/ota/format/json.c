#include "ota/format/json.h"

#include <string.h>

#include "libmcu/logging.h"
#include "libmcu/compiler.h"
#include "libmcu/base64.h"

#include "jsmnn.h"
#include "ota/ota.h"

#if !defined(MIN)
#define MIN(a, b)			(((a) > (b))? (b) : (a))
#endif
#if !defined(MAX)
#define MAX(a, b)			(((a) < (b))? (b) : (a))
#endif

#define CONST_CAST(t, v)		\
	(((union { const t cval; t val; }*)&(v))->val)

static size_t remove_json_meta(jsmn_t *json)
{
	char *dst = (char *)json;
	const char *src = jsmn_stringify(json);
	size_t len = 0;
	while (src[len] != '\0') {
		dst[len] = src[len];
		len++;
	}
	return len;
}

static size_t encode_json(void *buf, size_t bufsize, const ota_request_t *target)
{
	jsmn_t *json = jsmn_create((char *)buf, bufsize);
	jsmn_add_object(json, NULL);
	jsmn_add_string(json, "version", target->version);
	jsmn_add_number(json, "packet_size", target->file_chunk_size);
	jsmn_add_number(json, "index", target->file_chunk_index);
	jsmn_fin_object(json);

	return remove_json_meta(json);
}

static bool decode_json(void *outcome, const void *msg, size_t msgsize)
{
	uint8_t buf[384];
	jsmn_t *jsmn = jsmn_load(buf, sizeof(buf), msg, msgsize);
	if (jsmn == NULL) {
		return false;
	}

	jsmn_value_t type = { 0, };
	jsmn_value_t version = { 0, };
	jsmn_value_t size = { 0, };
	jsmn_value_t force = { 0, };
	jsmn_value_t index = { 0, };
	jsmn_value_t data = { 0, };

	jsmn_get(jsmn, &type, "type");
	jsmn_get(jsmn, &version, "version");
	jsmn_get(jsmn, &size, "size");
	jsmn_get(jsmn, &force, "force");
	jsmn_get(jsmn, &index, "index");
	jsmn_get(jsmn, &data, "data");

	if ((type.string != NULL && strncmp(type.string, "request",
				MAX(type.length, 7)) == 0)
			|| index.uintval == 0) {
		if (version.string == NULL || force.string == NULL
				|| size.uintval == 0) {
			return false;
		}
		ota_request_t *target = (ota_request_t *)outcome;
		target->version[version.length] = '\0';
		strncpy(target->version, version.string, version.length);
		target->file_size = size.uintval;
		target->force =
			!strncmp("true", force.string, MAX(4, force.length));
		debug("version %s, size %d, force %d", target->version,
				target->file_size, target->force);
		return true;
	}

	if (index.uintval == 0 || !jsmn_get(jsmn, &data, "data")) {
		return false;
	}

	ota_chunk_t *chunk = (ota_chunk_t *)outcome;
	chunk->index = index.intval;
	chunk->data_size = (uint16_t)base64_decode_overwrite(
			CONST_CAST(char *, data.string), data.length);
	chunk->data = (const uint8_t *)data.string;

	return true;
}

const ota_parser_t *ota_json_parser(void)
{
	static const ota_parser_t ota_parser = {
		.encode = encode_json,
		.decode = decode_json,
	};

	return &ota_parser;
}
