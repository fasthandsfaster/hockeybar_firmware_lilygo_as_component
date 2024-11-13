// Wifi with fixed configuration (SSID, PASW) is used for test
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_mac.h"

// Include generated file
//#include "hbf_params.h"
static const char* wifi_ssid = "H138-380_2300";
static const char* wifi_password = "4dTLmYDE2da";
//static char* wifi_ws_host = HBF_WS_HOST;

static const int connection_max_retry = 5;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "HBF_WIFI";

static char ip_address_str[16];
static int s_retry_num = 0;

char* wifi_get_ip_address_str(void) {
    return ip_address_str;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < connection_max_retry) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void hbf_connect_wifi_in_station_mode(void) {
    ESP_LOGI(TAG, "BEGIN hbf_connect_wifi_in_station_mode.");

    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };
    memcpy(wifi_config.sta.ssid, wifi_ssid, strlen(wifi_ssid));
    memcpy(wifi_config.sta.password, wifi_password, strlen(wifi_password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    
    ESP_LOGI(TAG, "wifi started");
    
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t event_bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened. */
    if (event_bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifi_ssid, wifi_password);
    } 
    else if (event_bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifi_ssid, wifi_password);
    }
    else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(sta_netif, &ip_info));
    sprintf(ip_address_str, IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "Got IPv4 address: %s", ip_address_str);

    ESP_LOGI(TAG, "END hbf_connect_wifi_in_station_mode.");
}

char* hbf_wifi_get_ip_address_str() {
    return ip_address_str;
}

/* char* hbf_wifi_get_ws_host_and_port() {
    return wifi_ws_host;
}*/

void hbf_wifi_init(void) {
    ESP_LOGI(TAG, "Initializing fixed wifi config");
    ESP_ERROR_CHECK(nvs_flash_init());
    hbf_connect_wifi_in_station_mode();
}
