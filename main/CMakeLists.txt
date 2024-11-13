set(srcs
    "trigger.c" 
    "main.c" 
    "websocket.c"
    "hbf_util.c"
    "seven_seg.c"
    "isr_handlers.c"
    "led_strip_encoder.c"
    "hbf_led_strip.c"
    "wifi_fixed_config.c")

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
)