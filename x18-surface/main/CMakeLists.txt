FILE(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi
    INCLUDE_DIRS "")
