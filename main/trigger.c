#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"

#include "hbf_constants.h"
#include "hbf_util.h"
#include "hbf_wifi.h"
#include "websocket.h"
#include "trigger.h"
#include "seven_seg.h"
#include "hbf_led_strip.h"
#include "hbf_util.h"

static const char *TAG = "HBF_TRIG";

static QueueHandle_t trigger_evt_queue = NULL;
static int device_id = -1;

char* trigger_event_to_str(trigger_event_t e) {
    return
        e==TE_START ? "TE_START" :
        e==TE_HIT_BEAM_A ? "TE_HIT_BEAM_A" :
        e==TE_HIT_BEAM_B ? "TE_HIT_BEAM_B" : 
        e==TE_HIT_BEAM_C ? "TE_HIT_BEAM_C" : "TE_UNKNOWN";
}

trigger_path_t trigger_path_str_to_enum(const char* s) {
    return
        !strcasecmp(s,"TP_A_TO_B") ? TP_A_TO_B :
        !strcasecmp(s,"TP_B_TO_A") ? TP_B_TO_A :
        !strcasecmp(s,"TP_UNDEF") ? TP_UNDEF: TP_UNDEF;
}

trigger_path_t trigger_direction_str_to_enum(const char* s) {
    return
        !strcasecmp(s,"TD_A_TO_B") ? TD_A_TO_B :
        !strcasecmp(s,"TD_B_TO_A") ? TD_B_TO_A :
        !strcasecmp(s,"TD_UNKNOWN") ? TD_UNKNOWN: TD_UNKNOWN;
}

char* trigger_path_to_str(trigger_path_t e) {
    return
        e==TP_A_TO_B ? "TP_A_TO_B" :
        e==TP_B_TO_A ? "TP_B_TO_A" : "TP_UNDEF";
        
}

char* trigger_direction_to_str(trigger_direction_t e) {
    return
        e==TD_A_TO_B ? "TD_A_TO_B" :
        e==TD_B_TO_A ? "TD_B_TO_A" : "TD_UNKNOWN";
}

trigger_event_t trigger_event_str_to_enum(const char* s) {
    return
        !strcasecmp(s,"TE_START") ? TE_START :
        !strcasecmp(s,"TE_HIT_BEAM_A") ? TE_HIT_BEAM_A :
        !strcasecmp(s,"TE_HIT_BEAM_B") ? TE_HIT_BEAM_B : TE_UNKNOWN;
}

char* trigger_state_to_str(trigger_state_t e) {
    return
        e==TS_IDLE ? "TS_IDLE" :
        e==TS_WAIT_FOR_FIRST_HIT ? "TS_WAIT_FOR_FIRST_HIT" :
        e==TS_WAIT_FOR_SECOND_HIT ? "TS_WAIT_FOR_SECOND_HIT" : "TS_UNKNOWN";
}




void trigger_add_event_to_queue(trigger_event_t trigger_event, trigger_evnet_parameters_t event_parameters, TickType_t event_ticks ,char* message_id) {
    trigger_data_t trigger_data;
    trigger_data.event = trigger_event;
    trigger_data.event_ticks = event_ticks;
    trigger_data.event_parameters = event_parameters;
    char message_id_array[33];
    sprintf(message_id_array, "%s", message_id);
    strcpy(trigger_data.message_id,message_id_array);
    if (!xQueueSendToBack(trigger_evt_queue, &trigger_data, portMAX_DELAY)) {
        ESP_LOGE(TAG, "*** trigger_evt_queue is full");
    }
}

void trigger_start_event_to_queue() {
    trigger_data_t trigger_data;
    trigger_data.event = TE_START;
    trigger_data.event_parameters.time_open = 2000;
    char message_id_array[33];
    sprintf(message_id_array, "%s", "0");
    strcpy(trigger_data.message_id,message_id_array);
    if (!xQueueSendToBack(trigger_evt_queue, &trigger_data, portMAX_DELAY)) {
        ESP_LOGE(TAG, "*** trigger_evt_queue is full");
    }
}

char* get_timestamp_str() {
   int system_millis = xTaskGetTickCount() / portTICK_PERIOD_MS;
   ESP_LOGD(TAG, "System millis, %d", system_millis);
   int reminder = 0;
   int hours = system_millis / 3600000;
   reminder = system_millis % 3600000;
   ESP_LOGD(TAG, "hours %d, reminder, %d", hours, reminder);
   int minutes = reminder / 60000;
   reminder = reminder % 60000;
   ESP_LOGD(TAG, "minutes %d, reminder, %d", minutes, reminder);
   int seconds = reminder / 1000;
   reminder = reminder % 1000;   
   ESP_LOGD(TAG, "seconds %d, reminder, %d", seconds, reminder);
   int millis = reminder;
   ESP_LOGD(TAG, "remaining: millis %d", millis);
   char timestamp_buf[36];
   char* timestamp_str = "1900-01-01T00:00:00.000Z";
   sprintf(timestamp_buf,"1900-01-01T%d%d:%d%d:%d%d.%d%d%dZ",hours/10,hours%10,minutes/10,minutes%10,seconds/10,seconds%10,millis/100, (millis%100)/10,millis%10);
   strncpy(timestamp_str,timestamp_buf,25);
   ESP_LOGD(TAG, "current date string: %s", timestamp_str);
   return timestamp_str;
}
/*
void add_json_header(char* json) {
    json_object_add_string(json,"mac",get_mac_address_str());
    json_object_add_string(json, "ip", hbf_wifi_get_ip_address_str());
    json_object_add_string(json,"messageId",get_messageid_str());
    json_object_add_string(json,"timestamp",get_timestamp_str());
    json_object_add_int(json, "deviceId", device_id);
}
*/

void add_json_header(char* json,char* message_id) {
    ESP_LOGI(TAG, "Header message id: %s",message_id);
    json_object_add_string(json,"deviceId",get_mac_address_str());
    if (strcmp(message_id,"00000000-0000-0000-0000-000000000000")) {
        json_object_add_string(json,"messageId",message_id);
    } else {
        json_object_add_null(json,"messageId");
    }
    json_object_add_string(json,"timestamp","12:00"); // get_timestamp_str());
} 


void add_acknowledge_payload(char* json, char* task) {
    const int buffer_size = 300;
    char payload[buffer_size];
    json_object_begin(payload);
    json_object_add_string(payload, "confirmedTask", task);
    json_object_end(payload);
    json_object_add_object(json, "payload",payload);
}

void return_acknowledge(char* task, char* message_id) {
    const int buffer_size = 300;
    char json[buffer_size];
    json_object_begin(json);
    add_json_header(json,message_id);
    add_acknowledge_payload(json,task);
    json_object_end(json);
    json_check_data_length(json, buffer_size);
    //websocket_send_text_async(json);
}


void add_connected_payload(char* json) {
    const int buffer_size = 300;
    char payload[buffer_size];
    json_object_begin(payload);
    json_object_add_bool(payload, "connected", true);
    json_object_add_string(payload,"type","Port");
    json_object_add_string(payload,"hwVersion","1.0");
    json_object_add_string(payload,"swVersion","1.0");
    json_object_end(payload);
    json_object_add_object(json, "payload",payload);
}


void return_connected_result(char* message_id) {
    const int buffer_size = 300;
    char json[buffer_size];
    json_object_begin(json);
    add_json_header(json, message_id);
    add_connected_payload(json);
    json_object_end(json);
    json_check_data_length(json, buffer_size);
    ESP_LOGI(TAG, "before  websocket_send_text_async");
    websocket_send_text_async(json);
    ESP_LOGI(TAG, "after websocket_send_text_async");
}

void add_init_payload(char* json) {
    const int buffer_size = 200;
    char payload[buffer_size];
    json_object_begin(payload);
    json_object_add_bool(payload, "initialized", true);
    json_object_end(payload);
    json_object_add_object(json, "payload",payload);

}

void return_init_result(char* message_id) {
    const int buffer_size = 200;
    char json[buffer_size];
    json_object_begin(json);
    add_json_header(json, message_id);
    add_init_payload(json);
    json_object_end(json);
    json_check_data_length(json, buffer_size);
    websocket_send_text_async(json);
}

void add_echo_payload(char* json,int index) {
    const int buffer_size = 200;
    char payload[buffer_size];
    json_object_begin(payload);
    json_object_add_string(payload, "task", "echo");
    json_object_add_int(payload, "index", index);
    json_object_end(payload);
    json_object_add_object(json, "payload",payload);
}

void return_echo_result(int index,char* message_id) {
    const int buffer_size = 200;
    char json[buffer_size];
    json_object_begin(json);
    add_json_header(json, message_id);
    add_echo_payload(json,index);
    json_object_end(json);
    json_check_data_length(json, buffer_size);
    websocket_send_text_async(json);
}

void add_solved_payload(char* json,trigger_direction_t direction, int reaction_millis, int first_hit_millis, double speed) {
    const int buffer_size = 200;
    char payload[buffer_size];
    json_object_begin(payload);
    json_object_add_bool(payload, "solved", true);
    json_object_add_int(payload, "time", first_hit_millis);
    json_object_add_int(payload, "reactiontime", reaction_millis);
    json_object_add_string(payload, "direction", trigger_direction_to_str(direction));
    json_object_add_double(payload, "speed", speed);
    json_object_end(payload);
    json_object_add_object(json, "payload",payload);

}


void return_result_as_solved(trigger_direction_t direction, int reaction_millis, int first_hit_millis, double speed, char* message_id) {
    const int buffer_size = 200;
    char json[buffer_size];
    json_object_begin(json);
    add_json_header(json, message_id);
    add_solved_payload(json,direction,reaction_millis,first_hit_millis, speed);
    json_object_end(json);
    json_check_data_length(json, buffer_size);
    websocket_send_text_async(json);
}

void add_not_solved_payload(char* json,trigger_direction_t direction, int first_hit_millis, double speed) {
    const int buffer_size = 200;
    char payload[buffer_size];
    json_object_begin(payload);
    json_object_add_bool(payload, "solved", false);
    json_object_add_int(payload, "time", first_hit_millis);
    json_object_add_string(payload, "direction", trigger_direction_to_str(direction));
    json_object_add_double(payload, "speed", speed);
    json_object_end(payload);
    json_object_add_object(json, "payload",payload);
}

void return_result_as_not_solved(int timeout_millis,char* message_id) {
    const int buffer_size = 200;
    char json[buffer_size];
    json_object_begin(json);
    add_json_header(json, message_id);
    add_not_solved_payload(json,TD_UNKNOWN,0,0);
    json_object_end(json);
    json_check_data_length(json, buffer_size);
    websocket_send_text_async(json);
}

void hbf_ws_connection_handler(int32_t event_id) {
    ESP_LOGI(TAG, "hbf_ws_connection_handler, event_id: %lu", event_id);
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "LHP before connected result sent");
        //seven_seg_stop_flasher();
        // hbf_led_strip_rgb(HBF_RGB_OFF);
        return_connected_result(empty_message_id);
        ESP_LOGI(TAG, "LHP connected result sent");
        break;
    
    case WEBSOCKET_EVENT_DISCONNECTED:
        //seven_seg_start_flasher();
        //hbf_led_strip_start_animation(LSA_RUNNING_BLUE);
        break;
    }
}

// The hbf_ws_data_handler is called from the websocket when there is incoming data
void hbf_ws_data_handler(const char* data_str, int data_len) {
    // handle message from control unit
    ESP_LOGI(TAG, "data_handler 1");
    cJSON *root = cJSON_ParseWithLength(data_str, data_len);
    char* unique_id = hbf_safe_GetObjectString(root, "deviceId", "");
    char* message_id = hbf_safe_GetObjectString(root, "messageId", "");
    char* timestamp = hbf_safe_GetObjectString(root, "timestamp", "");
    ESP_LOGI(TAG, "data_handler 2 unique_id: %s",unique_id);
    if(false) { //(!strcmp(unique_id,"") || !strcmp(message_id,"") || !strcmp(timestamp,"")) {
        ESP_LOGE(TAG, "Json header malformed: %s", unique_id);
    } 
    else {
        cJSON *payload = cJSON_GetObjectItem(root, "payload");
        char *payload_str = cJSON_Print(payload);
        ESP_LOGD(TAG, "JSON payload: %s",payload_str);
        if (payload == NULL) {
            ESP_LOGE(TAG, "payload empty");
        } 
        else {
            char* task = hbf_safe_GetObjectString(payload, "task", "");
            //An exerzise is started,send to all elements
            if (!strcmp(task, "start")) {
                printf("\n");
                return_acknowledge(task,message_id);
                // Get exercise start event parameters from input json here
            }
            else if (!strcmp(task, "set")) {
                // Element is set durring exercise execution
                return_acknowledge(task,message_id);
                cJSON *parameters = cJSON_GetObjectItem(payload, "parameters");
                trigger_evnet_parameters_t event_parameters;
                // Get exercise ongoing event parameters from input json here
                char* timestamp = hbf_safe_GetObjectString(parameters, "timestamp", "");
                event_parameters.time_open = hbf_safe_GetObjectInt(parameters, "timeOpen", 2000);
                event_parameters.silent = hbf_safe_GetObjectInt(parameters, "silent", false);
                char* path_str = hbf_safe_GetObjectString(parameters, "path", "TP_UNKNOWN");
                event_parameters.path = trigger_path_str_to_enum(path_str);
                TickType_t xTicks_event_time = xTaskGetTickCount();
                trigger_add_event_to_queue(TE_START, event_parameters, xTicks_event_time, message_id);

                //cJSON * sim_events_array = cJSON_GetObjectItem(root, "sim_events");
                //if (sim_events_array != NULL) {
                    // Found simulation data - use for test
                    //test_simulate_delayed_events(sim_events_array);
                //}
            }
            else if (!strcmp(task, "init")) {
                // Element is assigned an id after connection to control unit
                return_acknowledge(task,message_id);
                device_id = hbf_safe_GetObjectInt(payload, "elementId",0);
                ESP_LOGI(TAG, "Got device id: %d", device_id);
                seven_seg_shownumber(device_id);
                return_init_result(message_id);
                hbf_led_strip_off();
            }
            else if (!strcmp(task, "over")) {
                // Exercise is over, send to all elements
                return_acknowledge(task,message_id);
                int result = hbf_safe_GetObjectInt(payload, "result",0);
                trigger_evnet_parameters_t event_parameters;
                TickType_t xTicks_event_time = xTaskGetTickCount();
                trigger_add_event_to_queue(TE_OVER,event_parameters,xTicks_event_time,message_id);
                ESP_LOGI(TAG, "Got result: %d", result);
                hbf_led_strip_off(); 
                //seven_seg_shownumber(result);
                vTaskDelay(15000 / portTICK_PERIOD_MS);
                //seven_seg_shownumber(device_id);
            }

            else if (!strcmp(task, "ledstrip")) {
                return_acknowledge(task,message_id);
                u32_t rgb = hbf_safe_GetObjectInt(payload, "rgb", 0);
                int count = hbf_safe_GetObjectInt(payload, "count", 0);
                ESP_LOGI(TAG, "Got rgb: %lX, count: %d", rgb, count);
                hbf_led_strip_rgb(rgb, count);
            }
            else if (!strcmp(task, "echo")) {
                ESP_LOGI(TAG, "data_handler 4 - Echo");
                int index = hbf_safe_GetObjectInt(payload, "index", 0);
                // ESP_LOGI(TAG, "echo: index: %d", index);
                return_echo_result(index, message_id);
            }
            else {
                ESP_LOGE(TAG, "Unknown task: %s", task);
            }
        }    
    }
    // Free all memory, including child nodes
    cJSON_Delete(root);
}

static void event_trigger_loop(void* arg) {

    // Wait for the data on the queue
    ESP_LOGI(TAG, "Starting event_trigger_loop");

    trigger_data_t trigger_data;
    TickType_t trigger_queue_timeout_millis = NO_TIMEOUT;
    trigger_state_t trigger_state = TS_IDLE;
    TickType_t max_interval_between_first_and_second_hit_millis = 500;
    int first_hit_millis = 0;
    trigger_direction_t direction = TD_UNKNOWN;
    trigger_event_t first_hit = TE_UNKNOWN;
    trigger_event_t second_hit = TE_UNKNOWN;
    trigger_path_t path = TP_UNDEF;
    TickType_t xTicks_reaction_time;
    TickType_t xTicks_set_time = 0;    
    static char* set_message_id = "00000000-0000-0000-0000-000000000000";

    while(true) {
        // UBaseType_t queue_space = uxQueueSpacesAvailable(trigger_evt_queue);
        // ESP_LOGI(TAG, "Trigger state: %s - trigger_queue_timeout_millis: %d", trigger_state_to_str(trigger_state), trigger_queue_timeout_millis);

        TickType_t xTicks_wait_started = xTaskGetTickCount();
        TickType_t xTicksToWait = trigger_queue_timeout_millis == NO_TIMEOUT ? portMAX_DELAY : trigger_queue_timeout_millis / portTICK_PERIOD_MS;

        if(xQueueReceive(trigger_evt_queue, &trigger_data, portMAX_DELAY)) {
            // Queue has data
            bool event_handled = false;
            int elapsed_millis = (xTaskGetTickCount() - xTicks_wait_started) * portTICK_PERIOD_MS;

            if (trigger_data.event == TE_OVER) {
                hbf_led_strip_off();
                trigger_state = TS_IDLE;
                first_hit_millis = 0;
                direction = TD_UNKNOWN;
            } else {               
                switch (trigger_state) {
                    case TS_IDLE:
                        if (trigger_data.event == TE_START) {
                            //strcpy(set_message_id,trigger_data.message_id);
                            // Handle parameters
                            
                            ESP_LOGI(TAG, "Set message id: %s",set_message_id);
                            trigger_queue_timeout_millis = trigger_data.event_parameters.time_open;
                            bool silent_flag = trigger_data.event_parameters.silent;
                            path = trigger_data.event_parameters.path;
                            xTicks_set_time = xTaskGetTickCount();
                            trigger_state = TS_WAIT_FOR_FIRST_HIT;
                            event_handled = true;
                            ESP_LOGI(TAG, "PATH: %s",trigger_path_to_str(path));
                            switch (path) {
                                case TP_A_TO_B:
                                    first_hit = TE_HIT_BEAM_A;
                                    second_hit = TE_HIT_BEAM_B;
                                    if (!silent_flag) {
                                        hbf_led_strip_stop_animation();
                                        hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_RED,9,0);
                                        hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_GREEN,9,9);
                                    }

                                    break;

                                case TP_B_TO_A:
                                    first_hit = TE_HIT_BEAM_B;
                                    second_hit = TE_HIT_BEAM_A;
                                    
                                    if (!silent_flag) {
                                        hbf_led_strip_stop_animation();
                                        hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_GREEN,9,0);
                                        hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_RED,9,9);
                                    }

                                    break;

                                case TP_UNDEF:
                                    
                                    if (!silent_flag) {
                                        hbf_led_strip_stop_animation();
                                        hbf_led_strip_green();
                                    }

                                    break;
                            } 
                            //return_result_as_solved(direction, first_hit_millis, 0,(char *) set_message_id);
                        }
                        break;


                    case TS_WAIT_FOR_FIRST_HIT:
                        if (trigger_data.event == first_hit || path == TP_UNDEF) {
                            // When the first hit comes we know the direction
                            direction = trigger_data.event == TE_HIT_BEAM_A ? TD_A_TO_B : TD_B_TO_A;

                            first_hit_millis = elapsed_millis;
                            trigger_state = TS_WAIT_FOR_SECOND_HIT;
                            //trigger_state = TS_IDLE;
                            trigger_queue_timeout_millis = max_interval_between_first_and_second_hit_millis;

                            set_message_id = empty_message_id;
                            //hbf_led_strip_off();
                            event_handled = true;
                        } else {
                            int timeout_millis = (xTaskGetTickCount() - xTicks_wait_started) * portTICK_PERIOD_MS;
                            return_result_as_not_solved(timeout_millis,set_message_id);
                            set_message_id = empty_message_id;
                            trigger_state = TS_IDLE;
                            trigger_queue_timeout_millis = NO_TIMEOUT;
                            ESP_LOGI(TAG, "Timeout");
                            hbf_led_strip_red();
                            vTaskDelay(50 / portTICK_PERIOD_MS);
                            hbf_led_strip_off();
                            //vTaskDelay(500 / portTICK_PERIOD_MS);
                            //trigger_start_event_to_queue();
                            
                            event_handled = true;
                        }
                        break;

                    case TS_WAIT_FOR_SECOND_HIT:
                        // We use the second hit to calculate the speed
                        if (trigger_data.event == second_hit || path == TP_UNDEF) {
                            xTicks_reaction_time = xTaskGetTickCount() - xTicks_set_time;
                            trigger_state = TS_IDLE;
                            trigger_queue_timeout_millis = NO_TIMEOUT;
                            event_handled = true;
                            double speed = elapsed_millis>0 ? HBF_DISTANCE_BETWEEN_BEAMS_MM / elapsed_millis : -1; // meter per second
                            set_message_id = empty_message_id;
                            ESP_LOGI(TAG, "solved message id: %s",set_message_id);
                            return_result_as_solved(direction, first_hit_millis, xTicks_reaction_time,speed,(char *) set_message_id);
                            hbf_led_strip_green();
                            vTaskDelay(50 / portTICK_PERIOD_MS);
                            hbf_led_strip_off();
                            //hbf_led_strip_start_animation(LSA_FLASHING_GREEN);
                        } else {
                            int timeout_millis = (xTaskGetTickCount() - xTicks_wait_started) * portTICK_PERIOD_MS;
                            return_result_as_not_solved(timeout_millis,set_message_id);
                            set_message_id = empty_message_id;
                            trigger_state = TS_IDLE;
                            trigger_queue_timeout_millis = NO_TIMEOUT;
                            ESP_LOGI(TAG, "Timeout");
                            hbf_led_strip_red();
                            vTaskDelay(50 / portTICK_PERIOD_MS);
                            hbf_led_strip_off();

                            
                        }
                        //vTaskDelay(500 / portTICK_PERIOD_MS);
                        //trigger_start_event_to_queue();
                        event_handled = true;
                        trigger_state = TS_IDLE;
                        break;
                }
    
                if (event_handled) {
                    ESP_LOGI(TAG, "Got event %s", trigger_event_to_str(trigger_data.event));
                }
                else {
                    ESP_LOGI(TAG, "Ignored event: %s", trigger_event_to_str(trigger_data.event));
                }
            }
        }
        else {
            //Timeout from event queue
            int timeout_millis = (xTaskGetTickCount() - xTicks_wait_started) * portTICK_PERIOD_MS;
            return_result_as_not_solved(timeout_millis,set_message_id);
            set_message_id = empty_message_id;
            trigger_state = TS_IDLE;
            trigger_queue_timeout_millis = NO_TIMEOUT;
            ESP_LOGI(TAG, "Timeout");
            hbf_led_strip_red();
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            hbf_led_strip_off();
        }
    }
}

void trigger_init(void) {
    esp_log_level_set(TAG, ESP_LOG_DEBUG); // ESP_LOG_DEBUG

    ESP_LOGI(TAG, "trigger_init, coreId: %d", xPortGetCoreID());
    
    // Create a queue to handle trigger events
    trigger_evt_queue = xQueueCreate(10, sizeof(trigger_data_t));

    // Start trigger task at higest priority
    xTaskCreate(event_trigger_loop, "hbf_trigger_loop", 4096, NULL, HBF_PRIORITY_MEDIUM, NULL);
}
