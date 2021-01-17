goal := $(firstword $(MAKECMDGOALS))

ifneq ($(goal),)
ifneq ($(strip $(wildcard projects/$(goal).mk)),)
$(MAKECMDGOALS): build-goal
	@:
.PHONY: build-goal
build-goal:
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
OUTLIB := $(OUTDIR)/lib$(PROJECT).a

-include projects/$(PROJECT).mk
include projects/common/toolchain.mk

COMPONENTS_DIRS += components/httpsrv
COMPONENTS_INCS += components/httpsrv/include

SRCDIRS += src $(COMPONENTS_DIRS)

EXTRA_SRCS += \
	external/libmcu/src/pubsub.c \
	external/libmcu/examples/memory_storage.c \
	external/libmcu/src/ringbuf.c \
	external/libmcu/src/retry.c \
	external/libmcu/src/jobqueue.c \
	external/libmcu/src/logging.c
EXTRA_INCS += \
	external/libmcu/examples \
	external/libmcu/include \
	external/libmcu/include/libmcu/posix \
	external

SRCS += $(foreach dir, $(SRCDIRS), $(shell find $(dir) -type f -regex ".*\.c")) \
	$(EXTRA_SRCS)
OBJS += $(addprefix $(OUTDIR)/, $(SRCS:.c=.o))
DEPS += $(OBJS:.o=.d)
INCS += \
	$(EXTRA_INCS) \
	$(COMPONENTS_INCS) \
	include
DEFS += \
	_POSIX_THREADS \
	VERSION_TAG=$(VERSION_TAG) \
	VERSION=$(VERSION)
LIBS +=

.DEFAULT_GOAL :=
all:: $(OUTPUT)
	$(info done $(VERSION))
	@#@awk -P '/^ram/||/^ram_noinit/||/^.data/||/^.bss/ {printf("%10d", $$3)}' \
		$(OUTELF).map | LC_ALL=en_US.UTF-8 \
		awk '{printf("RAM %\04710d / %\04710d (%.2f%%)\n", \
		$$3 + $$4, $$1 + $$2, ($$3+$$4)/($$1+$$2)*100)}'
	@#@awk -P '/^rom/||/^.text/ {printf("%10d", $$3)}' \
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

$(OUTELF):: $(OBJS) $(LD_SCRIPT)
	$(info linking     $@)
	$(Q)$(SZ) -t --common $(sort $(OBJS))
	$(Q)$(CC) -o $@ $(OBJS) \
		-Wl,-Map,$(OUTELF).map \
		$(addprefix -T, $(LD_SCRIPT)) \
		$(LDFLAGS) \
		$(LIBS)

$(OUTLIB): $(OBJS)
	$(info archiving   $@)
	$(Q)$(AR) $(ARFLAGS) $@ $^ 1> /dev/null 2>&1

$(OBJS): $(OUTDIR)/%.o: %.c Makefile $(MAKEFILE_LIST) | $(PREREQUISITES)
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
