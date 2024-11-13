# HockeyBar Firmware

## Tweek menuconfig
*Notice: sdkconfig is reset when the target is changed !!!*

### To avoid crash error: 
````
A stack overflow in task Tmr Svc has been detected.
````
Increase [FREERTOS_TIMER_TASK_STACK_DEPTH] to 4096

Source: https://github.com/espressif/esp-idf/issues/5980

### To remove warning about memory:
````
Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header
or
Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
````
set Flash Size [ESPTOOLPY_FLASHSIZE] to 8 MB

### To enable DEBUG everywhere - NOT USED
````
// CONFIG_LOG_MAXIMUM_LEVEL=4 (DEBUG)
````

## Migration to idf 5.0

### Add websocket component
The websocket component is no longer a part of idf. It must be installed separately.
````
>idf.py add-dependency espressif/esp_websocket_client

Executing action: add-dependency
Successfully added dependency "espressif/esp_websocket_client*" for component "main"

>idf.py reconfigure

````

## Fix for smartconfig error - *NOT NEEDED FOR IDF V >= 5.0*
Error:
````
setsockopt failed
````
Patch in the esp-idf-v4.4\components\esp_wifi\src\smartconfig_ack.c
````
    /* if (setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int)) < 0) { */
    /*     ESP_LOGE(TAG,  "setsockopt failed"); */
    /*     goto _end; */
    /* } */

    /* FIX BEGIN*/
    if (setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(int)) < 0) {
        ESP_LOGE(TAG,  "setsockopt SO_BROADCAST failed");
        goto _end;
    }
    if (setsockopt(send_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
        ESP_LOGE(TAG,  "setsockopt SO_REUSEADDR failed");
        goto _end;
    }
    /* FIX END*/

````

## Old stuff
### Provisioning:
e:\udv\esp32\esp-idf-v4.4\examples\provisioning\wifi_prov_mgr

### Getting started
Follow video: ESP32 - How to create your First ESP IDF project (From Scratch): https://www.youtube.com/watch?v=oHHOCdmLiII

### Create project
idf.py create-project -p . hockeybar_firmware
