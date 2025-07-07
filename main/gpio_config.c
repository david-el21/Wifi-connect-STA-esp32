#include "driver/gpio.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include <stdio.h>


// Funcție pentru configurarea unui GPIO ca ieșire
void configure_LED_output(int gpio_num) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),  // Selectează GPIO-ul
        .mode = GPIO_MODE_OUTPUT,           // Setează ca ieșire
        .pull_up_en = GPIO_PULLUP_DISABLE,  // Dezactivează pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Dezactivează pull-down
        .intr_type = GPIO_INTR_DISABLE      // Fără întreruperi
    };
    gpio_config(&io_conf);
    printf("GPIO %d configurat ca ieșire.\n", gpio_num);
}
// Funcite pentru configurarea unui GPIO ca intrare
// Aceasta funcție configurează un GPIO specificat ca intrare, cu pull-up activ
void configure_BUTTON_input(int gpio_num) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),  // Selectează GPIO-ul
        .mode = GPIO_MODE_INPUT,           // Setează ca intrare
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Activează pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Dezactivează pull-down
        .intr_type = GPIO_INTR_DISABLE      // Fără întreruperi
    };
    gpio_config(&io_conf);
    printf("GPIO %d configurat ca intrare.\n", gpio_num);
}
// Configurare ADC2 ADC_ATTEN_NUM se ajusteaza pentru a mari plaja tensiunii masurate , 
// ADC_CHANNEL este canalul ADC-ului alocat pentru pinul respectiv 
