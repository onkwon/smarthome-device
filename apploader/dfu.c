#include "dfu.h"
#include <string.h>
#include "libmcu/compiler.h"
#include "sha256.h"

#define DFU_COUNTER_SIZE		512
#define BITS_PER_BYTE			8

extern uint8_t __bootopt;
extern uint8_t __app_partition;
extern uint8_t __dfu_partition;

struct dfu_s {
	uintptr_t baseaddr;
	uintptr_t offset;
};

typedef struct {
	uint32_t magic;
	size_t datasize;
	uint8_t digest[SHA256_DIGEST_SIZE];
	uint8_t data[];
} LIBMCU_PACKED dfu_image_t;

static struct {
	uint8_t *counter;
	uint8_t *error_counter;
	const dfu_io_t *io;

	dfu_t image;
} m;

static int get_counter_index(const uint8_t counter[DFU_COUNTER_SIZE])
{
	int index;

	for (index = 0; index < DFU_COUNTER_SIZE; index++) {
		if (counter[index] != 0xffU) {
			break;
		}
	}

	return index;
}

static int count_internal(const uint8_t counter[DFU_COUNTER_SIZE])
{
	int index = get_counter_index(counter);
	if (index >= DFU_COUNTER_SIZE) {
		return 0;
	}

	int remain;
	for (remain = 0; remain < BITS_PER_BYTE; remain++) {
		if (((counter[index] >> remain) & (uint8_t)1U) == 1) {
			break;
		}
	}

	return (DFU_COUNTER_SIZE - index - 1) * BITS_PER_BYTE + remain;
}

static bool has_update_internal(const uint8_t counter[DFU_COUNTER_SIZE])
{
	return !!(count_internal(counter) & 1);
}

static void get_image_digest(const dfu_t *self,
		const dfu_image_t *image, void *digest)
{
	sha256_t sha256;
	sha256_start(&sha256);

	uint8_t buf[128];
	uintptr_t addr = self->baseaddr + sizeof(*image);
	size_t n = image->datasize / sizeof(buf);
	size_t remain = image->datasize % sizeof(buf);

	for (size_t i = 0; i < n; i++) {
		m.io->read(buf, (void *)(addr + i*sizeof(buf)), sizeof(buf));
		sha256_update(&sha256, buf, sizeof(buf));
	}

	m.io->read(buf, (void *)(addr + n*sizeof(buf)), remain);
	sha256_update(&sha256, buf, remain);

	sha256_finish(&sha256, digest);
}

static bool validate(const dfu_t *self, const dfu_image_t *image)
{
	if (image->magic != DFU_MAGIC) {
		return false;
	}
	if (self->offset != image->datasize + sizeof(*image)) {
		return false;
	}

	uint8_t digest[SHA256_DIGEST_SIZE] = { 0, };
	get_image_digest(self, image, digest);
	if (memcmp(digest, image->digest, sizeof(digest)) != 0) {
		return false;
	}

	return true;
}

static void copy_to_app_partition(dfu_t *dfu, const dfu_image_t *image)
{
	uint8_t buf[128];
	uintptr_t dst = (uintptr_t)&__app_partition;
	uintptr_t src = dfu->baseaddr + sizeof(*image);
	size_t n = image->datasize / sizeof(buf);
	size_t remain = image->datasize % sizeof(buf);

	for (size_t i = 0; i < n; i++) {
		m.io->read(buf, (void *)(src + i*sizeof(buf)), sizeof(buf));
		m.io->overwrite((void *)(dst + i*sizeof(buf)), buf, sizeof(buf));
	}

	m.io->read(buf, (void *)(src + n*sizeof(buf)), remain);
	m.io->overwrite((void *)(dst + n*sizeof(buf)), buf, remain);
}

static bool increase_by_one(uint8_t buf[DFU_COUNTER_SIZE])
{
	m.io->read(buf, m.counter, DFU_COUNTER_SIZE);
	int index = get_counter_index(buf);

	if (index == 0 && buf[index] == 0) {
		if (!m.io->erase(m.counter, DFU_COUNTER_SIZE)) {
			return false;
		}
		m.io->read(buf, m.counter, DFU_COUNTER_SIZE);
		index = get_counter_index(buf);
	}

	if (index >= DFU_COUNTER_SIZE) {
		index = DFU_COUNTER_SIZE - 1;
	}
	if (buf[index] == 0) {
		index -= 1;
	}

	uint8_t val = (uint8_t)(buf[index] << 1U);
	if (!m.io->write(&m.counter[index], &val, sizeof(val))) {
		return false;
	}

	return true;
}

bool dfu_has_update(void)
{
	uint8_t buf[DFU_COUNTER_SIZE];
	m.io->read(buf, m.counter, sizeof(buf));
	return has_update_internal(buf);
}

bool dfu_update(void)
{
	dfu_t dfu = {
		.baseaddr = (uintptr_t)&__dfu_partition,
		.offset = 0,
	};
	dfu_image_t image = { 0, };
	m.io->read(&image, (const void *)dfu.baseaddr, sizeof(image));
	dfu.offset = image.datasize + sizeof(image);

	if (!validate(&dfu, &image)) {
		return false;
	}

	copy_to_app_partition(&dfu, &image);

	return true;
}

bool dfu_finish(void)
{
	uint8_t buf[DFU_COUNTER_SIZE];

	m.io->read(buf, m.counter, sizeof(buf));
	if (!has_update_internal(buf)) {
		return true;
	}

	if (!increase_by_one(buf)) {
		return false;
	}

	m.io->read(buf, m.counter, sizeof(buf));
	return !has_update_internal(buf);
}

dfu_t *dfu_new(void)
{
	memset(&m.image, 0, sizeof(m.image));
	m.image.baseaddr = (uintptr_t)&__dfu_partition;
	return &m.image;
}

bool dfu_write(dfu_t *self, const void *data, size_t datasize)
{
	void *addr = (void *)(self->baseaddr + self->offset);
	if (!m.io->overwrite(addr, data, datasize)) {
		return false;
	}
	self->offset += datasize;
	return true;
}

bool dfu_validate(dfu_t *self)
{
	dfu_image_t image = { 0, };
	m.io->read(&image, (const void *)self->baseaddr, sizeof(image));

	if (!validate(self, &image)) {
		return false;
	}

	return true;
}

bool dfu_register(dfu_t *self)
{
	unused(self);

	uint8_t buf[DFU_COUNTER_SIZE];

	m.io->read(buf, m.counter, sizeof(buf));
	if (has_update_internal(buf)) {
		return true;
	}

	if (!increase_by_one(buf)) {
		return false;
	}

	m.io->read(buf, m.counter, sizeof(buf));
	return has_update_internal(buf);
}

int dfu_count(void)
{
	uint8_t buf[DFU_COUNTER_SIZE];
	m.io->read(buf, m.counter, sizeof(buf));
	// incresed by 2 per each update:
	// +1 by app when requesting which result in odd number and
	// +1 by loader when finishing which result in even number
	return count_internal(buf) / 2;
}

int dfu_count_error(void)
{
	uint8_t buf[DFU_COUNTER_SIZE];
	m.io->read(buf, m.error_counter, sizeof(buf));
	return count_internal(buf);
}

void dfu_init(const dfu_io_t *io)
{
	m.io = io;
	m.counter = (uint8_t *)((uintptr_t)&__bootopt);
	m.error_counter = &m.counter[DFU_COUNTER_SIZE];
}
