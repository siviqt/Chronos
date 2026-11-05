idf_component_register(SRCS "src/format.c" "src/worker.c" "src/nokia.c" "src/vfd.c" "src/vfd_transport.c"
    INCLUDE_DIRS "include" PRIV_REQUIRES nokia5110 badgeteam__badge-bsp esp_driver_i2c freertos)
