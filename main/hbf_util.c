#include "esp_mac.h"
#include "esp_log.h"

#include "hbf_util.h"

static const char *TAG = "HBF_UTIL";

// Create JSON

int json_object_begin(char* json) {
    return sprintf(json, "{");
}

int json_object_add_string(char* json, char* key,  char* value) {
    // TODO escape special characters, quote, etc.
    return sprintf(json + strlen(json), "%s\"%s\":\"%s\"", (strlen(json) <= 1 ? "" : ","), key, value);
}

int json_object_add_int(char* json, char* key, int value) {
    return sprintf(json + strlen(json), "%s\"%s\":%d", (strlen(json) <= 1 ? "" : ","), key, value);
}

int json_object_add_double(char* json, char* key, double value) {
    return sprintf(json + strlen(json), "%s\"%s\":%lf", (strlen(json) <= 1 ? "" : ","), key, value);
}

int json_object_add_bool(char* json, char* key,  bool value) {
    return sprintf(json + strlen(json), "%s\"%s\":%s", (strlen(json) <= 1 ? "" : ","), key, (value ? "true" : "false"));
}

int json_object_add_null(char* json, char* key) {
    return sprintf(json + strlen(json), "%s\"%s\":null", (strlen(json) <= 1 ? "" : ","), key);
}


int json_object_add_object(char* json, char* key,  char* value) {
    return sprintf(json + strlen(json), "%s\"%s\":%s", (strlen(json) <= 1 ? "" : ","), key, value);
}


int json_object_end(char* json) {
    return sprintf(json + strlen(json), "}");
}

void json_check_data_length(const char* json, int buffer_size) {
    size_t data_length = strlen(json);
    if (data_length > buffer_size) {
        ESP_LOGE(TAG, "JSON data overflows buffer: data length: %d, buffer size: %d", data_length, buffer_size);
    }
}


// Parse JSON

int hbf_safe_GetObjectInt(const cJSON * const object, const char * const string, const int default_value) {
    cJSON *item = cJSON_GetObjectItem(object, string);
    return item == NULL ? default_value : item->valueint;
}

char* hbf_safe_GetObjectString(const cJSON * const object, const char * const string, char* default_value) {
    cJSON *item = cJSON_GetObjectItem(object, string);
    return item == NULL ? default_value : item->valuestring;
}

cJSON hbf_safe_GetObjectItem(const cJSON * const object, const char * const string, cJSON* default_value) {
    cJSON *item = cJSON_GetObjectItem(object, string);
    ESP_LOGI(TAG, "data_handler 2.2");
    return *item;
}

// Other stuff
static char mac_address_str[18] = {0};

char* get_mac_address_str() {
    if (mac_address_str[0] == 0) {
        ESP_LOGD(TAG, "Reading MAC adress");
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
        sprintf(mac_address_str, MACSTR, MAC2STR(mac));
    }
    return mac_address_str;
}