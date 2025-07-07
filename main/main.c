#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ws2812_control.h"

#define WS2812 38 // Define the GPIO pin for WS2812 (change this to your desired GPIO number)

//  WIFI - SETTINGS
#define SSID "my_wifi_ssid" // Replace with your WiFi SSID
#define PASSWORD "my_password"
//  WiFi will be used to connect to the network and send data.

// Declaram functia wifi_connect care va fi folosita pentru a conecta ESP32 la WiFi
void wifi_connect(const char *ssid, const char *password);


void app_main(void)
{    
    ws2812_init(WS2812); // Initializam WS2812 pe un GPIO specificat (inlocuieste int gpio_num cu numarul GPIO-ului dorit)
    // Verificam daca partitia NVS este initializata, daca nu, o initializam
    // NVS (Non-Volatile Storage) este folosit pentru a stoca date
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); // Stergem partitia NVS
        ret = nvs_flash_init(); // Reinitializam partitia NVS
    }
    ESP_ERROR_CHECK(ret);

    // Initializam ssid si pass cu detaliile SSID-ului, precum parola de la router (Wifi)
    const char *my_ssid = SSID;
    const char *my_pass = PASSWORD;

   wifi_connect(my_ssid, my_pass); // Conectam la WiFi folosind SSID-ul si parola specificate
}