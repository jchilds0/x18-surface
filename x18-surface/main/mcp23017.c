/*
 * mcp23017.c 
 */

#include "main.h"

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"

#include <pthread.h>
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

pthread_mutex_t i2c_lock = PTHREAD_MUTEX_INITIALIZER;

static void mcp23017_write_handler(void* event_args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    esp_err_t res;
    mcp23017_msg_t* msg = event_data;
    uint8_t buf[] = {msg->reg, msg->data};

    ESP_LOGD(TAG_MAX7219, "writing %x %x", msg->reg, msg->data);

    pthread_mutex_lock(&i2c_lock);

    res = i2c_master_transmit(mcp23017_handle, buf, 2, I2C_TIMEOUT);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error writing %x - %x: %s", msg->reg, msg->data, esp_err_to_name(res));
    }

    pthread_mutex_unlock(&i2c_lock);
}

esp_err_t x18_mcp23017_write(mcp23017_msg_t msg) {
    return esp_event_post_to(loop_handle, EVENT_MCP23017, EVENT_WRITE, &msg, sizeof(msg), EVENT_LOOP_TIMEOUT);
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

    ESP_LOGD(TAG_MAX7219, "reading %x", msg->reg);

    pthread_mutex_lock(&i2c_lock);

    res = i2c_master_transmit_receive(mcp23017_handle, &msg->reg, 1, &data.data, 1, I2C_TIMEOUT);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error reading %x: %s", msg->reg, esp_err_to_name(res));
    }

    pthread_mutex_unlock(&i2c_lock);

    ESP_LOGD(TAG_MAX7219, "sending read %x %x to %s %ld", 
        msg->reg, data.data, msg->event_base, msg->event_id
    );

    res = esp_event_post_to(loop_handle, msg->event_base, msg->event_id, &data, sizeof( data ), EVENT_LOOP_TIMEOUT);
    if (res != ESP_OK) {
        ESP_LOGE(TAG_MCP23017, "error sending reg %x to %s %ld: %s", 
            msg->reg, msg->event_base, msg->event_id, esp_err_to_name(res)
        );
    }
}

esp_err_t x18_mcp23017_read(uint8_t reg, esp_event_base_t event_base, int32_t event_id) {
    read_msg_t msg = {.reg = reg, .event_base = event_base, .event_id = event_id};
    return esp_event_post_to(loop_handle, EVENT_MCP23017, EVENT_READ, &msg, sizeof(msg), EVENT_LOOP_TIMEOUT);
}

