COMPONENT_NAME = jsmnn

SRC_FILES = \
	../src/jsmnn.c \
	stubs/logging.c

TEST_SRC_FILES = \
	src/test_jsmnn.cpp

INCLUDE_DIRS += \
	../external/libmcu/include

include test_runners/MakefileRunner.mk
