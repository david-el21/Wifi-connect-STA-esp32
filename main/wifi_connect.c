#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include <string.h>
#include "ws2812_control.h"

#define MAX_RETRY 5
#define gpio_num 38
static const char *TAG = "WiFiConnect";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
// SSID si PASSWORD declarate in fisierul Main.c
static char saved_ssid[33];    // max SSID length + null
static char saved_pass[65];    // max pass length + null

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) { // evenimentul de start al statiei WiFi
        esp_wifi_connect(); // Incepe conexiunea la WiFi
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++; // incrementam numarul de incercari
            ESP_LOGI(TAG, "Retry %d connecting to SSID: %s", s_retry_num, saved_ssid);
            ws2812_set_color(0,182,255);
            vTaskDelay(pdMS_TO_TICKS(1000));    //  Se asteapta conexiunea indicata de o lumina mov
            ws2812_set_color(0,0,0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); // daca s-au depasit incercarile de conectare, setam bitul de esec
        }
        // In caz de realizare a conexiunii, se aloca IP-ul conectat pentru device la router si se afiseaza
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip)); //*
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_connect(const char *ssid, const char *password)
{   
    // Initialize WS2812 on GPIO
    ws2812_init(gpio_num); // Initializam WS2812 pe GPIO-ul specificat
    ws2812_set_color(0,0,255);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Asteptam 1 secunda pentru a vedea culoarea albastra
    /**/

    // Salvam sirurile de caractere : SSID si PASSWORD pentru a le afisa in event_handler
    strncpy(saved_ssid, ssid, sizeof(saved_ssid));
    strncpy(saved_pass, password, sizeof(saved_pass));
    ws2812_set_color(182,255,0); // Aprinde LED galben dupa ce s-au salvat SSID si PASSWORD
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    /**/
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init()); // Initializam netif-ul ESP
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Initializam loop-ul de evenimente ESP
    esp_netif_create_default_wifi_sta(); // Cream un netif implicit pentru WiFi station

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK // Setam modul de autentificare la WPA2_PSK
            //
            //      Linia de mai sus permite conectarea la retele cu WPA2_PSK
            //      Pentru a realiza conexiunea la retele cu WPA3_PSK, trebuie sa schimbam linia de mai sus in:
            //
            //      .threshold.authmode = WIFI_AUTH_WPA3_PSK //-> Setam modul de autentificare la WPA3_PSK
            //
            //      Pentru a realiza conexiunea la retele cu WPA2_WPA3_PSK, trebuie sa schimbam linia de mai sus in:
            //
            //      .threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK //-> Setam modul de autentificare la WPA2_WPA3_PSK
            
            // .bssid_set = false, // Nu setam BSSID-ul, lasam routerul sa ne aloce unul

        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)); // copiem SSID-ul in structura wifi_config
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)); // copiem parola in structura wifi_config

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // verifica daca este setat modul WiFi in mod station
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // verifica daca e pornit WiFi-ul

    ESP_LOGI(TAG, "Trying to connect to SSID: %s", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(15000)); // timeout de 15s

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Connected to SSID: %s", ssid);
        ws2812_set_color(255,0,0); // Setam culoarea verde pentru succes
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ Failed to connect to SSID: %s", ssid);
        ws2812_set_color(0,255,0); // Setam culoarea rosie pentru esec
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        ESP_LOGE(TAG, "⏰ Timeout while trying to connect to SSID: %s", ssid);
        ws2812_set_color(255,255,255); // Setam culoarea alba pentru timeout
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
