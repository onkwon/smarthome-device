#!/bin/sh

cwd=$(pwd)

export IDF_PATH=$cwd/external/ESP8266_RTOS_SDK
export PATH=$cwd/tools/xtensa-lx106-elf/bin:$PATH
