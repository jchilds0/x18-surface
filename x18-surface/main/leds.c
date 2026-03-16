/*
 * leds.c 
 */

/*
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

static void x18_mcp23017_read_button(uint8_t reg, uint8_t data) {
    mcp23017_msg_t write = {.read = false, .reg = reg, .data = (data << 4)};
    x18_mcp23017_send(write);
}

static void x18_mcp23017_task(void* params) {
    mcp23017_msg_t read_a = {.read = true, .reg = MCP23017_GPIOA, .cb = x18_motor_process};
    x18_mcp23017_read(read_a);

    mcp23017_msg_t read_b = {.read = true, .reg = MCP23017_GPIOB, .cb = x18_mcp23017_read_button};
    x18_mcp23017_read(read_b);
}
*/
