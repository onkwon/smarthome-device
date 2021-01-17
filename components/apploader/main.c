#include <stdint.h>
#include "dfu/dfu.h"
#include "dfu/flash.h"

extern uintptr_t __loader_data_start;
extern uintptr_t __loader_data_end;
extern uintptr_t __loader_text_end;
extern uintptr_t __loader_bss_start;
extern uintptr_t __loader_bss_end;

void app_init(void);

void apploader(void);
void apploader_initialize_memory(void);
void apploader_run(void);

void apploader_initialize_memory(void)
{
	const uintptr_t *end, *src;
	uintptr_t *dst;
	uintptr_t i;

	/* copy .data section from flash to ram */
	dst = (uintptr_t *)&__loader_data_start;
	end = (const uintptr_t *)&__loader_data_end;
	src = (const uintptr_t *)&__loader_text_end;

	for (i = 0; &dst[i] < end; i++) {
		dst[i] = src[i];
	}

	/* clear .bss section */
	dst = (uintptr_t *)&__loader_bss_start;
	end = (const uintptr_t *)&__loader_bss_end;

	for (i = 0; &dst[i] < end; i++) {
		dst[i] = 0;
	}
}

void apploader_run(void)
{
	dfu_init(dfu_flash());

	if (dfu_has_update()) {
		dfu_update();
	}

	app_init();
}

void apploader(void)
{
	apploader_initialize_memory();
	apploader_run();
}
