## Building the project

```
$ git clone https://github.com/onkwon/smarthome-device.git
$ cd smarthome-device
$ IDF_TOOLS_PATH=tools/espressif ./external/esp-idf/install.sh 
$ . ./ports/esp32/export.sh
$ make esp32-devkit -j5
$ make esp32-devkit flash
$ make esp32-devkit monitor
```
