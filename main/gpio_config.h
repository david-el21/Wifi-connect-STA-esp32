#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "driver/gpio.h"

void configure_LED_output(int gpio_num);
void configure_BUTTON_input(int gpio_num);

#endif // GPIO_CONFIG_H