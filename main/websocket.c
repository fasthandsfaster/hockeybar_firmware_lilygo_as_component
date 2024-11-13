#include <stdio.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

// Define the higest level possible in this module. Use esp_log_level_set to set the actual level
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "esp_websocket_client.h"
#include "esp_event.h"

#include "hbf_constants.h"
#include "hbf_wifi.h"
#include "websocket.h"

static const char *TAG = "HBF_WSOC";

typedef struct HBF_WS_EVENT_HANDLERS {
    connection_handler_func_t *connection_handler_func;
    data_handler_func_t *data_handler_func;
} hbf_ws_event_handlers_t;

static char websocket_server_uri[128];
static esp_websocket_client_handle_t websocket_client_handle;
static QueueHandle_t reply_evt_queue = NULL;

// FROM rfc6455 - IANA has added initial values to the registry as follows.
//
//  |Opcode  | Meaning                             | Reference |
// -+--------+-------------------------------------+-----------|
//  | 0      | Continuation Frame                  | RFC 6455  |
// -+--------+-------------------------------------+-----------|
//  | 1      | Text Frame                          | RFC 6455  |
// -+--------+-------------------------------------+-----------|
//  | 2      | Binary Frame                        | RFC 6455  |
// -+--------+-------------------------------------+-----------|
//  | 8      | Connection Close Frame              | RFC 6455  |
// -+--------+-------------------------------------+-----------|
//  | 9      | Ping Frame                          | RFC 6455  |
// -+--------+-------------------------------------+-----------|
//  | 10     | Pong Frame                          | RFC 6455  |
// -+--------+-------------------------------------+-----------|

char* get_frame_name(int op_code) {
    return 
        op_code == 0 ? "Continuation Frame" :
        op_code == 1 ? "Text Frame" :
        op_code == 2 ? "Binary Frame" :
        op_code == 8 ? "Connection Close Frame" :
        op_code == 9 ? "Ping Frame" :
        op_code == 10 ? "Pong Frame" :
        "Unknown";
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    hbf_ws_event_handlers_t *hbf_ws_event_handlers = (hbf_ws_event_handlers_t*)handler_args;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_CONNECTED");
        hbf_ws_event_handlers->connection_handler_func(event_id);
        break;
    
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        hbf_ws_event_handlers->connection_handler_func(event_id);
        break;
    
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_DATA, opcode: %d [%s], total payload length: %d, data_len: %d, current payload offset: %d", data->op_code, get_frame_name(data->op_code), data->payload_len, data->data_len, data->payload_offset);

        switch (data->op_code) {          
        case 1:
            char* data_str = data->data_ptr; 
            
            ESP_LOGD(TAG, "Recieved %s", data_str);
         
            hbf_ws_event_handlers->data_handler_func(data->data_ptr, data->data_len);
            break;

        case 8:
            if (data->data_len == 2) {
                ESP_LOGD(TAG, "Received close message with code=%d", 256*data->data_ptr[0] + data->data_ptr[1]);
                websocket_connect(hbf_ws_event_handlers->connection_handler_func, hbf_ws_event_handlers->data_handler_func);
                //return_connected_result("00000000-0000-0000-0000-000000000000");
            }
            break;
        }        
        break;
        
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

void websocket_send_text_async(char* data) {
    size_t len = strlen(data);
    if (len > WEBSOCKET_REPLY_MAX_SIZE) {
        ESP_LOGE(TAG, "*** data size (%d) is larger than WEBSOCKET_REPLY_MAX_SIZE (%d)", len, WEBSOCKET_REPLY_MAX_SIZE);
    }
    else {
        if (!xQueueSendToBack(reply_evt_queue, data, portMAX_DELAY)) {
            ESP_LOGE(TAG, "*** reply_evt_queue is full");
        }
    }
}

static void reply_loop(void* arg) {
    ESP_LOGI(TAG, "Starting websocket reply loop");
    while(true) {
        char data[WEBSOCKET_REPLY_MAX_SIZE];
        if (xQueueReceive(reply_evt_queue, &data, portMAX_DELAY)) {
            if (esp_websocket_client_is_connected(websocket_client_handle)) {
                int len = strlen(data);
                ESP_LOGD(TAG, "Sending %s", data);
                esp_websocket_client_send_text(websocket_client_handle, data, len, portMAX_DELAY);
            }
            else {
                ESP_LOGE(TAG, "Not connected to server");
            }
        }
    }
}

static hbf_ws_event_handlers_t hbf_ws_event_handlers;

void websocket_connect(connection_handler_func_t *connection_handler, data_handler_func_t *data_handler) {
    
    esp_websocket_client_config_t websocket_cfg = {
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 10000,
    };
    /*sprintf(websocket_server_uri, "ws://%s/hockeybar", hbf_wifi_get_ws_host_and_port());    */
    sprintf(websocket_server_uri,"ws://192.168.8.123:8080/ws");
    websocket_cfg.uri = websocket_server_uri;

    ESP_LOGI(TAG, "Connecting to %s", websocket_cfg.uri);
    websocket_client_handle = esp_websocket_client_init(&websocket_cfg);

    hbf_ws_event_handlers.connection_handler_func = connection_handler;
    hbf_ws_event_handlers.data_handler_func = data_handler;

    esp_websocket_register_events(websocket_client_handle, WEBSOCKET_EVENT_ANY, websocket_event_handler, &hbf_ws_event_handlers);

    while (!esp_websocket_client_is_connected(websocket_client_handle)) {
        ESP_LOGD(TAG, "Waiting for connection...");

        esp_websocket_client_start(websocket_client_handle);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Connected to %s", websocket_cfg.uri);
    // Create a queue to handle async replies
    //reply_evt_queue = xQueueCreate(10, WEBSOCKET_REPLY_MAX_SIZE);
}

void websocket_init(connection_handler_func_t *connection_handler, data_handler_func_t *data_handler) {
    ESP_LOGI(TAG, "websocket_init, coreId: %d, LOG_LOCAL_LEVEL: %d", xPortGetCoreID(), LOG_LOCAL_LEVEL);

    // Use this to enable debug logging in this module
    esp_log_level_set(TAG, ESP_LOG_DEBUG); // ESP_LOG_DEBUG

    websocket_connect(connection_handler, data_handler);
    // Create a queue to handle async replies
    reply_evt_queue = xQueueCreate(10, WEBSOCKET_REPLY_MAX_SIZE);

    // Start task
    xTaskCreate(reply_loop, "hbf_websocket_reply_loop", 4096, NULL, HBF_PRIORITY_MEDIUM, NULL);
}
