#include "sha256.h"
#include "esp_sha.h"

_Static_assert(sizeof(sha256_t) == sizeof(esp_sha256_t), "must be the same size");

void sha256_start(sha256_t *obj)
{
	esp_sha256_t *ctx = (esp_sha256_t *)obj;
	esp_sha256_init(ctx);
}

void sha256_update(sha256_t *self, const void *data, size_t datasize)
{
	esp_sha256_update((esp_sha256_t *)self, data, datasize);
}

void sha256_finish(sha256_t *self, uint8_t digest[SHA256_DIGEST_SIZE])
{
	esp_sha256_finish((esp_sha256_t *)self, digest);
}
