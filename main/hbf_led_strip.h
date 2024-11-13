
typedef enum {
    LSA_RUNNING_BLUE,
    LSA_FLASHING_GREEN,
} led_strip_animation_type_t;


#define HBF_RGB_OFF 0

#define HBF_RGB_DIM_RED    0x0A0000
#define HBF_RGB_DIM_GREEN  0x000A00
#define HBF_RGB_DIM_BLUE   0x00000A
#define HBF_RGB_DIM_YELLOW 0x0A0400

#define HBF_RGB_BRIGHT_RED    0xFF0000
#define HBF_RGB_BRIGHT_GREEN  0x00FF00
#define HBF_RGB_BRIGHT_BLUE   0x0000FF
#define HBF_RGB_BRIGHT_YELLOW 0xFF6000

void hbf_led_strip_init(void);
void hbf_led_strip_set_same_color(uint32_t rgb, int led_count);
void hbf_led_strip_set_same_color_offset(uint32_t rgb, int led_count, int led_start);
void hbf_led_strip_red(void);
void hbf_led_strip_green(void);
void hbf_led_strip_blue(void);
void hbf_led_strip_off(void);
void hbf_led_strip_test_meter(void);
void hbf_led_strip_rgb(uint32_t rgb, int count);

void hbf_led_strip_start_animation(led_strip_animation_type_t led_strip_animation_type);
void hbf_led_strip_stop_animation();