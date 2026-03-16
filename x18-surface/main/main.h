/*
 * main.h
 *
 */

#ifndef __X18_SURFACE_MAIN_H
#define __X18_SURFACE_MAIN_H

#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "freertos/idf_additions.h"
#include "hal/spi_types.h"

#include <stdint.h>

#define LOW     0
#define HIGH    1

#define STACK_SIZE    4096

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

#define SLEEP(ms)                 (vTaskDelay((ms) / portTICK_PERIOD_MS))

ESP_EVENT_DECLARE_BASE(EVENT_MAX7219);
ESP_EVENT_DECLARE_BASE(EVENT_MCP23017);

enum {
    EVENT_WRITE,
    EVENT_READ,
};

extern esp_event_loop_handle_t loop_handle;

/* leds.c */
typedef struct max7219_msg_s {
    uint8_t addr;
    uint8_t data;
} max7219_msg_t;

void x18_max7219_init(spi_host_device_t spi_host);
void x18_max7219_start();
esp_err_t x18_max7219_send(max7219_msg_t msg);

typedef struct button_s {
    uint8_t addr;
    uint8_t data;
} button_t;

/* mcp23017.c */
typedef struct mcp23017_msg_s {
    uint8_t reg;
    uint8_t data;
} mcp23017_msg_t;

void x18_mcp23017_init(i2c_master_bus_handle_t bus_handle);
esp_err_t x18_mcp23017_write(mcp23017_msg_t);
esp_err_t x18_mcp23017_read(uint8_t reg, esp_event_base_t event_base, int32_t event_id);

/* motors.c */
typedef struct motor_s {
    int position;
    int target;
} motor_t;

void x18_motor_init();
void x18_motor_process(uint8_t reg, uint8_t data);

#endif // !__X18_SURFACE_MAIN_H

