#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

size_t base64_encode(char *outcome, const void *data, size_t datasize);
size_t base64_decode(void *outcome, const char *str, size_t strsize);
size_t base64_decode_overwrite(char *inout, size_t strsize);

#endif /* BASE64_H */
