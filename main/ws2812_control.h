#pragma once

#include <stdint.h>
#include "esp_err.h"

/// Inițializează WS2812 pe un GPIO dat
esp_err_t ws2812_init(int gpio_num);

/// Aprinde LED-ul cu culoarea (r, g, b)
esp_err_t ws2812_set_color(uint8_t r, uint8_t g, uint8_t b);