SRCDIRS += ports/esp8266/nodemcu/src
PLATFORM := esp8266
PLATFORM_DIR := ports/esp8266/nodemcu/platform
PLATFORM_SDK_DIR := external/ESP8266_RTOS_SDK

CROSS_COMPILE ?= xtensa-lx106-elf
CFLAGS += -mlongcalls -mtext-section-literals
LDFLAGS += -nostartfiles -Wl,--just-symbols=$(OUTDIR)/$(PLATFORM).elf
LD_SCRIPT += $(PLATFORM_DIR)/app.ld
DEFS += __ESP_FILE__=\"null\"

## Compiler errors
CFLAGS += \
	  -Wno-error=cast-qual \
	  -Wno-error=sign-conversion \
	  -Wno-error=redundant-decls \
	  -Wno-error=strict-prototypes \
	  -Wno-error=undef

EXTERNAL_INCS += \
	$(PLATFORM_SDK_DIR)/components/mqtt/esp-mqtt/include \
	$(PLATFORM_SDK_DIR)/components/esp_common/include \
	$(PLATFORM_SDK_DIR)/components/esp_event/include \
	$(PLATFORM_SDK_DIR)/components/freertos/include \
	$(PLATFORM_SDK_DIR)/components/freertos/port/esp8266/include \
	$(PLATFORM_SDK_DIR)/components/freertos/port/esp8266/include/freertos \
	$(PLATFORM_SDK_DIR)/components/freertos/include/freertos/private \
	$(PLATFORM_SDK_DIR)/components/esp8266/include \
	$(PLATFORM_SDK_DIR)/components/heap/include \
	$(PLATFORM_SDK_DIR)/components/heap/port/esp8266/include \
	$(PLATFORM_SDK_DIR)/components/tcpip_adapter/include \
	$(PLATFORM_SDK_DIR)/components/lwip/lwip/src/include \
	$(PLATFORM_SDK_DIR)/components/lwip/port/esp8266/include \
	$(PLATFORM_SDK_DIR)/components/vfs/include \
	$(PLATFORM_SDK_DIR)/components/lwip/include/apps/sntp \
	$(PLATFORM_SDK_DIR)/components/lwip/include/apps \
	$(PLATFORM_SDK_DIR)/components/app_update/include \
	$(PLATFORM_SDK_DIR)/components/spi_flash/include \
	$(PLATFORM_SDK_DIR)/components/bootloader_support/include \
	$(PLATFORM_SDK_DIR)/components/nvs_flash/include \
	$(PLATFORM_DIR) \
	.

all:: prerequisite

.PHONY: prerequisite
prerequisite:: $(OUTDIR)/$(PLATFORM).bin

$(OUTDIR)/$(PLATFORM).bin: $(OUTDIR)/$(PLATFORM).elf $(MAKEFILE_LIST)
	$(info generating  $<)
	$(Q)$(CC) \
		-Wl,--just-symbols=$(PLATFORM_DIR)/app.sym \
		-nostdlib -Wl,-static -Wl,--gc-sections \
		-u call_user_start_cpu0 -u esp_app_desc -u vsnprintf \
		-Wl,--start-group \
		-L$(OUTDIR)/esp-tls -lesp-tls  \
		-L$(OUTDIR)/esp8266 -lesp8266 \
		-L$(BASEDIR)/external/ESP8266_RTOS_SDK/components/esp8266/lib \
		-lhal -lcore -lnet80211 -lphy  -lclk -lpp -lespnow \
		-L$(BASEDIR)/external/ESP8266_RTOS_SDK/components/esp8266/ld \
		-T $(PLATFORM_DIR)/sdk.ld \
		-T $(OUTDIR)/esp8266/esp8266.project.ld \
		-T esp8266.rom.ld \
		-T esp8266.peripherals.ld \
		-Wl,--no-check-sections -u call_user_start -u g_esp_sys_info \
		-L$(OUTDIR)/esp_common -lesp_common \
		-u esp_reset_reason -u esp_get_free_heap_size \
		-L$(OUTDIR)/esp_event -lesp_event  \
		-u esp_event_loop_create_default -u esp_event_send \
		-L$(OUTDIR)/esp_ringbuf -lesp_ringbuf  \
		-L$(OUTDIR)/freertos -lfreertos \
		-u vTaskDelay -u vTaskList -u uxTaskGetNumberOfTasks \
		-L$(OUTDIR)/heap -lheap \
		-L$(OUTDIR)/http_parser -lhttp_parser \
		-L$(OUTDIR)/log -llog \
		-L$(OUTDIR)/lwip -llwip \
		-L$(OUTDIR)/mbedtls -lmbedtls  \
		-L$(OUTDIR)/mqtt -lmqtt \
		-u esp_mqtt_client_subscribe \
		-u esp_mqtt_client_unsubscribe \
		-u esp_mqtt_client_publish \
		-u esp_mqtt_client_init \
		-u esp_mqtt_client_register_event \
		-u esp_mqtt_client_destroy \
		-u esp_mqtt_client_start \
		-L$(OUTDIR)/newlib -lnewlib \
		-lc_nano -u __errno \
		-L$(OUTDIR)/nvs_flash -lnvs_flash \
		-L$(OUTDIR)/pthread -lpthread \
		-u pthread_include_pthread_impl \
		-u pthread_include_pthread_cond_impl \
		-u pthread_include_pthread_local_storage_impl \
		-u pthread_attr_init -u pthread_exit -u pthread_create \
		-u pthread_attr_setstacksize -u pthread_attr_setdetachstate \
		-L$(OUTDIR)/spi_flash -lspi_flash \
		-L$(OUTDIR)/tcp_transport -ltcp_transport \
		-L$(OUTDIR)/tcpip_adapter -ltcpip_adapter \
		-u tcpip_adapter_init \
		-u ip4addr_ntoa \
		-L$(OUTDIR)/vfs -lvfs \
		-L$(OUTDIR)/wpa_supplicant -lwpa_supplicant \
		-u esp_wifi_init -u esp_wifi_set_storage -u esp_wifi_set_mode \
		-u esp_wifi_start -u esp_wifi_scan_start -u esp_wifi_set_config \
		-u esp_wifi_sta_get_ap_info -u esp_wifi_connect \
		-u esp_wifi_scan_get_ap_records \
		-lgcc -lstdc++ -Wl,--end-group -Wl,-EL \
		-o $(OUTDIR)/$(PLATFORM).elf \
		-Wl,-Map=$(OUTDIR)/$(PLATFORM).map \
		-Wl,--print-memory-usage
	$(info generating  $@)
	$(Q)python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp8266 elf2image \
		--version=3 \
		-o $@ $<

$(OUTDIR)/$(PLATFORM).elf:
	$(info generating  $@ (this may take a while...))
	$(Q)$(MAKE) -C $(PLATFORM_DIR) $(MAKEFLAGS) \
		PLATFORM_BUILD_DIR=$(BASEDIR)/$(OUTDIR) \
		EXTRA_LDFLAGS=-Wl,--just-symbols=app.sym \
		PLATFORM_PROJECT_NAME=$(PLATFORM) 1> /dev/null

.PHONY: flash
flash: all
	python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp8266 \
		--port /dev/tty.SLAB_USBtoUART \
		--baud 921600 \
		--before default_reset \
		--after hard_reset write_flash -z \
		--flash_mode dio \
		--flash_freq 40m \
		--flash_size 4MB \
		0xe000 $(OUTDIR)/ota_data_initial.bin \
		0x0 $(OUTDIR)/bootloader/bootloader.bin \
		0x10000 $(OUTDIR)/esp8266.bin \
		0x8000 $(OUTDIR)/partitions.bin \
		0xA6010 $(OUTDIR)/nodemcu.bin
.PHONY: erase_flash
erase_flash:
	python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp8266 \
		--port /dev/tty.SLAB_USBtoUART \
		--baud 921600 \
		--before default_reset \
		$@
.PHONY: monitor
monitor:
	$(PLATFORM_SDK_DIR)/tools/idf_monitor.py $(OUTELF) \
		--port /dev/tty.SLAB_USBtoUART
	#python -m serial.tools.miniterm /dev/tty.SLAB_USBtoUART 115200
