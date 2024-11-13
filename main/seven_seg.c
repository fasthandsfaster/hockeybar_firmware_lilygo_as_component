#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "driver/i2c.h"

#include "hbf_constants.h"

static const char *TAG = "HBF_7SEG";

#define I2C_MASTER_SCL_IO 11        // GPIO number used for I2C master clock
#define I2C_MASTER_SDA_IO 10        // GPIO number used for I2C master data
#define I2C_MASTER_NUM 0            // I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip
#define I2C_MASTER_FREQ_HZ 400000   // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE 0 // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE 0 // I2C master doesn't need buffer

#define I2C_ADDR_WR 0x10 // Slave address of the Silego

#define SSEG_BLANK 0xff
#define SSEG_3_HORIZONTAL_LINES 0xb6

static const uint8_t hex_value_lookup[] = {0xC0, 0xF9, 0XA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E};

static void i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = 0,
        .scl_pullup_en = 0,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0));
}

static bool flasher_active = false;

// Sequential Write command, starting from register 0xD0.
// Each subsequent data byte will increment the internal address counter,
// and will be written into the next higher byte in the command addressing
void i2c_write_asm_state(const uint8_t *data, size_t data_len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    /* Start byte */
    i2c_master_start(cmd);

    /* Slave address byte */
    i2c_master_write_byte(cmd, I2C_ADDR_WR, true);
    i2c_master_write_byte(cmd, 0xD0, true);
    i2c_master_write(cmd, data, data_len, true);

    /* Stop byte */
    i2c_master_stop(cmd);

    /* Ready to fire */
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(5));

    /* Deinitialize to release allocated resources */
    i2c_cmd_link_delete(cmd);
}

void seven_seg_shownumber(int number) {
    int base = 10;
    bool show_decimal_point = false;
    uint8_t dp_mask = show_decimal_point ? 0x7f : 0xff;

    const int DIGIT_COUNT = 3;
    uint8_t digit_buf[DIGIT_COUNT];

    int value = number;
    for (int p = DIGIT_COUNT-1; p >= 0; p--) {
        if (number > 0 && value == 0) {
            digit_buf[p] = SSEG_BLANK & dp_mask;
        }
        else {
            uint8_t digit = value % base;
            digit_buf[p] = hex_value_lookup[digit] & dp_mask;
        }
        if (value > 0) {
            value /= base;
        }
    }
    i2c_write_asm_state(digit_buf, DIGIT_COUNT);
}

static void seven_seg_flasher_loop(void* arg) {
    ESP_LOGD(TAG, "Starting seven_seg_flasher_loop");
    uint8_t pattern_off[] = {SSEG_BLANK, SSEG_BLANK, SSEG_BLANK};
    uint8_t pattern_on[] = {SSEG_3_HORIZONTAL_LINES, SSEG_3_HORIZONTAL_LINES, SSEG_3_HORIZONTAL_LINES};
    while(true) {
        if (flasher_active) {
            i2c_write_asm_state(pattern_on, 3);
            vTaskDelay(300 / portTICK_PERIOD_MS);

            if (flasher_active) {
                i2c_write_asm_state(pattern_off, 3);
                vTaskDelay(300 / portTICK_PERIOD_MS);
            }
        }
        else {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

void seven_seg_start_flasher() {
    flasher_active = true;
}

void seven_seg_stop_flasher() {
    flasher_active = false;
}

void seven_seg_init(void) {
    esp_log_level_set(TAG, ESP_LOG_DEBUG); // ESP_LOG_DEBUG
    i2c_master_init();

    // Start task at low priority
    xTaskCreate(seven_seg_flasher_loop, "hbf_seven_seg_flasher_loop", 4096, NULL, HBF_PRIORITY_LOW, NULL);

    seven_seg_start_flasher();
}
