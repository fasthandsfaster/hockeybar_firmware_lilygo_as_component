// Smartconfig for HockeyBar Firmware
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_mac.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

// static char ssid[33] = { 0 };
// static char password[65] = { 0 };
static char ip_address_str[16];
static char cellphone_ip_address_str[16];
static char wifi_ws_host_and_port[32] =  { 0 };

static int ws_server_port = 8081; // TODO must match server on mobile device

static esp_netif_t *sta_netif;
static SemaphoreHandle_t initialized_sema;

// The event group allows multiple bits for each event,
// but we only care about one event - are we connected
// to the AP with an IP?
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "HBF_SCFG";

static void smartconfig_event_task(void * parm);

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wifi Station starting");
        xTaskCreate(smartconfig_event_task, "smartconfig_event_task", 4096, NULL, 3, NULL);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wifi Station disconnected - reconnect");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Wifi station connected");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wifi station got ip");
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Smart Config Scan done");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Smart Config Found channel");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Smart Config Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        ESP_LOGI(TAG, "SSID: %s", evt->ssid);
        ESP_LOGI(TAG, "PASSWORD: %s", evt->password);
        
        sprintf(cellphone_ip_address_str, IPSTR, evt->cellphone_ip[0], evt->cellphone_ip[1], evt->cellphone_ip[2], evt->cellphone_ip[3]);
        ESP_LOGI(TAG, "cellphone_ip: %s", cellphone_ip_address_str);

        sprintf(wifi_ws_host_and_port, "%s:%d", cellphone_ip_address_str, ws_server_port);
        ESP_LOGI(TAG, "wifi_ws_host_and_port: %s", wifi_ws_host_and_port);

        wifi_config_t wifi_config;
        bzero(&wifi_config, sizeof(wifi_config_t));
        
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        // if (evt->type == SC_TYPE_ESPTOUCH_V2) {
        //     ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(wifi_ws_host_and_port, sizeof(wifi_ws_host_and_port)) );
        //     ESP_LOGI(TAG, "wifi_ws_host_and_port: %s", wifi_ws_host_and_port);
        // }

        vTaskDelay(300 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, ">>> DISCONNECT");
        ESP_ERROR_CHECK(esp_wifi_disconnect() );

        vTaskDelay(300 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, ">>> SET CONFIG");
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

        vTaskDelay(300 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, ">>> (RE) CONNECT");
        ESP_ERROR_CHECK(esp_wifi_connect());

    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        ESP_LOGI(TAG, "Smart Config Setting ESPTOUCH_DONE_BIT");
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
    else {
        ESP_LOGI(TAG, "event_base: %s, Unexpected event_id: %lu", event_base, event_id);
    }
}

static void initialise_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void smartconfig_event_task(void * parm) {
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );

    ESP_ERROR_CHECK( esp_smartconfig_fast_mode(true));

    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            ESP_ERROR_CHECK( esp_smartconfig_stop() );
            xSemaphoreGive(initialized_sema);
            vTaskDelete(NULL);
        }
    }
}

char* hbf_wifi_get_ip_address_str() {
    return ip_address_str;
}

char* hbf_wifi_get_ws_host_and_port() {
    return wifi_ws_host_and_port;
}

void hbf_wifi_init() {
    ESP_LOGI(TAG, "Initializing SmartConfig wifi ");
    initialized_sema = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi();

    // Wait for config
    xSemaphoreTake(initialized_sema, portMAX_DELAY);

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(sta_netif, &ip_info));
    sprintf(ip_address_str, IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "Got IPv4 address: %s", ip_address_str);
}
