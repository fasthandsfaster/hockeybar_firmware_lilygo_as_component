#include "driver/spi_master.h"
#include "esp_websocket_client.h"

#define WEBSOCKET_REPLY_MAX_SIZE 300

typedef void data_handler_func_t(const char* data_str, int data_len);
typedef void connection_handler_func_t(int32_t event_id);

void websocket_init(connection_handler_func_t *connection_handler_func, data_handler_func_t *data_handler_func);
void websocket_send_text_async(char* data);
void websocket_connect(connection_handler_func_t *connection_handler, data_handler_func_t *data_handler);
