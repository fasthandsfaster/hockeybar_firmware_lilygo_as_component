// HockeyBar Firmware (HBF) code - main module
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "hbf_wifi.h"
#include "seven_seg.h"
#include "hbf_led_strip.h"
#include "websocket.h"
#include "trigger.h"

#include "hbf_test.h"
#include "isr_handlers.h"
#include "Lilygo-Display-IDF.h"

//tmp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//tmp

static const char *TAG = "HBF_MAIN";

void app_main(void) {
    //seven_seg_init();
    //hbf_led_strip_init();
    hbf_wifi_init();
    trigger_init();
    websocket_init(hbf_ws_connection_handler, hbf_ws_data_handler);
    //breakbeam_init(); // Must be called before push
    //push_init();
    isr_loop_init();
    init_lilygo_display();

    //vTaskDelay(15000 / portTICK_PERIOD_MS;);

    //get_timestamp_str();

    ESP_LOGI(TAG, "**********************************************************************************");
    ESP_LOGI(TAG, "* HockeyBar Firmware Started");
    ESP_LOGI(TAG, "**********************************************************************************");
}
