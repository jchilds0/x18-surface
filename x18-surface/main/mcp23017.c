/*
 * mcp23017.c 
 */

#include "freertos/idf_additions.h"
#include "hal/gpio_types.h"
#include "main.h"

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "portmacro.h"

#include <pthread.h>
#include <stdint.h>

pthread_mutex_t i2c_lock = PTHREAD_MUTEX_INITIALIZER;

static i2c_master_dev_handle_t mcp23017_handle;
static QueueHandle_t mcp23017_intr_q;

ESP_EVENT_DEFINE_BASE(EVENT_MCP23017);

static void IRAM_ATTR mcp23017_intr_handler(void* params) {
    BaseType_t xHigherPriorityTaskWoken;
    uint8_t index = 0;

    xQueueSendFromISR(mcp23017_intr_q, &index, &xHigherPriorityTaskWoken);
}

static void mcp23017_intr_task(void* params) {
    uint8_t index;

    while (1) {
        if (xQueueReceive(mcp23017_intr_q, &index, portMAX_DELAY)) {
            mcp23017_msg_t msg;

            x18_mcp23017_read(MCP23017_GPIOB, &msg);

            ESP_LOGI(TAG_MCP23017, "intr received: %x", msg.data);
            msg.reg = MCP23017_GPIOA;

            x18_mcp23017_write(msg);
        }
    }
}

void x18_mcp23017_init(i2c_master_bus_handle_t bus_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MCP23017_DEVICE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &mcp23017_handle));

    mcp23017_msg_t conf[] = {
        {.reg = MCP23017_IOCON, .data = 0b00101100},
        {.reg = MCP23017_IPOLB, .data = 0xFF},
        {.reg = MCP23017_INTCONB, .data = 0x00},
        {.reg = MCP23017_GPINTENB, .data = 0xFF},
        {.reg = MCP23017_GPPUB, .data = 0xFF},
        {.reg = MCP23017_IODIRA, .data = 0x00},
        {.reg = MCP23017_IODIRB, .data = 0xFF},
    };

    for (size_t i = 0; i < sizeof( conf ) / sizeof( conf[0] ); i++) {
        ESP_ERROR_CHECK(x18_mcp23017_write(conf[i]));
    }

    gpio_config_t intr_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << MCP23017_INTR_B),
        .pull_up_en = false,
        .pull_down_en = false,
    };

    mcp23017_intr_q = xQueueCreate(16, sizeof( uint8_t ));

    TaskHandle_t task_handle;
    xTaskCreate(mcp23017_intr_task, "mcp23017 intr task", STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);

    ESP_ERROR_CHECK(gpio_config(&intr_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(MCP23017_INTR_B, mcp23017_intr_handler, NULL));

    mcp23017_msg_t reg_b;
    ESP_ERROR_CHECK(x18_mcp23017_read(MCP23017_GPIOB, &reg_b));

    ESP_LOGI(TAG_MCP23017, "init complete");
}

esp_err_t x18_mcp23017_write(mcp23017_msg_t msg) {
    esp_err_t res;
    uint8_t buf[] = {msg.reg, msg.data};

    pthread_mutex_lock(&i2c_lock);
    res = i2c_master_transmit(mcp23017_handle, buf, 2, I2C_TIMEOUT);
    pthread_mutex_unlock(&i2c_lock);

    ESP_LOGD(TAG_MAX7219, "write %x: %x", msg.reg, msg.data);

    return res;
}

esp_err_t x18_mcp23017_read(uint8_t reg, mcp23017_msg_t *msg) {
    esp_err_t res;
    msg->reg = reg;
    msg->data = 0;

    pthread_mutex_lock(&i2c_lock);
    res = i2c_master_transmit_receive(mcp23017_handle, &msg->reg, 1, &msg->data, 1, I2C_TIMEOUT);
    pthread_mutex_unlock(&i2c_lock);

    ESP_LOGD(TAG_MAX7219, "read %x: %x", msg->reg, msg->data);

    return res;
}

