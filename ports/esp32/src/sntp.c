#include "esp_sntp.h"

static void sntp_callback(struct timeval *tv)
{
	(void)tv;
}

void sntp_test(void);
void sntp_test(void)
{
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_set_time_sync_notification_cb(sntp_callback);
	sntp_init();
}

#include <time.h>
#include "libmcu/logging.h"
void print_time(void);
void print_time(void)
{
	time_t now;
	time(&now);
	debug("TIME: %lu", now);
}
