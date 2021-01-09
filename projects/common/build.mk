goal := $(firstword $(MAKECMDGOALS))

ifneq ($(goal),)
ifneq ($(strip $(wildcard projects/$(goal).mk)),)
$(MAKECMDGOALS):
	$(Q)PROJECT=$(goal) $(MAKE) -f projects/common/build.mk \
		$(filter-out $(goal), $(MAKECMDGOALS))
done := 1
endif
endif

ifeq ($(done),)

ifeq ($(PROJECT),)
DEFAULT_PROJECT ?= nodemcu
PROJECT := $(DEFAULT_PROJECT)
endif

OUTDIR := $(BUILDIR)/$(PROJECT)
OUTELF := $(OUTDIR)/$(PROJECT)
OUTBIN := $(OUTDIR)/$(PROJECT).bin

-include projects/$(PROJECT).mk
include projects/common/toolchain.mk

SRCDIRS += src
EXTERNAL_SRCS += \
	external/libmcu/examples/memory_storage.c \
	external/libmcu/src/ringbuf.c \
	external/libmcu/src/retry.c \
	external/libmcu/src/jobqueue.c \
	external/libmcu/src/logging.c
EXTERNAL_INCS += \
	external/libmcu/examples \
	external/libmcu/include \
	external/libmcu/include/libmcu/posix \
	external

SRCS += $(foreach dir, $(SRCDIRS), $(shell find $(dir) -type f -regex ".*\.c")) \
	$(EXTERNAL_SRCS)
OBJS += $(addprefix $(OUTDIR)/, $(SRCS:.c=.o))
DEPS += $(OBJS:.o=.d)
INCS += \
	include \
	$(EXTERNAL_INCS)
DEFS += \
	_POSIX_THREADS \
	VERSION_TAG=$(VERSION_TAG) \
	VERSION=$(VERSION)
LIBS += -lc_nano

.DEFAULT_GOAL :=
all:: $(OUTBIN) $(OUTELF).sym $(OUTELF).dump $(OUTELF).lst $(OUTELF).size \
	$(OUTDIR)/sources.txt $(OUTDIR)/includes.txt
	$(info done $(VERSION))
	@awk -P '/^ram/||/^ram_noinit/||/^.data/||/^.bss/ {printf("%10d", $$3)}' \
		$(OUTELF).map | LC_ALL=en_US.UTF-8 \
		awk '{printf("RAM %\04710d / %\04710d (%.2f%%)\n", \
		$$3 + $$4, $$1 + $$2, ($$3+$$4)/($$1+$$2)*100)}'
	@awk -P '/^rom/||/^.text/ {printf("%10d", $$3)}' \
		$(OUTELF).map | LC_ALL=en_US.UTF-8 \
		awk '{printf("ROM %\04710d / %\04710d (%.2f%%)\n", \
		$$2, $$1, $$2/$$1*100)}'

$(OUTDIR)/sources.txt: $(OUTELF)
	$(info generating  $@)
	$(Q)echo $(sort $(SRCS)) | tr ' ' '\n' > $@
$(OUTDIR)/includes.txt: $(OUTELF)
	$(info generating  $@)
	$(Q)echo $(subst -I,,$(sort $(INCS))) | tr ' ' '\n' > $@

$(OUTELF).size: $(OUTELF)
	$(info generating  $@)
	$(Q)$(NM) -S --size-sort $< > $@
$(OUTELF).dump: $(OUTELF)
	$(info generating  $@)
	$(Q)$(OD) -x $< > $@
$(OUTELF).lst: $(OUTELF)
	$(info generating  $@)
	$(Q)$(OD) -d $< > $@
$(OUTELF).sym: $(OUTELF)
	$(info generating  $@)
	$(Q)$(OD) -t $< | sort > $@
$(OUTELF).hex: $(OUTELF)
	$(info generating  $@)
	$(Q)$(OC) -O ihex $< $@
$(OUTBIN): $(OUTELF)
	$(info generating  $@)
	$(Q)$(SZ) $<
	$(Q)$(OC) -O binary $< $@

$(OUTELF):: $(OBJS)
	$(info linking     $@)
	$(Q)$(SZ) -t --common $(sort $(OBJS))
	$(Q)$(CC) -o $@ -Wl,-Map,$(OUTELF).map $^ \
		$(addprefix -T, $(LD_SCRIPT)) \
		$(CFLAGS) $(LDFLAGS) \
		$(LIBS)

$(OBJS): $(OUTDIR)/%.o: %.c Makefile $(MAKEFILE_LIST) $(LD_SCRIPT)
	$(info compiling   $<)
	@mkdir -p $(@D)
	$(Q)$(CC) -o $@ -c $*.c -MMD \
		$(addprefix -D, $(DEFS)) \
		$(addprefix -I, $(INCS)) \
		$(CFLAGS)

ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), depend)
-include $(DEPS)
endif
endif

.PHONY: clean
clean:
	$(Q)rm -fr $(OUTDIR)

endif # $(done)
