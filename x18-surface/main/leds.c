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
    max7219_msg_t led_on = {.addr = dig + 1, .data = 1 << seg};

    ESP_LOGD(TAG_LED, "updating led %x %x", led_on.addr, led_on.data);
    x18_max7219_send(led_on);

    dig = (dig + (seg + 1) / 6) % 4;
    seg = (seg + 1) % 6;
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

    ESP_LOGI(TAG_LED, "started leds");
}
