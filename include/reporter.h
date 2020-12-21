#ifndef REPORTER_H
#define REPORTER_H

#include <stdbool.h>
#include <stddef.h>

bool reporter_init(const char *reporter_name);
bool reporter_start(void);

/* send as soon as possible */
bool reporter_send(const void *data, size_t data_size);
bool reporter_send_event(const void *data, size_t data_size);
/* send periodic with its own interval */
bool reporter_collect(const void *data, size_t data_size);

#endif /* REPORTER_H */
