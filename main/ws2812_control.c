#include "ws2812_control.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "WS2812";
static led_strip_handle_t led_strip;

esp_err_t ws2812_init(int gpio_num) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = 1,
        .flags = {
            .invert_out = false,
        },
    };
 
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // Use default clock source
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false, // Disable DMA for simplicity
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip); // Create a new RMT device for the LED strip
    if (ret != ESP_OK) { // Check if the device was created successfully
        ESP_LOGE(TAG, "Eroare inițializare WS2812: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    ESP_LOGI(TAG, "WS2812 inițializat pe GPIO %d", gpio_num);
    return ESP_OK;
}


esp_err_t ws2812_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!led_strip) return ESP_FAIL;
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, r, g, b));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    return ESP_OK;
}
