#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE		32

typedef union {
	char _size[108];
	long _align;
} sha256_t;

void sha256_start(sha256_t *obj);
void sha256_update(sha256_t *self, const void *data, size_t datasize);
void sha256_finish(sha256_t *self, uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* SHA256_H */
