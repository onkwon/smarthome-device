PLATFORM_DIR := ports/esp8266
BOARD_DIR := $(PLATFORM_DIR)/tywe3s
PREREQUISITES += $(OUTDIR)/$(PLATFORM).elf
EXTRA_SRCS += $(wildcard $(BOARD_DIR)/src/*.c)
DEFS += BUTTON_MAX=2

OUTPUT := \
	$(OUTBIN) \
	$(OUTELF).sym \
	$(OUTELF).dump \
	$(OUTELF).lst \
	$(OUTELF).size \
	$(OUTDEF) \
	$(OUTSRC) \
	$(OUTINC)

APPLOADER := $(OUTDIR)/apploader
APPLOADER_SRCS := \
	$(PLATFORM_DIR)/src/dfu_flash.c \
	$(PLATFORM_DIR)/src/sha256.c \
	components/dfu/dfu.c \
	components/apploader/main.c
APPLOADER_OBJS := $(addprefix $(APPLOADER)/, $(APPLOADER_SRCS:.c=.o))
DEPS += $(APPLOADER_OBJS:.o=.d)

include ports/esp8266/esp8266.mk

all:: $(OUTELF).img

$(OUTELF).img: $(OUTBIN)
	$(info generating  $@)
	$(Q)printf "C0DEFEED" \
		| tools/endian.sh \
		| xxd -r -p > $@
	$(Q)wc -c < $< \
		| awk '{printf("%08x", $$1)}' \
		| tools/endian.sh \
		| xxd -r -p >> $@
	$(Q)openssl dgst -sha256 $< \
		| awk '{print $$2}' \
		| xxd -r -p >> $@
	$(Q)cat $< >> $@

$(OUTELF):: $(OUTDIR)/$(PLATFORM).bin $(APPLOADER).bin

$(APPLOADER).bin: $(APPLOADER).elf
	$(info generating  $@)
	$(Q)$(SZ) $<
	$(Q)$(OC) -O binary $< $@
$(APPLOADER).elf: $(APPLOADER_OBJS) \
				$(OUTDIR)/$(PLATFORM).bin \
				$(BOARD_DIR)/loader.ld \
				$(BOARD_DIR)/app.sym
	$(info linking     $@)
	$(Q)$(SZ) -t --common $(sort $(APPLOADER_OBJS))
	$(Q)$(CC) -o $@ $(filter %.o,$^) \
		-Wl,-Map,$(APPLOADER).map \
		-Wl,--just-symbols=$(BOARD_DIR)/app.sym \
		-Wl,--just-symbols=$(OUTDIR)/$(PLATFORM).elf \
		-T $(BOARD_DIR)/loader.ld \
		$(LDFLAGS)
$(APPLOADER_OBJS): $(APPLOADER)/%.o: %.c | $(PREREQUISITES)
	$(info compiling   $<)
	@mkdir -p $(@D)
	$(Q)$(CC) -o $@ -c $*.c -MMD \
		$(addprefix -D, $(DEFS)) \
		$(addprefix -I, $(INCS)) \
		$(CFLAGS)

.PHONY: flash
flash: all
	python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp8266 \
		--port $(PORT) \
		--baud 921600 \
		--before "default_reset" \
		--after "hard_reset" \
		write_flash -z \
		--flash_mode "dout" \
		--flash_freq "40m" \
		--flash_size "1MB" \
		0xe000 $(OUTDIR)/ota_data_initial.bin \
		0x0 $(OUTDIR)/bootloader/bootloader.bin \
		0x10000 $(OUTDIR)/esp8266.bin \
		0x8000 $(OUTDIR)/partitions.bin \
		0xBE000 $(OUTDIR)/apploader.bin \
		0xC0000 $(OUTDIR)/$(PROJECT).bin
.PHONY: erase_flash
erase_flash:
	python $(PLATFORM_SDK_DIR)/components/esptool_py/esptool/esptool.py \
		--chip esp8266 \
		--port $(PORT) \
		--baud 921600 \
		--before default_reset \
		$@
.PHONY: monitor
monitor:
	$(PLATFORM_SDK_DIR)/tools/idf_monitor.py $(OUTELF) \
		--port $(PORT)
	#python -m serial.tools.miniterm $(PORT) 115200
