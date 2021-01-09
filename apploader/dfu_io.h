#ifndef DFU_IO_H
#define DFU_IO_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct {
	bool (*prepare)(void *context);
	bool (*write)(void *addr, const void *data, size_t datasize);
	bool (*overwrite)(void *addr, const void *data, size_t datasize);
	bool (*read)(void *buf, const void *addr, size_t bufsize);
	bool (*erase)(void *addr, size_t size);
	bool (*finish)(void *context);
} dfu_io_t;

#if defined(__cplusplus)
}
#endif

#endif /* DFU_IO_H */
