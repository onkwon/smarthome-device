COMPONENT_NAME = base64

SRC_FILES = \
	../src/base64.c

TEST_SRC_FILES = \
	src/test_base64.cpp

INCLUDE_DIRS += \
	../src \
	../external/libmcu/include

include test_runners/MakefileRunner.mk
