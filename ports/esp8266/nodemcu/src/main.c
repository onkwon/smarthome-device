#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"

#define DEFAULT_TASK_PRIORITY				1
#define DEFAULT_TASK_STACK_SIZE				3072

extern uintptr_t _app_data;
extern uintptr_t _app_data_end;
extern uintptr_t _app_text_end;
extern uintptr_t _app_bss;
extern uintptr_t _app_bss_end;

extern void application(void);

void _app_main(void);
void start_application(void);
void initialize_memory(void);

static void esp_init(void)
{
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(nvs_flash_init());
}

static void app_wrapper(void *e)
{
	(void)e;
	application();
	vTaskDelete(NULL);
}

void initialize_memory(void)
{
	const uintptr_t *end, *src;
	uintptr_t *dst;
	uintptr_t i;

	/* copy .data section from flash to ram */
	dst = (uintptr_t *)&_app_data;
	end = (const uintptr_t *)&_app_data_end;
	src = (const uintptr_t *)&_app_text_end;

	for (i = 0; &dst[i] < end; i++) {
		dst[i] = src[i];
	}

	/* clear .bss section */
	dst = (uintptr_t *)&_app_bss;
	end = (const uintptr_t *)&_app_bss_end;

	for (i = 0; &dst[i] < end; i++) {
		dst[i] = 0;
	}
}

void start_application(void)
{
	esp_init();

	xTaskCreate(app_wrapper,
			"app",
			DEFAULT_TASK_STACK_SIZE,
			NULL,
			DEFAULT_TASK_PRIORITY,
			NULL);
}

void _app_main(void)
{
	initialize_memory();
	start_application();
}
