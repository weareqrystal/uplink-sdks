idf_component_register(SRCS "espidf_qrystal.cpp"
    INCLUDE_DIRS "."
    REQUIRES qrystal nvs_flash)
