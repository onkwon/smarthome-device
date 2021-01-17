#include "url.h"
#include <stdlib.h>
#include <ctype.h>

// https://tools.ietf.org/html/rfc3986
size_t url_decode(const char *src, char *dst, size_t maxlen)
{
	size_t index, outdex;

	if (!src || !dst || !maxlen) {
		return 0;
	}

	for (index = outdex = 0; src[index] && outdex < maxlen-1; index++) {
		if (src[index] != '%') {
			dst[outdex++] = src[index];
			continue;
		}

		if (!isxdigit(src[index+1])
				|| !isxdigit(src[index+2])) {
			dst[outdex++] = src[index];
			continue;
		}

		char t[3] = { src[index+1], src[index+2], 0 };
		dst[outdex++] = (char)strtol(t, NULL, 16);
		index += 2;
	}

	dst[outdex] = '\0';

	return outdex;
}

void url_convert_plus_to_space(char *str)
{
	if (!str) {
		return;
	}

	for (int i = 0; str[i]; i++) {
		if (str[i] == '+') {
			str[i] = ' ';
		}
	}
}
