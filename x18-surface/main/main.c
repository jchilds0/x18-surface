/*
 * main.c
 *
 */

#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>

#include <hal/spi_types.h>

#include <stdint.h>

#include "portmacro.h"

#define LOW     0
#define HIGH    1

#define TAG_MAX7219      "[MAX7219]"
#define TAG_MCP23017     "[MCP23017]"

#define VSPI_MISO                 GPIO_NUM_19
#define VSPI_MOSI                 GPIO_NUM_23
#define VSPI_SCK                  GPIO_NUM_18
#define VSPI_SS                   GPIO_NUM_5

#define MAX7219_DECODE_MODE       0x09
#define MAX7219_INTENSITY         0x0a
#define MAX7219_SCAN_LIMIT        0x0b
#define MAX7219_SHUTDOWN          0x0c
#define MAX7219_DISPLAY_TEST      0x0f

#define I2C_MASTER_SCL_IO         GPIO_NUM_22
#define I2C_MASTER_SDA_IO         GPIO_NUM_21
#define I2C_MASTER_FREQ_HZ        400 * 1000
#define I2C_MASTER_TIMEOUT_MS     10

#define MCP23017_DEVICE_ADDR      0x40

/* leds.c */
spi_device_handle_t max7219_spi;

void max7219_init(void);
void max7219_message(uint8_t addr, uint8_t data);

/* motors.c */
typedef struct motor_s {
    int position;
    int target;
} motor_t;

motor_t motors[2];
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

void mcp23017_init(void);
void mcp23017_read(void* params);

static void sleep(unsigned int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void app_main(void) {
    i2c_master_bus_config_t i2c_mst_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_cfg, &bus_handle));

    max7219_init();
    mcp23017_init();

    for (;;) {
        for (uint8_t i = 1; i < 5; i++) {
            for (uint8_t seg = 2; seg < 7; seg++) {
                //ESP_LOGI(TAG_MAX7219, "digit %d, segment %d", digits[i], seg);

                max7219_message(i, 1 << seg);
                sleep(10);
                max7219_message(i, 0);
            }
        }
    }
}

void max7219_init(void) {
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

    /* display test */
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
}

void max7219_message(uint8_t addr, uint8_t data) {
    const uint8_t buf[] = {addr, data};

    spi_transaction_t t = {
        .length = 8 * sizeof(buf),
        .tx_buffer = buf,
    };

    ESP_ERROR_CHECK(spi_device_transmit(max7219_spi, &t));
}

void mcp23017_init(void) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MCP23017_DEVICE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    esp_timer_create_args_t timercfg = {
        .name = "read_motors",
        .callback = mcp23017_read,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timercfg, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1 * 1000));

    /* set IOCON */
    uint8_t iocon[] = {0x05, 0b00100000};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, iocon, sizeof(iocon), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));

    /* set IODIR */
    uint8_t iodirA[] = {0x00, 0xFF};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, iodirA, sizeof(iodirA), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));

    uint8_t iodirB[] = {0x01, 0x00};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, iodirB, sizeof(iodirB), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
}

void mcp23017_read(void* params) {
    uint8_t reg_addr = 0;
    uint8_t data[1];

    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, sizeof(data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
    ESP_LOGI(TAG_MCP23017, "data %x", data[0]);
}
