#include "board.h"
#include "alerts_config.hpp"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const gpio_num_t buzzer_gpio = GPIO_NUM_6;

static esp_err_t write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value)
{
    const uint8_t bytes[] = {reg, value};
    return i2c_master_transmit(device, bytes, sizeof(bytes), 100);
}

esp_err_t alerts_board_start(void)
{
    const gpio_config_t buzzer_config = {
        .pin_bit_mask = 1ULL << buzzer_gpio,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(buzzer_gpio, 0), "board", "Buzzer idle level");
    ESP_RETURN_ON_ERROR(gpio_config(&buzzer_config), "board", "Buzzer output");

    i2c_master_bus_handle_t bus = NULL;
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_8,
        .scl_io_num = GPIO_NUM_9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), "board", "I2C bus");
    i2c_master_dev_handle_t expander = NULL;
    const i2c_device_config_t device_config = {
        .device_address = 0x24,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_config, &expander), "board", "I/O expander");
    ESP_RETURN_ON_ERROR(write_register(expander, 0x02, 0xff), "board", "I/O mode");
    uint8_t outputs = 0xdf;
    ESP_RETURN_ON_ERROR(write_register(expander, 0x03, outputs), "board", "LCD power");
    const gpio_config_t interrupt_config = {
        .pin_bit_mask = 1ULL << GPIO_NUM_4,
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&interrupt_config), "board", "Touch IRQ mode");
    outputs &= (uint8_t)~(1U << 1);
    ESP_RETURN_ON_ERROR(write_register(expander, 0x03, outputs), "board", "Touch reset low");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_4, 0), "board", "Touch address");
    vTaskDelay(pdMS_TO_TICKS(100));
    outputs |= 1U << 1;
    ESP_RETURN_ON_ERROR(write_register(expander, 0x03, outputs), "board", "Touch reset high");
    vTaskDelay(pdMS_TO_TICKS(200));

    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const uint8_t buffers = esp_lv_adapter_get_required_frame_buffer_count(
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB, rotation);
    esp_lcd_panel_handle_t panel = NULL;
    const esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 24000000, .h_res = 1024, .v_res = 600,
            .hsync_pulse_width = 162, .hsync_back_porch = 152, .hsync_front_porch = 48,
            .vsync_pulse_width = 45, .vsync_back_porch = 13, .vsync_front_porch = 3,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16, .bits_per_pixel = 16,
        .num_fbs = buffers, .bounce_buffer_size_px = 1024 * 20,
        .sram_trans_align = 4, .psram_trans_align = 64,
        .hsync_gpio_num = 46, .vsync_gpio_num = 3, .de_gpio_num = 5,
        .pclk_gpio_num = 7, .disp_gpio_num = -1,
        .data_gpio_nums = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40},
        .flags.fb_in_psram = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &panel), "board", "RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), "board", "LCD initialization");
    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    touch_io_config.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &touch_io_config, &touch_io), "board", "Touch I/O");
    const esp_lcd_touch_config_t touch_config = {
        .x_max = 1024, .y_max = 600,
        .rst_gpio_num = GPIO_NUM_NC, .int_gpio_num = GPIO_NUM_4,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    esp_lcd_touch_handle_t touch = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(touch_io, &touch_config, &touch), "board", "GT911");
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 12 * 1024;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_config), "board", "LVGL adapter");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        panel, NULL, 1024, 600, rotation);
    // Partial RGB rendering allocates this buffer in internal RAM.
    // Ten lines leave more room for Wi-Fi without changing the LCD DMA buffers.
    display_config.profile.buffer_height = 10;
    display_config.profile.use_psram = true;
    lv_display_t* display = esp_lv_adapter_register_display(&display_config);
    if (!display) return ESP_ERR_NO_MEM;
    const esp_lv_adapter_touch_config_t input = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch);
    if (!esp_lv_adapter_register_touch(&input)) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), "board", "LVGL task");
    const uint8_t dimming = (uint8_t)((100 - ALERTS_BRIGHTNESS) * 255 / 100);
    return write_register(expander, 0x05, dimming);
}

esp_err_t alerts_board_beep(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(buzzer_gpio, 1), "board", "Buzzer on");
    vTaskDelay(pdMS_TO_TICKS(180));
    return gpio_set_level(buzzer_gpio, 0);
}
