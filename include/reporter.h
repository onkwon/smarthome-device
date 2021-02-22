#ifndef REPORTER_H
#define REPORTER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
	REPORT_HEARTBEAT,
	REPORT_LOGGING,
	REPORT_DATA,
	REPORT_EVENT,
	REPORT_ROOM,
} report_t;

typedef struct reporter_s reporter_t;

reporter_t *reporter_new(const char *reporter_name);
bool reporter_start(void);

bool reporter_send(report_t type, const void *data, size_t data_size);
bool reporter_send_event(report_t type, const void *data, size_t data_size);
/* send sensor data periodic in sync with its own interval */
bool reporter_collect(const void *data, size_t data_size);

bool reporter_is_enabled(void);

#endif /* REPORTER_H */
