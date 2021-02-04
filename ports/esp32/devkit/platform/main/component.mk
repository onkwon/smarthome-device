COMPONENT_SRCDIRS +=
COMPONENT_ADD_LDFLAGS += -Wl,--whole-archive \
			 -L../../../../build/esp32-devkit -lesp32-devkit \
			 -Wl,--no-whole-archive
COMPONENT_ADD_LINKER_DEPS += ../../../../../build/esp32-devkit/libesp32-devkit.a
COMPONENT_PRIV_INCLUDEDIRS +=
COMPONENT_EMBED_TXTFILES :=
