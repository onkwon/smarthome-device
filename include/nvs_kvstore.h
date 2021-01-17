#ifndef NVS_KVSTORE_H
#define NVS_KVSTORE_H

#include "libmcu/kvstore.h"

int nvs_kvstore_init(void);
kvstore_t *nvs_kvstore_open(const char *ns);
void nvs_kvstore_close(kvstore_t *kvstore);

#endif /* NVS_KVSTORE_H */
