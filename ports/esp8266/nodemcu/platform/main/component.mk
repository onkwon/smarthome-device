COMPONENT_SRCDIRS +=
COMPONENT_ADD_LDFLAGS += -Wl,--whole-archive \
			 -L../../../../build/nodemcu -lnodemcu \
			 -Wl,--no-whole-archive
COMPONENT_ADD_LINKER_DEPS += ../../../../../build/nodemcu/libnodemcu.a
COMPONENT_PRIV_INCLUDEDIRS +=
COMPONENT_EMBED_TXTFILES :=
