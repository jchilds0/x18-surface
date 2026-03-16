/*
 * main.c
 *
 */

#include "main.h"

#include "driver/i2c_master.h"
#include "esp_event_base.h"
#include "freertos/idf_additions.h"

#include <esp_timer.h>
#include <esp_event.h>

esp_event_loop_handle_t loop_handle;

void app_main(void) {
    esp_event_loop_args_t loop_args = {
        .queue_size = 1,
        .task_name = "main loop",
        .task_priority = tskIDLE_PRIORITY,
        .task_stack_size = STACK_SIZE,
        .task_core_id = 1,
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &loop_handle));

    /* i2c bus init */
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t i2c_mst_cfg = {
        .i2c_port = I2C_NUM_0,
        .clk_source = I2C_CLK_SRC,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_cfg, &bus_handle));

    /* spi bus init */
    spi_host_device_t spi_host = SPI3_HOST;
    spi_bus_config_t buscfg = {
        .mosi_io_num = VSPI_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = VSPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO));

    /* initialise components */
    x18_max7219_init(spi_host);
    x18_max7219_start();

    for (;;) {
        SLEEP(1000);
    }
    x18_mcp23017_init(bus_handle);
    x18_motor_init();

    /* start components */
    x18_max7219_start();
    x18_mcp23017_start();

    for (;;) {
        SLEEP(1000);
    }
}

