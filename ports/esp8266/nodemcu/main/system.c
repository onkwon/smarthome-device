#include "libmcu/system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_ota_ops.h"

static const char *stringify_reboot_reason(int reason)
{
	switch (reason) {
	default:
	case ESP_RST_UNKNOWN:
		return "unknown";
	case ESP_RST_POWERON:
		return "power-on";
	case ESP_RST_EXT:
		return "external reset pin";
	case ESP_RST_SW:
		return "software reset";
	case ESP_RST_PANIC:
		return "panic";
	case ESP_RST_INT_WDT:
		return "interrupt watchdog";
	case ESP_RST_TASK_WDT:
		return "task watchdog";
	case ESP_RST_WDT:
		return "watchdog";
	case ESP_RST_DEEPSLEEP:
		return "deepsleep";
	case ESP_RST_BROWNOUT:
		return "brownout";
	case ESP_RST_SDIO:
		return "sdio";
	}
}

static int get_reboot_reason(void)
{
	return (int)esp_reset_reason();
}

const char *system_get_reboot_reason_string(void)
{
	return stringify_reboot_reason(get_reboot_reason());
}

void system_reboot(void)
{
	esp_restart();
	while (1);
}

unsigned long system_get_free_heap_bytes(void)
{
	return esp_get_free_heap_size();
}

unsigned long system_get_current_stack_watermark(void)
{
	return uxTaskGetStackHighWaterMark(NULL);
}

const char *system_get_version_string(void)
{
        const esp_app_desc_t *app_desc = esp_ota_get_app_description();
	return app_desc->version;
}

unsigned long system_get_nr_tasks(void)
{
	return uxTaskGetNumberOfTasks();
}

const char *system_get_current_task_name(void)
{
	return pcTaskGetTaskName(NULL);
}

void system_print_tasks_info(void)
{
#if 0
	char *buf = malloc(2048);
	if (buf) {
		vTaskGetRunTimeStats(buf);
		printf("%s\n", buf);
		free(buf);
	}
#else
	const size_t bytes_per_task = 40; /* see vTaskList description */
	char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
	if (task_list_buffer == NULL) {
		// OOM
		return;
	}
	vTaskList(task_list_buffer);
	printf("Task Name\tStatus\tPrio\tHWM\tTask Number\tCore\n");
	printf("%s\n", task_list_buffer);
	free(task_list_buffer);
#endif
}

int system_random(void)
{
	return esp_random();
}

const char *system_get_serial_number_string(void)
{
	return "sn1234";
}
