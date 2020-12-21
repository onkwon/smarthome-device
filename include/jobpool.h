#ifndef JOBPOOL_H
#define JOBPOOL_H

#include <stdbool.h>

bool jobpool_init(void);
bool jobpool_schedule(void (*job)(void *context), void *job_context);
unsigned int jobpool_count(void);

#endif /* JOBPOOL_H */
