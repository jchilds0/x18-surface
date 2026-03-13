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

#include "hal/gpio_types.h"
#include "portmacro.h"

#define LOW     0
#define HIGH    1

#define TAG_MAX7219               "[MAX7219]"
#define TAG_MCP23017              "[MCP23017]"

#define I2C_CLK_SRC               I2C_CLK_SRC_DEFAULT
#define SPI_CLK_SRC               SPI_CLK_SRC_DEFAULT

#define VSPI_MISO                 GPIO_NUM_19
#define VSPI_MOSI                 GPIO_NUM_23
#define VSPI_SCK                  GPIO_NUM_18
#define VSPI_SS                   GPIO_NUM_5

#define MAX7219_DECODE_MODE       0x09
#define MAX7219_INTENSITY         0x0a
#define MAX7219_SCAN_LIMIT        0x0b
#define MAX7219_SHUTDOWN          0x0c
#define MAX7219_DISPLAY_TEST      0x0f

#define I2C_MASTER_SDA_IO         GPIO_NUM_21
#define I2C_MASTER_SCL_IO         GPIO_NUM_22
#define I2C_MASTER_FREQ_HZ        400 * 1000
#define I2C_MASTER_TIMEOUT_MS     10

#define MCP23017_DEVICE_ADDR      0x20
#define MCP23017_IODIRA           0x00
#define MCP23017_IODIRB           0x01
#define MCP23017_IOCON            0x0a
#define MCP23017_GPIOA            0x12
#define MCP23017_GPIOB            0x13

/* leds.c */
typedef struct led_s {
    uint8_t digit;
    uint8_t segments;
} led_t;

static spi_device_handle_t max7219_spi;
static QueueHandle_t led_queue;

void max7219_init(void);
void max7219_message(uint8_t addr, uint8_t data);
void max7219_change_led(void* params);

/* motors.c */
typedef struct motor_s {
    int position;
    int target;
} motor_t;

static motor_t motors[2];
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

void mcp23017_init(void);
void mcp23017_read(void* params);

static void sleep(unsigned int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void app_main(void) {
    i2c_master_bus_config_t i2c_mst_cfg = {
        .i2c_port = I2C_NUM_0,
        .clk_source = I2C_CLK_SRC,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_cfg, &bus_handle));

    max7219_init();
    mcp23017_init();

    esp_timer_create_args_t timercfg = {
        .name = "read_motors",
        .callback = mcp23017_read,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timercfg, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 5 * 1000));

    esp_timer_create_args_t timercfg2 = {
        .name = "change_led",
        .callback = max7219_change_led,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer2;
    ESP_ERROR_CHECK(esp_timer_create(&timercfg2, &timer2));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer2, 100 * 1000));

    led_t led;
    for (;;) {
        if (xQueueReceive(led_queue, &led, portMAX_DELAY)) {
            max7219_message(led.digit, led.segments);
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
        .clock_source = SPI_CLK_SRC,
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

    led_queue = xQueueCreate(10, sizeof( led_t ));
}

void max7219_message(uint8_t addr, uint8_t data) {
    const uint8_t buf[] = {addr, data};

    spi_transaction_t t = {
        .length = 8 * sizeof(buf),
        .tx_buffer = buf,
    };

    ESP_ERROR_CHECK(spi_device_transmit(max7219_spi, &t));
}

void max7219_change_led(void* params) {
    static uint8_t dig = 0;
    static uint8_t seg = 0;

    led_t led_off = {.digit = dig + 1, .segments = 0};
    led_t led_on = {.digit = dig + 1, .segments = 1 << (seg + 2)};

    xQueueSend(led_queue, &led_off, portMAX_DELAY);
    xQueueSend(led_queue, &led_on, portMAX_DELAY);

    dig = (dig + (seg + 1) / 5) % 4;
    seg = (seg + 1) % 5;
}

void mcp23017_init(void) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MCP23017_DEVICE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    /* set IOCON */
    uint8_t iocon[] = {MCP23017_IOCON, 0b00100000};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, iocon, sizeof(iocon), I2C_MASTER_TIMEOUT_MS));

    /* set IODIR */
    uint8_t iodirA[] = {MCP23017_IODIRA, 0x0F};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, iodirA, sizeof(iodirA), I2C_MASTER_TIMEOUT_MS));

    uint8_t iodirB[] = {MCP23017_IODIRB, 0x0F};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, iodirB, sizeof(iodirB), I2C_MASTER_TIMEOUT_MS));

    ESP_LOGI(TAG_MCP23017, "init complete");
}

static const int8_t transition_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

void mcp23017_read(void* params) {
    esp_err_t res;
    uint8_t ctrl_a = MCP23017_GPIOA;
    uint8_t ctrl_b = MCP23017_GPIOB;
    uint8_t reg_a = 0;
    uint8_t reg_b = 0;

    static uint8_t prev_state = 0;
    static int8_t encval = 0;

    res = i2c_master_transmit_receive(dev_handle, &ctrl_a, 1, &reg_a, 1, I2C_MASTER_TIMEOUT_MS);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error getting register a: %s", esp_err_to_name(res));
        return;
    }

    res = i2c_master_transmit_receive(dev_handle, &ctrl_b, 1, &reg_b, 1, I2C_MASTER_TIMEOUT_MS);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error getting register b: %s", esp_err_to_name(res));
        return;
    }

    uint8_t leds[] = {MCP23017_GPIOB, reg_b << 4};
    res = i2c_master_transmit(dev_handle, leds, 2, I2C_MASTER_TIMEOUT_MS);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error getting updating b: %s", esp_err_to_name(res));
        return;
    }

    uint32_t current_state = (reg_a >> 4) & 0b0011;
    uint8_t index = (prev_state << 2) | current_state;
    encval += transition_table[index];
    prev_state = current_state;

    if (encval > 3) {
        encval = 0;
        ESP_LOGI(TAG_MCP23017, "rotation: %s", "anti-clockwise");
    } else if (encval < -3) {
        encval = 0;
        ESP_LOGI(TAG_MCP23017, "rotation: %s", "clockwise");
    }
}

