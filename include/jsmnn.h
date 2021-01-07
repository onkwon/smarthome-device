#ifndef JSMNN_H
#define JSMNN_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define JSMN_VALUE(x)		(void *)(x)

typedef enum {
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOLEAN,
	JSON_NULL,
	JSON_OBJECT,
	JSON_ARRAY,
} jsmn_data_t;

typedef struct jsmn_obj jsmn_t;

typedef struct jsmn_data_obj {
	const char *string;
	size_t length;
	union {
		int intval;
		unsigned int uintval;
		bool boolval;
		float fval;
	};
} jsmn_value_t;

jsmn_t *jsmn_load(void *mem, size_t memsize, const char *js, size_t len);
bool jsmn_get(jsmn_t *self, jsmn_value_t *data, const char *key);

jsmn_t *jsmn_create(void *buf, size_t bufsize);
bool jsmn_add_object(jsmn_t *self, const char *key);
bool jsmn_fin_object(jsmn_t *self);
bool jsmn_add_array(jsmn_t *self, const char *key);
bool jsmn_fin_array(jsmn_t *self);
bool jsmn_add_string(jsmn_t *self, const char *key, const char *value);
bool jsmn_add_number(jsmn_t *self, const char *key, int value);
bool jsmn_add_boolean(jsmn_t *self, const char *key, bool value);
const char *jsmn_stringify(jsmn_t *self);

unsigned int jsmn_count(jsmn_t *self);

#if defined(__cplusplus)
}
#endif

#endif /* JSMNN_H */
