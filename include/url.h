#ifndef URL_H
#define URL_H

#include <stddef.h>

size_t url_decode(const char *src, char *dst, size_t maxlen);
void url_convert_plus_to_space(char *str);

#endif /* URL_H */
