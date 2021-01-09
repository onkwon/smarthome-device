#include "../apploader/dfu_flash.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "libmcu/compiler.h"

#define FLASH_SECTOR_SIZE			4096

static bool dfu_flash_prepare(void *context)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	unused(context);
	return false;
}

static bool dfu_flash_finish(void *context)
{
	unused(context);
	return false;
}

static bool dfu_flash_read(void *buf, const void *addr, size_t bufsize)
{
	esp_err_t rc = spi_flash_read((size_t)addr, buf, bufsize);
	return rc == ESP_OK;
}

static bool dfu_flash_write(void *addr, const void *data, size_t datasize)
{
	esp_err_t rc = spi_flash_write((size_t)addr, data, datasize);
	return rc == ESP_OK;
}

static bool dfu_flash_overwrite(void *addr, const void *data, size_t datasize)
{
	esp_err_t rc = ESP_OK;

	if (((size_t)addr & (FLASH_SECTOR_SIZE-1)) == 0) {
		rc = spi_flash_erase_range((size_t)addr, FLASH_SECTOR_SIZE);
	}

	if (rc == ESP_OK) {
		rc = spi_flash_write((size_t)addr, data, datasize);
	}

	return rc == ESP_OK;
}

static bool dfu_flash_erase(void *addr, size_t size)
{
	size = ALIGN(size, FLASH_SECTOR_SIZE);
	esp_err_t rc = spi_flash_erase_range((size_t)addr, size);
	return rc == ESP_OK;
}

const dfu_io_t *dfu_flash(void)
{
	static const dfu_io_t io = {
		.prepare = dfu_flash_prepare,
		.write = dfu_flash_write,
		.overwrite = dfu_flash_overwrite,
		.read = dfu_flash_read,
		.erase = dfu_flash_erase,
		.finish = dfu_flash_finish,
	};

	return &io;
}
