idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES furi_core momentum_hal st7789 mtm_input
)
