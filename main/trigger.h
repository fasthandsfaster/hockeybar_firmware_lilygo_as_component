#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define NO_TIMEOUT -1

typedef enum {
    TE_START,
    TE_OVER,
    TE_PUSH,
    TE_HIT_BEAM_A,
    TE_HIT_BEAM_B,
    TE_HIT_BEAM_C,
    TE_UNKNOWN
} trigger_event_t;

typedef enum {
    TP_A_TO_B,
    TP_B_TO_A,
    TP_UNDEF
} trigger_path_t;

typedef enum {
    TD_A_TO_B,
    TD_B_TO_A,
    TD_UNKNOWN
} trigger_direction_t;


typedef enum {
    TS_IDLE,
    TS_WAIT_FOR_FIRST_HIT,
    TS_WAIT_FOR_SECOND_HIT
} trigger_state_t;

typedef struct TRIGGER_P_DATA {
   TickType_t time_open;
   bool silent;
   trigger_path_t path;
} trigger_evnet_parameters_t;

typedef struct TRIGGER_Q_DATA {
    trigger_event_t event;
    TickType_t event_ticks;
    trigger_evnet_parameters_t event_parameters;
    char message_id[33];
} trigger_data_t;

static char* empty_message_id = "00000000-0000-0000-0000-000000000000";

char* trigger_event_to_str(trigger_event_t e);
char* trigger_state_to_str(trigger_state_t e);
char* trigger_path_to_str(trigger_path_t e);
char* trigger_direction_to_str(trigger_direction_t e);

trigger_event_t trigger_event_str_to_enum(const char* s);
trigger_path_t trigger_path_str_to_enum(const char* s);

void trigger_add_event_to_queue(trigger_event_t trigger_event,trigger_evnet_parameters_t event_parameters, TickType_t Ticks_event_time, char* message_id);
void trigger_start_event_to_queue();
void hbf_ws_data_handler(const char* data_str, int data_len);
void hbf_ws_connection_handler(int32_t event_id);
void trigger_init(void);
char* get_timestamp_str(void);
