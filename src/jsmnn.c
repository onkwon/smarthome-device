#include "jsmnn.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "jsmn/jsmn.h"

#if !defined(MAX)
#define MAX(a, b)			(((a) < (b))? (b) : (a))
#endif

struct jsmn_obj {
	union {
		struct {
			unsigned int length;
			const char *original_string;
			jsmntok_t tokens[];
		}; // input
		struct {
			unsigned int capacity;
			unsigned int index;
			unsigned int nr_items;
			char buffer[];
		}; // output
	};
};

static size_t get_token_length(jsmntok_t *token)
{
	return token->end - token->start;
}

bool jsmn_get(jsmn_t *self, jsmn_value_t *data, const char *key)
{
	for (unsigned int i = 0; i < self->length; i++) {
		if (self->tokens[i].type != JSMN_STRING
				|| self->tokens[i].size < 1) {
			continue;
		}

		jsmntok_t *token = &self->tokens[i];
		size_t len = get_token_length(token);
		const char *s = &self->original_string[token->start];

		if (strncmp(s, key, MAX(len, strlen(key))) == 0) {
			token = &self->tokens[i+1];
			s = &self->original_string[token->start];
			len = get_token_length(token);

			data->string = s;
			data->length = len;
			data->intval = (int)strtol(s, NULL, 10);

			return true;
		}
	}

	return false;
}

jsmn_t *jsmn_load(void *mem, size_t memsize, const char *js, size_t len)
{
	jsmn_t *jsmn = (jsmn_t *)mem;
	jsmn->original_string = js;
	jsmn->length = 0;
	unsigned int max_tokens = (unsigned int)
		((memsize - sizeof(*jsmn)) / sizeof(jsmntok_t));

	jsmn_parser parser;
	jsmn_init(&parser);
	int n = jsmn_parse(&parser, js, len, jsmn->tokens, max_tokens);
	if (n <= 0) {
		return NULL;
	}

	jsmn->length = (unsigned int)n;

	return jsmn;
}

jsmn_t *jsmn_create(void *buf, size_t bufsize)
{
	jsmn_t *jsmn = (jsmn_t *)buf;
	jsmn->capacity = (unsigned int)(bufsize - sizeof(*jsmn));
	jsmn->index = 0;
	jsmn->nr_items = 0;
	return jsmn;
}

static bool jsmn_add_internal(jsmn_t *self,
		const char *fmt, const char *key, const void *value)
{
	const char *comma = "";
	if (self->nr_items > 0) {
		comma = ",";
	}

	int len = snprintf(&self->buffer[self->index],
			self->capacity - self->index - 1,
			fmt,
			comma, key, value);
	if (len < 0) {
		return false;
	}

	self->index += (unsigned int)len;
	self->nr_items++;

	return true;
}

bool jsmn_add_object(jsmn_t *self, const char *key)
{
	if (key == NULL) {
		if (jsmn_add_internal(self, "%s%s", "{", NULL)) {
			self->nr_items--;
			return true;
		}
		return false;
	}
	return jsmn_add_internal(self, "%s\"%s\":%s", key, "{");
}

bool jsmn_fin_object(jsmn_t *self)
{
	if (self->index >= self->capacity-1) {
		return false;
	}
	self->buffer[self->index++] = '}';
	self->buffer[self->index] = '\0';
	return true;
}

bool jsmn_add_array(jsmn_t *self, const char *key)
{
	return jsmn_add_internal(self, "%s\"%s\":%s", key, "[");
}

bool jsmn_fin_array(jsmn_t *self)
{
	if (self->index >= self->capacity-1) {
		return false;
	}
	self->buffer[self->index++] = ']';
	self->buffer[self->index] = '\0';
	return true;
}

bool jsmn_add_string(jsmn_t *self, const char *key, const char *value)
{
	return jsmn_add_internal(self, "%s\"%s\":\"%s\"", key, value);
}

bool jsmn_add_number(jsmn_t *self, const char *key, int value)
{
	return jsmn_add_internal(self, "%s\"%s\":%d", key,
			(const void *)(long)value);
}

bool jsmn_add_boolean(jsmn_t *self, const char *key, bool value)
{
	return jsmn_add_internal(self, "%s\"%s\":%s",
			key, value == true? "true" : "false");
}

const char *jsmn_stringify(jsmn_t *self)
{
	return self->buffer;
}

unsigned int jsmn_count(jsmn_t *self)
{
	return self->length;
}
