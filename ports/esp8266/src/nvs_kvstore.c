// NOTE: maximum key and namespace string length is 15 bytes
#include "nvs_kvstore.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

struct nvs_kvstore_s {
	kvstore_t ops;
	nvs_handle handle;
};

static int open_internal(const char *ns, nvs_handle *namespace_handle)
{
	return !nvs_open(ns, NVS_READWRITE, namespace_handle);
}

static size_t write_internal(kvstore_t *self, const char *key, const void *value, size_t size)
{
	struct nvs_kvstore_s *p = (typeof(p))self;

	if (nvs_set_blob(p->handle, key, value, size)) {
		return 0;
	}

	return !nvs_commit(p->handle)? size : 0;
}

static size_t read_internal(const kvstore_t *self, const char *key, void *buf, size_t bufsize)
{
	const struct nvs_kvstore_s *p = (typeof(p))self;
	return !nvs_get_blob(p->handle, key, buf, &bufsize)? bufsize : 0;
}

kvstore_t *nvs_kvstore_open(const char *ns)
{
	struct nvs_kvstore_s *p;

	if (!(p = malloc(sizeof(*p)))) {
		return NULL;
	}

	if (!open_internal(ns, &p->handle)) {
		free(p);
		return NULL;
	}

	p->ops = (typeof(p->ops)) {
		.write = write_internal,
		.read = read_internal,
	};

	return &p->ops;
}

void nvs_kvstore_close(kvstore_t *kvstore)
{
	nvs_close(((typeof(struct nvs_kvstore_s *))kvstore)->handle);
	free(kvstore);
}

int nvs_kvstore_init(void)
{
	return nvs_flash_init();
}
