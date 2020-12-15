#include "include/retry.h"
#include "esp_system.h"
#include "libmcu/logging.h"
#include "include/sleep.h"

static uint32_t generate_random(void)
{
	return esp_random();
}

static uint16_t get_jitter(uint16_t max_jitter_ms)
{
	return generate_random() % max_jitter_ms;
}

static void reset_internal(retry_t *param)
{
	param->next_max_jitter_ms =
		param->min_backoff_ms + get_jitter(param->max_jitter_ms);
	param->attempts = 0;
}

retry_error_t retry_backoff(retry_t *param)
{
	if (param->attempts >= param->max_attempts) {
		reset_internal(param);
		return RETRY_EXHAUSTED;
	}

	info("Retry in %u ms", param->next_max_jitter_ms);
	sleep_ms(param->next_max_jitter_ms);

	uint16_t jitter = get_jitter(param->max_jitter_ms);
	param->next_max_jitter_ms = param->next_max_jitter_ms * 2 + jitter;
	if (param->next_max_jitter_ms > param->max_backoff_ms) {
		param->next_max_jitter_ms =
			param->max_backoff_ms - param->max_jitter_ms + jitter;
	}

	param->attempts++;

	return RETRY_SUCCESS;
}

void retry_reset(retry_t *param)
{
	reset_internal(param);
}

void retry_init(retry_t *param, uint8_t max_attempts, uint16_t max_jitter_ms,
		uint16_t min_backoff_ms, uint32_t max_backoff_ms)
{
	param->max_attempts = max_attempts;
	param->max_jitter_ms = max_jitter_ms;
	param->min_backoff_ms = min_backoff_ms;
	param->max_backoff_ms = max_backoff_ms;

	reset_internal(param);
}
