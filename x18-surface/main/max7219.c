/*
 * max7219.c 
 */

#include "main.h"

#include "driver/spi_master.h"

#include "esp_event.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <pthread.h>

static void max7219_change_led(void* params);
static void max7219_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data);

static spi_device_handle_t max7219_handle;

ESP_EVENT_DEFINE_BASE(EVENT_MAX7219);

void x18_max7219_init(spi_host_device_t spi_host) {
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .clock_source = SPI_CLK_SRC,
        .mode = 0,
        .spics_io_num = VSPI_SS,
        .queue_size = 1
    };

    ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &devcfg, &max7219_handle));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EVENT_MAX7219, EVENT_WRITE, max7219_handler, NULL));

    ESP_LOGI(TAG_MAX7219, "init complete");
}

void x18_max7219_start(void) {
    max7219_msg_t led;

    /* display test */
    led.addr = MAX7219_DISPLAY_TEST;
    led.data = 0x01;
    ESP_ERROR_CHECK(x18_max7219_send(led));

    SLEEP(1000);

    led.addr = MAX7219_DISPLAY_TEST;
    led.data = 0x00;
    ESP_ERROR_CHECK(x18_max7219_send(led));

    SLEEP(10);

    max7219_msg_t conf[] = {
        {.addr = MAX7219_SCAN_LIMIT, .data = 0x07},
        {.addr = MAX7219_DECODE_MODE, .data = 0x00},
        {.addr = MAX7219_INTENSITY, .data = 0x0F},
        {.addr = MAX7219_SHUTDOWN, .data = 0x01},
    };

    for (size_t i = 0; i < sizeof(conf) / sizeof(conf[0]); i++) {
        ESP_ERROR_CHECK(x18_max7219_send(led));
    }

    SLEEP(10);

    for (int i = 1; i <= 8; i++) {
        led.addr = i;
        led.data = 0x00;
        ESP_ERROR_CHECK(x18_max7219_send(led));
    }

    esp_timer_create_args_t timercfg = {
        .name = "change_led",
        .callback = max7219_change_led,
        .arg = loop_handle,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timercfg, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 100 * 1000));
}

static void max7219_change_led(void* params) {
    static uint8_t dig = 0;
    static uint8_t seg = 0;

    max7219_msg_t led_off = {.addr = dig + 1, .data = 0};
    max7219_msg_t led_on = {.addr = dig + 1, .data = 1 << seg};

    x18_max7219_send(led_off);
    x18_max7219_send(led_on);

    dig = (dig + (seg + 1) / 6) % 4;
    seg = (seg + 1) % 6;
}

pthread_mutex_t spi_lock = PTHREAD_MUTEX_INITIALIZER;

static void max7219_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    max7219_msg_t* led = event_data;
    const uint8_t buf[] = {led->addr, led->data};

    spi_transaction_t t = {
        .length = 8 * sizeof(buf),
        .tx_buffer = buf,
    };

    pthread_mutex_lock(&spi_lock);

    esp_err_t res = spi_device_transmit(max7219_handle, &t);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MAX7219, "error sending (%d %d) to max7219: %s", led->addr, led->data, esp_err_to_name(res));
    }

    pthread_mutex_unlock(&spi_lock);
}

esp_err_t x18_max7219_send(max7219_msg_t msg) {
    return esp_event_post_to(loop_handle, EVENT_MAX7219, EVENT_WRITE, &msg, sizeof(msg), EVENT_LOOP_TIMEOUT);
}
