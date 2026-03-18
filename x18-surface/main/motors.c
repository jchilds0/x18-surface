/*
 * motors.c 
 */

#include "main.h"

#include "esp_log.h"

void x18_motor_start(void) {

}

static const int8_t transition_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

void x18_motor_process(uint8_t reg, uint8_t data) {
    static uint8_t prev_state = 0;
    static int8_t encval = 0;

    uint32_t current_state = (data >> 4) & 0b0011;
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

