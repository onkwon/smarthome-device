COMPONENT_NAME = ota

SRC_FILES = \
	../src/ota.c \
	../src/ota_json.c \
	../src/jsmnn.c \
	stubs/logging.c \
	stubs/jobpool.c

TEST_SRC_FILES = \
	src/test_ota.cpp

INCLUDE_DIRS += \
	../src \
	../external/libmcu/include \
	../external/libmcu/include/libmcu/posix

CPPUTEST_CPPFLAGS += \
	-DVERSION_TAG=v1.2.3

include test_runners/MakefileRunner.mk
