/*
 * mcp23017.c 
 */

#include "main.h"

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"

#include "portmacro.h"
#include <stdint.h>

static void mcp23017_write_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void mcp23017_read_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data);

static i2c_master_dev_handle_t mcp23017_handle;

ESP_EVENT_DEFINE_BASE(EVENT_MCP23017);

void x18_mcp23017_init(i2c_master_bus_handle_t bus_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MCP23017_DEVICE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &mcp23017_handle));

    /* set IOCON */
    mcp23017_msg_t iocon = {.reg = MCP23017_IOCON, .data = 0b00100000};
    ESP_ERROR_CHECK(x18_mcp23017_write(iocon));

    /* set IODIR */
    mcp23017_msg_t iodirA = {.reg = MCP23017_IODIRA, .data = 0x0F};
    ESP_ERROR_CHECK(x18_mcp23017_write(iodirA));

    mcp23017_msg_t iodirB = {.reg = MCP23017_IODIRB, .data = 0x0F};
    ESP_ERROR_CHECK(x18_mcp23017_write(iodirB));

    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EVENT_MCP23017, EVENT_READ, mcp23017_read_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EVENT_MCP23017, EVENT_WRITE, mcp23017_write_handler, NULL));

    ESP_LOGI(TAG_MCP23017, "init complete");
}

static void mcp23017_write_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    esp_err_t res;
    mcp23017_msg_t* msg = event_data;
    uint8_t buf[] = {msg->reg, msg->data};

    res = i2c_master_transmit(mcp23017_handle, buf, 2, portMAX_DELAY);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error writing %x - %x: %s", msg->reg, msg->data, esp_err_to_name(res));
    }
}

esp_err_t x18_mcp23017_write(mcp23017_msg_t msg) {
    return esp_event_post_to(loop_handle, EVENT_MCP23017, EVENT_WRITE, &msg, sizeof(msg), portMAX_DELAY);
}

typedef struct read_msg_s {
    uint8_t reg;
    esp_event_base_t event_base;
    int32_t event_id;
} read_msg_t;

static void mcp23017_read_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    esp_err_t res;
    read_msg_t* msg = event_data;
    mcp23017_msg_t data = {.reg = msg->reg};

    res = i2c_master_transmit_receive(mcp23017_handle, &msg->reg, 1, &data.data, 1, portMAX_DELAY);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error reading %x: %s", msg->reg, esp_err_to_name(res));
    }

    res = esp_event_post_to(loop_handle, msg->event_base, msg->event_id, &data, sizeof( data ), portMAX_DELAY);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error sending reg %x to %s %ld: %s", msg->reg, msg->event_base, msg->event_id, esp_err_to_name(res));
    }
}

esp_err_t x18_mcp23017_read(uint8_t reg, esp_event_base_t event_base, int32_t event_id) {
    read_msg_t msg = {.reg = reg, .event_base = event_base, .event_id = event_id};
    return esp_event_post_to(loop_handle, EVENT_MCP23017, EVENT_READ, &msg, sizeof(msg), portMAX_DELAY);
}

