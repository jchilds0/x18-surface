FILE(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_driver_i2c esp_timer 
    INCLUDE_DIRS "")
