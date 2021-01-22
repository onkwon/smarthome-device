PLATFORM := esp32
PLATFORM_SDK_DIR := external/esp-idf

SRCDIRS += $(PLATFORM_DIR)/src

CROSS_COMPILE ?= xtensa-esp32-elf
CFLAGS += -mlongcalls

## Compiler errors
CFLAGS += \
	  -Wno-error=inline \
	  -Wno-error=unused-parameter \
	  -Wno-error=cast-qual \
	  -Wno-error=sign-conversion \
	  -Wno-error=redundant-decls \
	  -Wno-error=strict-prototypes \
	  -Wno-error=undef

EXTRA_INCS += \
	$(PLATFORM_SDK_DIR)/components/mbedtls/mbedtls/include \
	$(PLATFORM_SDK_DIR)/components/esp_netif/include \
	$(PLATFORM_SDK_DIR)/components/esp_eth/include \
	$(PLATFORM_SDK_DIR)/components/esp_wifi/include \
	$(PLATFORM_SDK_DIR)/components/nghttp/port/include \
	$(PLATFORM_SDK_DIR)/components/soc/include \
	$(PLATFORM_SDK_DIR)/components/soc/esp32/include \
	$(PLATFORM_SDK_DIR)/components/hal/include \
	$(PLATFORM_SDK_DIR)/components/hal/esp32/include \
	$(PLATFORM_SDK_DIR)/components/esp_hw_support/include \
	$(PLATFORM_SDK_DIR)/components/newlib/platform_include \
	$(PLATFORM_SDK_DIR)/components/esp_system/include \
	$(PLATFORM_SDK_DIR)/components/esp_timer/include \
	$(PLATFORM_SDK_DIR)/components/esp_rom/include \
	$(PLATFORM_SDK_DIR)/components/xtensa/esp32/include \
	$(PLATFORM_SDK_DIR)/components/xtensa/include \
	$(PLATFORM_SDK_DIR)/components/mdns/include \
	$(PLATFORM_SDK_DIR)/components/esp_http_server/include \
	$(PLATFORM_SDK_DIR)/components/mqtt/esp-mqtt/include \
	$(PLATFORM_SDK_DIR)/components/esp_common/include \
	$(PLATFORM_SDK_DIR)/components/esp_event/include \
	$(PLATFORM_SDK_DIR)/components/freertos/include \
	$(PLATFORM_SDK_DIR)/components/freertos/port/xtensa/include \
	$(PLATFORM_SDK_DIR)/components/heap/include \
	$(PLATFORM_SDK_DIR)/components/tcpip_adapter/include \
	$(PLATFORM_SDK_DIR)/components/lwip/lwip/src/include \
	$(PLATFORM_SDK_DIR)/components/lwip/port/esp32/include \
	$(PLATFORM_SDK_DIR)/components/lwip/include/apps/sntp \
	$(PLATFORM_SDK_DIR)/components/lwip/include/apps \
	$(PLATFORM_SDK_DIR)/components/vfs/include \
	$(PLATFORM_SDK_DIR)/components/app_update/include \
	$(PLATFORM_SDK_DIR)/components/spi_flash/include \
	$(PLATFORM_SDK_DIR)/components/bootloader_support/include \
	$(PLATFORM_SDK_DIR)/components/nvs_flash/include \
	$(PLATFORM_DIR) \
	$(OUTDIR)/include \
	.
