idf_component_register(
    SRCS "main.cpp" "board.c" "network.cpp" "http.cpp" "ota.cpp"
         "../src/model.cpp" "../src/primary.cpp" "../src/reserve.cpp"
         "../src/engine.cpp" "../src/demo.cpp" "../src/time.cpp"
         "../src/ui.cpp" "../src/ui_map.cpp" "../src/ui_details.cpp"
         "../generated/map_assets.cpp"
         "../generated/ui_font_18.c" "../generated/ui_font_22.c" "../generated/ui_font_30.c"
    INCLUDE_DIRS "../include" "${CMAKE_BINARY_DIR}/config"
    REQUIRES esp_lcd esp_driver_gpio esp_driver_i2c esp_wifi esp_netif esp_event
             esp_http_client esp_https_ota app_update nvs_flash json esp_timer lwip mbedtls
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
