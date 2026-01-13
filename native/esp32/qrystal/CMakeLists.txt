idf_component_register(
    SRCS "qrystal.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_wifi mbedtls)
