#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"

#include "jsmnn.h"
#include "libmcu/logging.h"

#define MEMSIZE		1024

TEST_GROUP(jsmnn) {
	uint8_t mem[MEMSIZE];

	void setup(void) {
	}
	void teardown() {
	}
};

TEST(jsmnn, load_ShouldReturnObject) {
	const char *fixed_json = "{\"key\":\"value\"}";
	CHECK(jsmn_load(mem, sizeof(mem), fixed_json, strlen(fixed_json)) != NULL);
}

TEST(jsmnn, load_ShouldReturnNull_WhenNullJsonGiven) {
	POINTERS_EQUAL(NULL, jsmn_load(mem, sizeof(mem), NULL, 0));
}

TEST(jsmnn, load_ShouldReturnNull_WhenZeroLengthGiven) {
	const char *fixed_json = "{\"key\":\"value\"}";
	POINTERS_EQUAL(NULL, jsmn_load(mem, sizeof(mem), fixed_json, 0));
}

TEST(jsmnn, get_ShouldReturnValue) {
	const char *fixed_json = "{\"key\":\"value\",\"ival\":1234,\"neg\":-12}";
	jsmn_t *jsmn = jsmn_load(mem, sizeof(mem), fixed_json, strlen(fixed_json));
	jsmn_value_t data;
	CHECK(jsmn_get(jsmn, &data, "ival") == true);
	LONGS_EQUAL(1234, data.intval);
	CHECK(jsmn_get(jsmn, &data, "neg") == true);
	LONGS_EQUAL(-12, data.intval);
	CHECK(jsmn_get(jsmn, &data, "key") == true);
	STRNCMP_EQUAL("value", data.string, data.length);
}

TEST(jsmnn, t) {
	//{"type":"request","version":"1.2.3","packetSize":1024}
	char buf[256];
	jsmn_t *root = jsmn_create(buf, sizeof(buf));
	jsmn_add_object(root, NULL);
	jsmn_add_string(root, "type", "request");
	jsmn_add_string(root, "version", "1.2.3");
	jsmn_add_number(root, "packetSize", 1024);
	jsmn_add_boolean(root, "force", false);
	jsmn_add_object(root, "abc");
	jsmn_fin_object(root);
	jsmn_fin_object(root);
	debug("=> %s", jsmn_stringify(root));
}

TEST(jsmnn, count_ShouldReturnNumberOfTokens) {
	const char *fixed_json = "{\"key\":\"value\"}";
	jsmn_t *jsmn = jsmn_load(mem, sizeof(mem), fixed_json, strlen(fixed_json));
	LONGS_EQUAL(3, jsmn_count(jsmn));
}
