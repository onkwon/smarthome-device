PLATFORM_DIR := ports/esp32
BOARD_DIR := $(PLATFORM_DIR)/devkit
PREREQUISITES += $(OUTDIR)/include/sdkconfig.h

OUTPUT := $(OUTDIR)/$(PROJECT)_final

include ports/esp32/esp32.mk

$(OUTPUT): $(OUTLIB)
	$(info linking     $@)
	$(Q)$(MAKE) -C $(BOARD_DIR)/platform $(MAKEFLAGS) \
		PLATFORM_BUILD_DIR=$(BASEDIR)/$(OUTDIR) \
		PLATFORM_PROJECT_NAME=$(notdir $@) 1> /dev/null
$(OUTDIR)/include/sdkconfig.h:
	$(info generating  $@ (this may take a while...))
	-$(Q)$(MAKE) -C $(BOARD_DIR)/platform $(MAKEFLAGS) \
		PLATFORM_BUILD_DIR=$(BASEDIR)/$(OUTDIR) \
		PLATFORM_PROJECT_NAME=$(notdir $@) 1> /dev/null

.PHONY: flash
flash: all
	python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp32 \
		--port $(PORT) \
		--baud 921600 \
		--before default_reset \
		--after hard_reset write_flash -z \
		--flash_mode dio \
		--flash_freq 40m \
		--flash_size detect \
		0x1000 $(OUTDIR)/bootloader/bootloader.bin \
		0x8000 $(OUTDIR)/partitions.bin \
		0xd000 $(OUTDIR)/ota_data_initial.bin \
		0x10000 $(OUTPUT).bin
.PHONY: erase_flash
erase_flash:
	python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp32 \
		--port $(PORT) \
		--baud 921600 \
		--before default_reset \
		$@
.PHONY: monitor
monitor:
	$(PLATFORM_SDK_DIR)/tools/idf_monitor.py $(OUTPUT).elf \
		--port $(PORT)
