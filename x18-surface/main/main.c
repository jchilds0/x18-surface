/*
 * main.c
 *
 */

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

#include <esp_log.h>
#include <esp_err.h>

#include <hal/spi_types.h>

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "hal/gpio_types.h"
#include "rom/ets_sys.h"
#include "portmacro.h"

#define LOW     0
#define HIGH    1

#define TAG_MAX7219      "[MAX7219]"

#define VSPI_MISO                 GPIO_NUM_19
#define VSPI_MOSI                 GPIO_NUM_23
#define VSPI_SCK                  GPIO_NUM_18
#define VSPI_SS                   GPIO_NUM_5

#define MAX7219_DECODE_MODE          0x09
#define MAX7219_INTENSITY            0x0a
#define MAX7219_SCAN_LIMIT           0x0b
#define MAX7219_SHUTDOWN             0x0c
#define MAX7219_DISPLAY_TEST         0x0f

spi_device_handle_t max7219_spi;

static void max7219_init(void);
static void max7219_message(uint8_t addr, uint8_t data);

static void sleep(unsigned int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void app_main(void) {
    max7219_init();

    max7219_message(MAX7219_DISPLAY_TEST, 0x01);

    sleep(1000);

    max7219_message(MAX7219_DISPLAY_TEST, 0x00);

    sleep(10);

    max7219_message(MAX7219_SCAN_LIMIT, 0x07);
    max7219_message(MAX7219_DECODE_MODE, 0x00);
    max7219_message(MAX7219_INTENSITY, 0x0F);
    max7219_message(MAX7219_SHUTDOWN, 0x01);

    sleep(10);

    for (int i = 1; i <= 8; i++) {
        max7219_message(i, 0x00);
    }

    uint8_t digits[] = {1, 2, 3, 4};

    for (;;) {
        for (uint8_t i = 0; i < sizeof(digits); i++) {
            for (uint8_t seg = 2; seg < 7; seg++) {
                //ESP_LOGI(TAG_MAX7219, "digit %d, segment %d", digits[i], seg);

                max7219_message(digits[i], 1 << seg);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                max7219_message(digits[i], 0);
            }
        }
    }
}

static void max7219_init(void) {
    spi_bus_config_t buscfg = {
        .mosi_io_num = VSPI_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = VSPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = VSPI_SS,
        .queue_size = 1
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &max7219_spi));

    ESP_LOGI(TAG_MAX7219, "init complete");
}

static void max7219_message(uint8_t addr, uint8_t data) {
    const uint8_t buf[] = {addr, data};

    spi_transaction_t t = {
        .length = 8 * sizeof(buf),
        .tx_buffer = buf,
    };

    ESP_ERROR_CHECK(spi_device_transmit(max7219_spi, &t));
}

