#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define the higest level possible in this module. Use esp_log_level_set to set the actual level
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "hbf_constants.h"
#include "hbf_util.h"
#include "trigger.h"
#include "isr_handlers.h"
#include "driver/gpio.h"

static const char *TAG = "HBF_ISR_HANDLERS";

#define IRTXA_GPIO 9
#define IRRXA_GPIO 8
#define IRTXB_GPIO 38
#define IRRXB_GPIO 37
#define PUSH_GPIO 19

#define PUSH_INPUT_PIN_SEL  ((1ULL<<PUSH_GPIO))
#define GPIO_INPUT_PIN_SEL  ((1ULL<<IRRXA_GPIO) | (1ULL<<IRRXB_GPIO) | (1ULL<<PUSH_GPIO))
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<IRTXA_GPIO) | (1ULL<<IRTXB_GPIO))  
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t isr_evt_queue = NULL;


typedef struct TEST_Q_DATA {
    TickType_t event_ticks;
    trigger_event_t event;
} event_data_t;

event_data_t prev_event_data;
event_data_t event_data;


static void IRAM_ATTR push_gpio_isr_handler(void* arg) {
    event_data_t event;
    event.event = TE_PUSH;
    event_data.event_ticks = xTaskGetTickCount();
    xQueueSendFromISR(isr_evt_queue, &event, NULL);
}


static void IRAM_ATTR breakbeam_gpio_isr_handler(void* arg) {
    ESP_LOGD(TAG, "intrrupt: %d", (int)arg);
    event_data_t event_data;
    switch((int)arg) {
        case IRRXA_GPIO:
            event_data.event = TE_HIT_BEAM_A;
            break;
        case IRRXB_GPIO:
            event_data.event = TE_HIT_BEAM_B;
            break;
        default:
            event_data.event = TE_UNKNOWN;
    }
    event_data.event_ticks = xTaskGetTickCount();
    xQueueSendFromISR(isr_evt_queue, &event_data, NULL);
}
   

static void isr_event_loop(void* arg) {
    ESP_LOGD(TAG, "Starting test_event_loop");
    event_data_t event_data;
    while(true) {
        if(xQueueReceive(isr_evt_queue, &event_data, portMAX_DELAY)) {
            //Remove the problem where a 2 interrupts are triggered by one signal (that looks fine in scope)
            //Reference: https://forum.micropython.org/viewtopic.php?t=8523&p=48304 
            if (event_data.event == prev_event_data.event && (event_data.event_ticks - prev_event_data.event_ticks) < 100) {
                ESP_LOGW(TAG, "Ignored event: %s", trigger_event_to_str(event_data.event));
            } else {
                trigger_evnet_parameters_t event_parameters;
                trigger_add_event_to_queue(event_data.event,event_parameters,event_data.event_ticks,empty_message_id);
            }
            prev_event_data = event_data;
        }
    }
}

void breakbeam_init() {
    // configure gpio for ir reciever
    ///////////////////////////////////////////////

    gpio_config_t irrx_conf = {};
    
    // Interrupt of rising edge
    irrx_conf.intr_type = GPIO_INTR_POSEDGE;

    // Bit mask of the pins
    irrx_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;

    // Set as input mode
    irrx_conf.mode = GPIO_MODE_INPUT;

    // Enable pull-up mode
    irrx_conf.pull_up_en = 1;

    gpio_config(&irrx_conf);

    
    // Confiure gpio for ir transmitter
    ////////////////////////////////////////////////////

    gpio_config_t irtx_conf = {};
    irtx_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    irtx_conf.mode = GPIO_MODE_OUTPUT;
    irtx_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    irtx_conf.pull_down_en = 0;
    //disable pull-up mode
    irtx_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&irtx_conf);
    
    gpio_set_level(IRTXA_GPIO, 1);
    gpio_set_level(IRTXB_GPIO, 1);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // Hook isr handler for specific gpio pin
    gpio_isr_handler_add(IRRXA_GPIO, breakbeam_gpio_isr_handler, (void*) IRRXA_GPIO);
    gpio_isr_handler_add(IRRXB_GPIO, breakbeam_gpio_isr_handler, (void*) IRRXB_GPIO);
}

void push_init(void) {
    // configure gpio for push buttom
    ///////////////////////////////////////////////

    gpio_config_t push_conf = {};
    
    // Interrupt of rising edge
    push_conf.intr_type = GPIO_INTR_NEGEDGE;

    // Bit mask of the pins
    push_conf.pin_bit_mask = PUSH_INPUT_PIN_SEL;

    // Set as input mode
    push_conf.mode = GPIO_MODE_INPUT;

    // Enable pull-up mode
    push_conf.pull_up_en = 1;

    gpio_config(&push_conf);

    // Create a queue to handle gpio event from isr
    //push_queue = xQueueCreate(10, sizeof(uint32_t));

    // Hook isr handler for specific gpio pin
    gpio_isr_handler_add(PUSH_GPIO, push_gpio_isr_handler, (void*) PUSH_GPIO);
}




void isr_loop_init(void) {
    // Use this to enable debug logging in this module
    esp_log_level_set(TAG, ESP_LOG_INFO); // ESP_LOG_DEBUG

    ESP_LOGD(TAG, "isr_loop_init, coreId: %d", xPortGetCoreID());
    
    // Create a queue to handle test events
    isr_evt_queue = xQueueCreate(10, sizeof(event_data_t));

    // Start task at low priority
    xTaskCreate(isr_event_loop, "isr_event_loop", 4096, NULL, HBF_PRIORITY_HIGH, NULL);
}
