#ifndef OTA_H
#define OTA_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "ota/protocol.h"
#include "ota/parser.h"

#define OTA_VERSION_MAXLEN		12

typedef enum {
	OTA_SUCCESS			= 0,
	OTA_CANCELED,
	OTA_NO_ENOUGH_SPACE,
	OTA_TIMEOUT,
	OTA_AUTHENTICATION_FAILURE,
	OTA_CORRUPTED_FILE,
} ota_error_t;

typedef struct ota_request {
	char version[OTA_VERSION_MAXLEN];
	bool force;
	size_t file_size;
	uint16_t file_chunk_size;
	int file_chunk_index;
} ota_request_t;

typedef struct ota_data_chunk {
	int index;
	uint16_t data_size;
	const uint8_t *data;
} ota_chunk_t;

void ota_init(void *context, const ota_protocol_t *protocol,
		const ota_parser_t *parser);
void ota_start(void *context, const void *msg, size_t msgsize);

#endif /* OTA_H */
