#include <assert.h>
#include "libmcu/logging.h"
#include "libmcu/system.h"
#include "sleep.h"
#include "jobpool.h"
#include "reporter.h"

extern void system_print_tasks_info(void);
void application(void);

void application(void)
{
	bool initialized = jobpool_init();
	assert(initialized == true);
	initialized = reporter_init(system_get_serial_number_string());
	assert(initialized == true);

	while (1) {
		system_print_tasks_info();
		debug("%s", system_get_reboot_reason_string());
		debug("heap: %d", system_get_free_heap_bytes());

		sleep_ms(20000);
		reporter_send("Hello", 5);
		sleep_ms(10000);
		//wifiman_disconnect();
	}
}

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
size_t logging_save(const logging_t type, const void *pc, const void *lr, ...)
{
	int len = printf("%lu: [%s] <%p,%p> ", (unsigned long)time(0),
			type == 0? "VERBOSE" :
			type == 1? "DEBUG" :
			type == 2? "INFO" :
			type == 3? "NOTICE" :
			type == 4? "WARN" :
			type == 5? "ERROR" :
			type == 6? "ALERT" : "UNKNOWN",
			pc, lr);
	va_list ap;
	va_start(ap, lr);
	const char *fmt = va_arg(ap, char *);
	if (fmt) {
		len += vfprintf(stdout, fmt, ap);
		len += printf("\n");
	}
	return (size_t)len;
}
