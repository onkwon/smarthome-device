#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := $(APP_PROJECT_NAME)
ifeq ($(APP_BUILD_DIR),)
	APP_BUILD_DIR := build
endif
BUILD_DIR_BASE = $(APP_BUILD_DIR)

COMPONENT_LINKER_DEPS += $(BUILD_DIR_BASE)/libapp.a

include $(IDF_PATH)/make/project.mk

