#ifndef DFU_H
#define DFU_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "dfu/io.h"

#define DFU_MAGIC				0xC0DEFEEDUL

typedef struct dfu_s dfu_t;

void dfu_init(const dfu_io_t *io);
bool dfu_has_update(void);
bool dfu_update(void);
bool dfu_finish(void);

dfu_t *dfu_new(void);
bool dfu_write(dfu_t *self, const void *data, size_t datasize);
bool dfu_validate(dfu_t *self);
bool dfu_register(dfu_t *self);

int dfu_count(void);
int dfu_count_error(void);

#if defined(__cplusplus)
}
#endif

#endif /* DFU_H */
