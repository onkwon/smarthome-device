PROJECT := esp8266
VERSION := $(shell git describe --long --tags --dirty --always)
BASEDIR := $(shell pwd)
BUILDIR := build
SRCDIRS := src

V ?= 0
ifeq ($(V), 0)
	Q = @
else
	Q =
endif

PLATFORM = ports/esp8266/nodemcu
APP_PARTITION_SIZE = 1048576
APP_INCLUDES = include
APP_DEFINES  = \
	_POSIX_THREADS \
	VERSION=$(VERSION)

EXTRA_SRCS = \
	external/libmcu/src/logging.c \
	external/libmcu/examples/logging/fake_storage.c
EXTRA_INCS = \
	external/libmcu/include \
	external/libmcu/examples

# Toolchains
ifneq ($(CROSS_COMPILE),)
	CROSS_COMPILE_PREFIX := $(CROSS_COMPILE)-
endif
CC := $(CROSS_COMPILE_PREFIX)gcc
LD := $(CROSS_COMPILE_PREFIX)ld
SZ := $(CROSS_COMPILE_PREFIX)size
AR := $(CROSS_COMPILE_PREFIX)ar
OC := $(CROSS_COMPILE_PREFIX)objcopy
OD := $(CROSS_COMPILE_PREFIX)objdump

# Compiler options
CFLAGS += \
	  -std=gnu99 \
	  -static \
	  -nostdlib \
	  -fno-builtin \
	  -fno-common \
	  -ffunction-sections \
	  -fdata-sections \
	  -Os \
	  \
	  -mlongcalls

## Compiler warnings
CFLAGS += \
	  -Wall \
	  -Wextra \
	  -Wformat=2 \
	  -Wmissing-prototypes \
	  -Wstrict-prototypes \
	  -Wmissing-declarations \
	  -Wcast-align \
	  -Wpointer-arith \
	  -Wbad-function-cast \
	  -Wnested-externs \
	  -Wcast-qual \
	  -Wmissing-format-attribute \
	  -Wmissing-include-dirs \
	  -Wformat-nonliteral \
	  -Wdouble-promotion \
	  -Wfloat-equal \
	  -Winline \
	  -Wundef \
	  -Wshadow \
	  -Wwrite-strings \
	  -Waggregate-return \
	  -Wredundant-decls \
	  -Wconversion \
	  -Wstrict-overflow=5 \
	  -Werror

## Compiler errors
CFLAGS += -Wno-error=format-nonliteral

ifndef NDEBUG
	CFLAGS += -g3
	#CFLAGS += -fsanitize=address
endif

# Linker options
LDFLAGS += \
	   --gc-sections \
	   --print-gc-sections
LIBS =
LD_SCRIPT =
ifneq ($(LD_SCRIPT),)
	LDFLAGS += -T$(LD_SCRIPT)
endif
ifdef LD_LIBRARY_PATH
	LDFLAGS += -L$(LD_LIBRARY_PATH) -lc
endif

ARFLAGS = crsu

# Build options
SRCS += $(foreach dir, $(SRCDIRS), $(shell find $(dir) -type f -regex ".*\.c")) \
	$(EXTRA_SRCS)
INCS += $(addprefix -I, $(APP_INCLUDES)) \
	$(addprefix -I, $(EXTRA_INCS))
DEFS += $(addprefix -D, $(APP_DEFINES))
OBJS += $(addprefix $(BUILDIR)/, $(SRCS:.c=.o))
DEPS += $(OBJS:.o=.d)

.DEFAULT_GOAL :=
all: $(PLATFORM)
	@echo "\n  $(PROJECT)_$(VERSION)\n"
	@awk -P '/^dram/ || /^.dram/ {printf("%10d", $$3)}' \
		$(BUILDIR)/$(PROJECT).map \
		| LC_ALL=en_US.UTF-8 \
		awk '{printf("  ram\t %\04710d / %\047-10d (%.2f%%)\n", \
		$$2 + $$3, $$1, ($$2+$$3)/$$1*100)}'
	@awk -P '/^iram0_2/ || /^.iram/ {printf("%10d", $$3)}' \
		$(BUILDIR)/$(PROJECT).map \
		| LC_ALL=en_US.UTF-8 \
		awk '{printf("  flash\t %\04710d / %\047-10d (%.2f%%)\n", \
		$$2 + $$3, $(APP_PARTITION_SIZE), \
		($$2+$$3)/$(APP_PARTITION_SIZE)*100)}'

$(PLATFORM): $(BUILDIR)/libapp.a
	@echo "  BUILD    $@"
	$(Q)$(MAKE) -C $(PLATFORM) $(MAKEFLAGS) \
		APP_BUILD_DIR=$(BASEDIR)/$(BUILDIR) \
		APP_PROJECT_NAME=$(PROJECT) 1> /dev/null
$(BUILDIR)/libapp.a: $(OBJS)
	@echo "  AR       $@"
	$(Q)$(AR) $(ARFLAGS) $@ $^ 1> /dev/null 2>&1
$(OBJS): $(BUILDIR)/%.o: %.c Makefile $(LD_SCRIPT)
	@echo "  CC       $*.c"
	@mkdir -p $(@D)
	$(Q)$(CC) -o $@ -c $*.c -MMD $(DEFS) $(INCS) $(CFLAGS)

ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), depend)
-include $(DEPS)
endif
endif

.PHONY: test
test:
	$(Q)$(MAKE) -C tests
.PHONY: coverage
coverage:
	$(Q)$(MAKE) -C tests $@
.PHONY: clean
clean:
	#$(Q)$(MAKE) -C tests clean
	$(Q)rm -rf $(BUILDIR)
.PHONY: flash
flash:
	#$(Q)$(MAKE) -C $(PLATFORM) $@
	python external/ESP8266_RTOS_SDK/components/esptool_py/esptool/esptool.py \
		--chip esp8266 \
		--port /dev/tty.SLAB_USBtoUART \
		--baud 921600 \
		--before default_reset \
		--after hard_reset write_flash -z \
		--flash_mode dio \
		--flash_freq 40m \
		--flash_size 4MB 0xe000 \
		build/ota_data_initial.bin 0x0 \
		build/bootloader/bootloader.bin 0x10000 \
		build/esp8266.bin 0x8000 \
		build/partitions.bin
.PHONY: erase_flash
erase_flash:
	python external/ESP8266_RTOS_SDK/components/esptool_py/esptool/esptool.py \
		--chip esp8266 \
		--port /dev/tty.SLAB_USBtoUART \
		--baud 921600 \
		--before default_reset \
		$@
.PHONY: monitor
monitor:
	python -m serial.tools.miniterm /dev/tty.SLAB_USBtoUART 115200
	#minicom -D /dev/tty.SLAB_USBtoUART
