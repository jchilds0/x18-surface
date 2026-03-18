/*
 * leds.c 
 */

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "main.h"

#include "esp_timer.h"

ESP_EVENT_DEFINE_BASE(EVENT_LED);

static uint8_t dig = 0;
static uint8_t seg = 0;


static void led_max7219_change(void* params) {
    max7219_msg_t led_off = {.addr = dig + 1, .data = 0};
    max7219_msg_t led_on = {.addr = dig + 1, .data = 1 << (seg + 2)};

    ESP_LOGD(TAG_LED, "updating led %x %x", led_on.addr, led_on.data);

    x18_max7219_send(led_off);
    x18_max7219_send(led_on);

    dig = (dig + (seg + 1) / 5) % 4;
    seg = (seg + 1) % 5;
}

static void led_mcp23017_read_button(void* params) {
    // mcp23017_msg_t led = {.reg = MCP23017_GPIOB, .data = 0xFF};
    //
    // x18_mcp23017_write(led);
    // return;
    ESP_LOGD(TAG_LED, "reading mcp23017 GPIOB");
    x18_mcp23017_read(MCP23017_GPIOB, EVENT_LED, EVENT_MCP23017_REPLY);
}

static void led_mcp23017_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    mcp23017_msg_t *msg = event_data;
    mcp23017_msg_t led = {.reg = msg->reg, .data = (msg->data << 4)};

    x18_mcp23017_write(led);
}

void x18_led_start(void) {
    esp_timer_create_args_t timercfg = {
        .name = "change led",
        .callback = led_max7219_change,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timercfg, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 100 * 1000));

    esp_timer_create_args_t timercfg2 = {
        .name = "read button",
        .callback = led_mcp23017_read_button,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer2;
    // ESP_ERROR_CHECK(esp_timer_create(&timercfg2, &timer2));
    // ESP_ERROR_CHECK(esp_timer_start_periodic(timer2, 100 * 1000));
    //
    // ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EVENT_LED, EVENT_MCP23017_REPLY, led_mcp23017_handler, NULL));

    ESP_LOGI(TAG_LED, "started leds");
}
