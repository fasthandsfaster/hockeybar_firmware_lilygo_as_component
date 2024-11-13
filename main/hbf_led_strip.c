#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"

#include "led_strip_encoder.h"

#include "hbf_constants.h"
#include "hbf_led_strip.h"

static const char *TAG = "HBF_LEDS";

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      35
#define HBF_LED_COUNT 18
#define HBF_LED_PIXEL_COUNT (HBF_LED_COUNT * 3)

typedef uint8_t led_strip_pixels_array_t[HBF_LED_PIXEL_COUNT];
static led_strip_pixels_array_t led_strip_pixels;

static rmt_channel_handle_t led_channel = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// Animation
static SemaphoreHandle_t animation_sema = NULL;
static StaticSemaphore_t animation_sema_buffer;
static bool animation_running = false;
static led_strip_animation_type_t animation_type;

void hbf_led_strip_set_same_color(uint32_t rgb, int led_count) {
    uint8_t red = (rgb >> 16) & 0xff;
    uint8_t green = (rgb >> 8) & 0xff;
    uint8_t blue = rgb & 0xff;
    int p = 0;
    for (int led_no = 0; led_no < HBF_LED_COUNT; led_no++) {
        if (led_no < led_count) {
            led_strip_pixels[p++] = green;
            led_strip_pixels[p++] = red;
            led_strip_pixels[p++] = blue;
        }
        else {
            led_strip_pixels[p++] = 0;
            led_strip_pixels[p++] = 0;
            led_strip_pixels[p++] = 0;        
        }
    }
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    ESP_ERROR_CHECK(rmt_transmit(led_channel, led_encoder, led_strip_pixels, HBF_LED_PIXEL_COUNT, &tx_config));
}

void hbf_led_strip_set_same_color_offset(uint32_t rgb, int led_count, int led_start) {
    uint8_t red = (rgb >> 16) & 0xff;
    uint8_t green = (rgb >> 8) & 0xff;
    uint8_t blue = rgb & 0xff;
    int p = led_start * 3;
    printf("r: %d g: %d, b: %d\n",red,green,blue);
    for (int led_no = led_start; led_no < (led_start + led_count); led_no++) {  // HBF_LED_COUNT
        if (led_no < (led_start + led_count)) {
            //printf("Set color %d",p);
            led_strip_pixels[p++] = green;
            led_strip_pixels[p++] = red;
            led_strip_pixels[p++] = blue;
        }
        else {
            //printf("Set zero %d",p);
            led_strip_pixels[p++] = 0;
            led_strip_pixels[p++] = 0;
            led_strip_pixels[p++] = 0;        
        } 
    }
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    printf("before transmit\n");
    if ((led_start + led_count) == HBF_LED_COUNT) {
        printf("transmit\n");
        //for(int idx = 0; idx < 38*3; idx++)
        //    printf("index %d array value: %d XX ",idx, led_strip_pixels[idx]);
        ESP_ERROR_CHECK(rmt_transmit(led_channel, led_encoder, led_strip_pixels, HBF_LED_PIXEL_COUNT, &tx_config));
    }
}

void hbf_led_strip_rgb(uint32_t rgb, int led_count) {
    hbf_led_strip_stop_animation();
    hbf_led_strip_set_same_color(rgb, led_count);
}

void hbf_led_strip_red(void) {
    hbf_led_strip_stop_animation();
    hbf_led_strip_set_same_color(HBF_RGB_DIM_RED, HBF_LED_COUNT);
}

void hbf_led_strip_green(void) {
    hbf_led_strip_stop_animation();
    hbf_led_strip_set_same_color(HBF_RGB_DIM_GREEN, HBF_LED_COUNT);
}

void hbf_led_strip_blue(void) {
    hbf_led_strip_stop_animation();
    hbf_led_strip_set_same_color(HBF_RGB_DIM_BLUE, HBF_LED_COUNT);
}

void hbf_led_strip_off(void) {
    hbf_led_strip_stop_animation();
    hbf_led_strip_set_same_color(HBF_RGB_OFF, HBF_LED_COUNT);
}


void hbf_led_strip_triangle_ab(void) {
    hbf_led_strip_stop_animation();
    hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_GREEN,15,0);
    hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_BLUE,4,15);
    hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_RED,15,19);
}

void hbf_led_strip_test_meter(void) {
    uint32_t colors[] = {HBF_RGB_DIM_RED, HBF_RGB_DIM_GREEN, HBF_RGB_DIM_BLUE};
    int color_count = sizeof(colors) / sizeof(uint32_t);

    for (int i = 0; i < color_count; i++) {
        for (int n = 1; n <= HBF_LED_COUNT; n++) {
            hbf_led_strip_set_same_color(colors[i], n);
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }
    }
}

void hbf_led_strip_test_colors(void) {
    int step = 32;
    int max = 128;
    for (uint32_t red = 0; red < max; red += step) {
        for (uint32_t green = 0; green < max; green += step) {
            for (uint32_t blue = 0; blue < max; blue += step) {

                uint32_t rgb = (red << 16) | (green << 8) | blue;
                printf("rgb: %06lX\n", rgb);
                hbf_led_strip_set_same_color(rgb, HBF_LED_COUNT);
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
    }
}

static void hbf_led_strip_animation_loop(void* arg) {
    ESP_LOGD(TAG, "Starting hbf_led_strip_animation_loop");

    while(true) {
        animation_running = false;

        // Wait for start signal
        ESP_LOGD(TAG, "WAITING");
        xSemaphoreTake(animation_sema, portMAX_DELAY);

        animation_running = true;
        TickType_t delay = 10 / portTICK_PERIOD_MS;
        int index = 0;

        ESP_LOGD(TAG, "STARTED");
        while(!xSemaphoreTake(animation_sema, delay)) {
            ESP_LOGD(TAG, "RUNNING: %d", index);
            switch (animation_type) {
                case LSA_RUNNING_BLUE:
                    hbf_led_strip_set_same_color(HBF_RGB_DIM_BLUE, (index % HBF_LED_COUNT) + 1);
                    delay = 1000 / portTICK_PERIOD_MS;
                    break;
                
                case LSA_FLASHING_GREEN:
                    hbf_led_strip_set_same_color(index % 2 == 0 ? HBF_RGB_BRIGHT_GREEN : HBF_RGB_OFF, HBF_LED_COUNT);
                    delay = 200 / portTICK_PERIOD_MS;
                    if (index > 10) {
                        xSemaphoreGive(animation_sema); // Stop flash
                    }
                    break;
            }
            index++;
        }
    }
}

void hbf_led_strip_start_animation(led_strip_animation_type_t type) { // TODO Add: interval 
    if (animation_running) {
        ESP_LOGD(TAG, "SENDING STOP SIGNAL - BEFORE START");
        xSemaphoreGive(animation_sema);
    }
    animation_type = type;
    ESP_LOGD(TAG, "SENDING START SIGNAL");
    xSemaphoreGive(animation_sema);
}

void hbf_led_strip_stop_animation() {
    if (animation_running) {
        ESP_LOGD(TAG, "SENDING STOP SIGNAL");
        xSemaphoreGive(animation_sema);
    }
}

void hbf_led_strip_init(void) {
    // Use this to enable debug logging in this module
    esp_log_level_set(TAG, ESP_LOG_INFO); // ESP_LOG_DEBUG

    ESP_LOGD(TAG, "Create RMT TX channel");
    led_channel = NULL;
    rmt_tx_channel_config_t tx_channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,



        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_config, &led_channel));

    ESP_LOGD(TAG, "Install led strip encoder");
    led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGD(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_channel));

    animation_sema = xSemaphoreCreateCountingStatic(10, 0, &animation_sema_buffer);
    assert(animation_sema);

    // Start task at low priority
    xTaskCreate(hbf_led_strip_animation_loop, "hbf_led_strip_animation_loop", 4096, NULL, HBF_PRIORITY_LOW, NULL);

    // hbf_led_strip_start_animation(LSA_RUNNING_BLUE);  // TODO REMOVE

    // hbf_led_strip_test_meter();
    // hbf_led_strip_test_colors();

    hbf_led_strip_red();
    //hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_GREEN,9,0);
    //hbf_led_strip_set_same_color_offset(HBF_RGB_DIM_RED,9,9);

    ESP_LOGI(TAG, "Led Strip Innitialized");
}
