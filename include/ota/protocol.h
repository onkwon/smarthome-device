#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H

#include <stddef.h>
#include <stdbool.h>
#include <semaphore.h>
#include "ota/parser.h"

typedef struct {
	const char *name;
	bool (*prepare)(void *context, void (*rx_data_handler)(void *context,
				const void *data, size_t datasize),
			sem_t *next_chunk);
	bool (*finish)(void *context);
	/* request the next file chunk */
	bool (*request)(void *context, const void *data, size_t datasize);
	/* report the current version */
	bool (*report)(void *context, const void *data, size_t datasize);
} ota_protocol_t;

#endif /* OTA_PROTOCOL_H */
