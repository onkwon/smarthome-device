#ifndef RETRY_H
#define RETRY_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

typedef enum {
	RETRY_SUCCESS		= 0,
	RETRY_EXHAUSTED,
} retry_error_t;

typedef struct {
	uint32_t next_max_jitter_ms;
	uint32_t max_backoff_ms;
	uint16_t min_backoff_ms;
	uint16_t max_jitter_ms;
	uint16_t max_attempts;
	uint16_t attempts;
} retry_t;

void retry_init(retry_t *param, uint8_t max_attempts, uint16_t max_jitter_ms,
		uint16_t min_backoff_ms, uint32_t max_backoff_ms);
void retry_reset(retry_t *param);
retry_error_t retry_backoff(retry_t *param);

#if defined(__cplusplus)
}
#endif

#endif /* RETRY_H */
