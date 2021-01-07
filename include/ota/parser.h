#ifndef OTA_PARSER_H
#define OTA_PARSER_H

#include <stddef.h>
#include <stdbool.h>

struct ota_request;

typedef struct ota_parser {
	bool (*decode)(void *outcome, const void *msg, size_t msgsize);
	size_t (*encode)(void *buf, size_t bufsize,
			const struct ota_request *target);
} ota_parser_t;

#endif /* OTA_PARSER_H */
