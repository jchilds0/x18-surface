/*
 * mcp23017.c 
 */

#include "driver/i2c_master.h"

#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_log.h"

#include "esp_timer.h"
#include "main.h"
#include "portmacro.h"

static void x18_mcp23017_handle_queue(void* params);

static i2c_master_dev_handle_t mcp23017_handle;
static QueueHandle_t mcp23017_q;

ESP_EVENT_DEFINE_BASE(EVENT_MCP23017);

void x18_mcp23017_init(i2c_master_bus_handle_t bus_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MCP23017_DEVICE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &mcp23017_handle));

    /* set IOCON */
    uint8_t iocon[] = {MCP23017_IOCON, 0b00100000};
    ESP_ERROR_CHECK(i2c_master_transmit(mcp23017_handle, iocon, sizeof(iocon), I2C_MASTER_TIMEOUT_MS));

    /* set IODIR */
    uint8_t iodirA[] = {MCP23017_IODIRA, 0x0F};
    ESP_ERROR_CHECK(i2c_master_transmit(mcp23017_handle, iodirA, sizeof(iodirA), I2C_MASTER_TIMEOUT_MS));

    uint8_t iodirB[] = {MCP23017_IODIRB, 0x0F};
    ESP_ERROR_CHECK(i2c_master_transmit(mcp23017_handle, iodirB, sizeof(iodirB), I2C_MASTER_TIMEOUT_MS));

    /* queue handler */
    mcp23017_q = xQueueCreate(10, sizeof( mcp23017_msg_t ));

    TaskHandle_t task_handle = NULL;
    xTaskCreate(x18_mcp23017_handle_queue, "max7219 queue handler", STACK_SIZE, NULL, 2, &task_handle);

    ESP_LOGI(TAG_MCP23017, "init complete");
}

void x18_mcp23017_start(void) {
    esp_timer_create_args_t timercfg = {
        .name = "read_motors",
        .callback = x18_mcp23017_read,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timercfg, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 5 * 1000));
}

static void x18_mcp23017_handle_queue(void* params) {
    esp_err_t res;
    mcp23017_msg_t msg;
    
    for (;;) {
        if (xQueueReceive(mcp23017_q, &msg, portMAX_DELAY)) {
            if (msg.read) {
                res = i2c_master_transmit_receive(mcp23017_handle, &msg.reg, 1, &msg.data, 1, portMAX_DELAY);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG_MAX7219, "error reading %x: %s", msg.reg, esp_err_to_name(res));
                }

                if (msg.cb != NULL) {
                    msg.cb(msg.reg, msg.data);
                }
            } else {
                uint8_t buf[] = {msg.reg, msg.data};
                res = i2c_master_transmit(mcp23017_handle, buf, 2, portMAX_DELAY);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG_MAX7219, "error writing %x - %x: %s", msg.reg, msg.data, esp_err_to_name(res));
                }
            }
        }
    }
}

void x18_mcp23017_send(mcp23017_msg_t msg) {
    xQueueSend(mcp23017_q, &msg, portMAX_DELAY);
}

static void x18_mcp23017_read_button(uint8_t reg, uint8_t data) {
    mcp23017_msg_t write = {.read = false, .reg = reg, .data = (data << 4)};
    x18_mcp23017_send(write);
}

void x18_mcp23017_read(void* params) {
    mcp23017_msg_t read_a = {.read = true, .reg = MCP23017_GPIOA, .cb = x18_motor_process};
    x18_mcp23017_send(read_a);

    mcp23017_msg_t read_b = {.read = true, .reg = MCP23017_GPIOB, .cb = x18_mcp23017_read_button};
    x18_mcp23017_send(read_b);
}

