PLATFORM := esp8266
PLATFORM_SDK_DIR := external/ESP8266_RTOS_SDK

SRCDIRS += $(PLATFORM_DIR)/src

CROSS_COMPILE ?= xtensa-lx106-elf
CFLAGS += -mlongcalls -mtext-section-literals
LDFLAGS += -nostartfiles -Wl,--just-symbols=$(OUTDIR)/$(PLATFORM).elf
LD_SCRIPT += $(BOARD_DIR)/app.ld
DEFS += __ESP_FILE__=\"null\" CONFIG_LWIP_IPV6=1

## Compiler errors
CFLAGS += \
	  -Wno-error=cast-qual \
	  -Wno-error=sign-conversion \
	  -Wno-error=redundant-decls \
	  -Wno-error=strict-prototypes \
	  -Wno-error=undef

EXTRA_INCS += \
	$(PLATFORM_SDK_DIR)/components/mdns/include \
	$(PLATFORM_SDK_DIR)/components/http_parser/include \
	$(PLATFORM_SDK_DIR)/components/esp_http_server/include \
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
	$(OUTDIR)/include \
	.

$(OUTDIR)/$(PLATFORM).bin: $(OUTDIR)/$(PLATFORM).elf $(MAKEFILE_LIST) \
				$(BOARD_DIR)/loader.sym \
				$(BOARD_DIR)/sdk.ld
	$(info rewriting   $<)
	$(Q)$(CC) \
		-Wl,--just-symbols=$(BOARD_DIR)/loader.sym \
		-nostdlib -Wl,-static -Wl,--gc-sections \
		-u call_user_start_cpu0 -u esp_app_desc -u vsnprintf \
		-Wl,--start-group \
		-L$(OUTDIR)/esp-tls -lesp-tls  \
		-L$(OUTDIR)/esp8266 -lesp8266 \
		-L$(BASEDIR)/external/ESP8266_RTOS_SDK/components/esp8266/lib \
		-lhal -lcore -lnet80211 -lphy  -lclk -lpp -lespnow \
		-L$(BASEDIR)/external/ESP8266_RTOS_SDK/components/esp8266/ld \
		-T $(BOARD_DIR)/sdk.ld \
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
		-u sntp_init -u sntp_setservername -u sntp_setoperatingmode \
		-u sntp_set_time_sync_notification_cb \
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
		-u spi_flash_erase_range \
		-L$(OUTDIR)/tcp_transport -ltcp_transport \
		-L$(OUTDIR)/tcpip_adapter -ltcpip_adapter \
		-u tcpip_adapter_init -u ip4addr_ntoa \
		-u tcpip_adapter_set_hostname -u tcpip_adapter_get_hostname \
		-L$(OUTDIR)/vfs -lvfs \
		-L$(OUTDIR)/wpa_supplicant -lwpa_supplicant \
		-u esp_wifi_init -u esp_wifi_set_storage -u esp_wifi_set_mode \
		-u esp_wifi_start -u esp_wifi_scan_start -u esp_wifi_set_config \
		-u esp_wifi_sta_get_ap_info -u esp_wifi_connect -u esp_restart \
		-u esp_wifi_scan_get_ap_records -u esp_wifi_stop \
		-L$(OUTDIR)/esp_http_server -lesp_http_server \
		-u httpd_start -u httpd_register_uri_handler \
		-u httpd_resp_send_500 -u httpd_query_key_value \
		-u httpd_req_to_sockfd \
		-L$(OUTDIR)/mdns -lmdns \
		-u mdns_init -u mdns_hostname_set -u mdns_instance_name_set \
		-u mdns_service_add -u mdns_service_txt_item_set \
		-lgcc -lstdc++ -Wl,--end-group -Wl,-EL \
		-o $(OUTDIR)/$(PLATFORM).elf \
		-Wl,-Map=$(OUTDIR)/$(PLATFORM).map \
		-Wl,--print-memory-usage
	$(info generating  $@)
	$(Q)python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp8266 elf2image \
		--version=3 \
		-o $@ $< 1>/dev/null

.PHONY: $(OUTDIR)/$(PLATFORM).elf
$(OUTDIR)/$(PLATFORM).elf: $(BOARD_DIR)/loader.sym
	$(info generating  platform sdk: $@ (this may take a while...))
	$(Q)$(MAKE) -C $(BOARD_DIR)/platform $(MAKEFLAGS) \
		PLATFORM_BUILD_DIR=$(BASEDIR)/$(OUTDIR) \
		EXTRA_LDFLAGS=-Wl,--just-symbols=../loader.sym \
		PLATFORM_PROJECT_NAME=$(PLATFORM) 1> /dev/null
