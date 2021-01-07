#include "libmcu/jobqueue.h"
#include "jobpool.h"

bool jobpool_schedule(void (*job)(void *context), void *job_context)
{
	job(job_context);
	return true;
}

bool jobpool_init(void)
{
	return true;
}

unsigned int jobpool_count(void)
{
	return 0;
}
