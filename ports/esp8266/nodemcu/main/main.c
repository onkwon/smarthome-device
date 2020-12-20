#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"

#define DEFAULT_TASK_PRIORITY			1
#define DEFAULT_TASK_STACK_SIZE			3072

static void esp_init(void)
{
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(nvs_flash_init());
}

static void start_application(void *e)
{
	extern void application(void);
	application();
	vTaskDelete(NULL);
}

void app_main(void)
{
	esp_init();

	extern void app_start(void);
	xTaskCreate(start_application,
			"app",
			DEFAULT_TASK_STACK_SIZE,
			NULL,
			DEFAULT_TASK_PRIORITY,
			NULL);
}
