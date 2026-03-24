#include <string.h>
#include <sys/param.h>
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "driver/ledc.h"
#include "driver/gpio.h"

static const char *TAG = "RC_CONTROLLER";
#define PORT 4210

// Pin Assignments
#define L_EN_PIN    5
#define R_EN_PIN    19
#define LPWM_PIN    23
#define RPWM_PIN    18
#define LED_GPIO    2

// PWM Configuration
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT 
#define LEDC_FREQUENCY          (5000)
#define MAX_DUTY                8191 // 100% Speed (2^13 - 1)

#define LEDC_CHANNEL_L          LEDC_CHANNEL_0
#define LEDC_CHANNEL_R          LEDC_CHANNEL_1

void hardware_init(void) {
    // GPIO Init for Enable and LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << L_EN_PIN) | (1ULL << R_EN_PIN) | (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0, .pull_down_en = 0, .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // PWM Timer Init
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE, .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES, .freq_hz = LEDC_FREQUENCY, .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // PWM Channels Init
    ledc_channel_config_t lpwm_ch = {
        .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL_L,
        .timer_sel = LEDC_TIMER, .gpio_num = LPWM_PIN, .duty = 0, .hpoint = 0
    };
    ledc_channel_config(&lpwm_ch);

    ledc_channel_config_t rpwm_ch = {
        .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL_R,
        .timer_sel = LEDC_TIMER, .gpio_num = RPWM_PIN, .duty = 0, .hpoint = 0
    };
    ledc_channel_config(&rpwm_ch);

    // Set Driver Enable HIGH
    gpio_set_level(L_EN_PIN, 1);
    gpio_set_level(R_EN_PIN, 1);
}

void set_motor(int speed) {
    if (speed > 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_L, speed);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_R, 0);
    } else if (speed < 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_L, 0);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_R, -speed);
    } else {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_L, 0);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_R, 0);
    }
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_L);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_R);
}

void blink_status(void) {
    for(int i = 0; i < 4; i++) {
        gpio_set_level(LED_GPIO, i % 2);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Device Connected: " MACSTR, MAC2STR(event->mac));
    }
}

void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_RC_Controller",
            .ssid_len = strlen("ESP32_RC_Controller"),
            .channel = 1,
            .password = "password1234",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
}

static void udp_server_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            uint8_t input = rx_buffer[0];
            switch (input) {
                case 1: 
                    set_motor(MAX_DUTY); 
                    ESP_LOGI(TAG, "Forward - 100%%"); 
                    break;
                case 2: 
                case 4: 
                    set_motor(0); 
                    ESP_LOGI(TAG, "Stop"); 
                    break;
                case 3: 
                    set_motor(-MAX_DUTY); 
                    ESP_LOGI(TAG, "Reverse - 100%%"); 
                    break;
                default: break;
            }
            blink_status();
        }
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    hardware_init();
    wifi_init_softap();
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}