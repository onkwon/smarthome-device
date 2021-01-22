#include "sha256.h"
#include "mbedtls/md.h"

void sha256_start(sha256_t *obj)
{
	mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)obj;
	mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
	mbedtls_md_init(ctx);
	mbedtls_md_setup(ctx, mbedtls_md_info_from_type(md_type), 0);
	mbedtls_md_starts(ctx);
}

void sha256_update(sha256_t *self, const void *data, size_t datasize)
{
	mbedtls_md_update((mbedtls_md_context_t *)self, data, datasize);
}

void sha256_finish(sha256_t *self, uint8_t digest[SHA256_DIGEST_SIZE])
{
	mbedtls_md_finish((mbedtls_md_context_t *)self, digest);
	mbedtls_md_free((mbedtls_md_context_t *)self);
}
