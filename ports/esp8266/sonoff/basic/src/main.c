#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"

#define DEFAULT_TASK_PRIORITY				1
#define DEFAULT_TASK_STACK_SIZE				3072

extern uintptr_t __app_data_start;
extern uintptr_t __app_data_end;
extern uintptr_t __app_text_end;
extern uintptr_t __app_bss_start;
extern uintptr_t __app_bss_end;

extern int main(void);

void _app_init(void);
void app_start(void);
void app_initialize_memory(void);

static void esp_init(void)
{
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(nvs_flash_init());
}

static void app_wrapper(void *e)
{
	(void)e;
	main();
	vTaskDelete(NULL);
}

void app_initialize_memory(void)
{
	const uintptr_t *end, *src;
	uintptr_t *dst;
	uintptr_t i;

	/* copy .data section from flash to ram */
	dst = (uintptr_t *)&__app_data_start;
	end = (const uintptr_t *)&__app_data_end;
	src = (const uintptr_t *)&__app_text_end;

	for (i = 0; &dst[i] < end; i++) {
		dst[i] = src[i];
	}

	/* clear .bss section */
	dst = (uintptr_t *)&__app_bss_start;
	end = (const uintptr_t *)&__app_bss_end;

	for (i = 0; &dst[i] < end; i++) {
		dst[i] = 0;
	}
}

void app_start(void)
{
	esp_init();

	xTaskCreate(app_wrapper,
			"app",
			DEFAULT_TASK_STACK_SIZE,
			NULL,
			DEFAULT_TASK_PRIORITY,
			NULL);
}

void _app_init(void)
{
	app_initialize_memory();
	app_start();
}
