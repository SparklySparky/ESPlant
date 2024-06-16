#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/ip_addr.h>
#include <driver/gpio.h>
#include <stdlib.h>
#include <cJSON.h>
#include <esp_tls.h>
#include <esp_sntp.h>
#include <esp_netif.h>
#include <esp_http_client.h>
#include <esp_http_server.h>

#include <data_storage.h>
#include <water_timer.h>


#define WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define WIFI_PASSWORD   CONFIG_ESP_WIFI_PASSWORD
#define WIFI_CHANNEL    CONFIG_ESP_WIFI_CHANNEL
#define WIFI_MAX_STA    CONFIG_ESP_MAX_STA_CONN
#define WIFI_MAX_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "Http Server";

static int s_retry_num = 0;

static EventGroupHandle_t s_wifi_event_group;

esp_err_t get_handler(httpd_req_t *req) {
    const char response[] = "Pinged Back From ESP-Plant";
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Status REQUESTED: %s", response);
    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

esp_err_t get_time_left_handler(httpd_req_t *req) {
    uint8_t time_left = 0;
    char response_buffer[30];
    snprintf(response_buffer, sizeof(response_buffer), "%" PRIu8, time_left);
    httpd_resp_send(req, response_buffer, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Get Time Left REQUESTED: %s", response_buffer);
    return ESP_OK;
}

httpd_uri_t uri_get_time_left = {
    .uri      = "/time_left",
    .method   = HTTP_GET,
    .handler  = get_time_left_handler,
    .user_ctx = NULL
};

esp_err_t get_watering_interval_handler(httpd_req_t *req) {
    char response_buffer[30];
    uint64_t t = 20;
    snprintf(response_buffer, sizeof(response_buffer), "%" PRIu64, t);
    httpd_resp_send(req, response_buffer, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Get Watering Interval REQUESTED: %s", response_buffer);
    return ESP_OK;
}

httpd_uri_t uri_get_watering_interval = {
    .uri      = "/watering_interval",
    .method   = HTTP_GET,
    .handler  = get_watering_interval_handler,
    .user_ctx = NULL
};

esp_err_t post_update_data_handler(httpd_req_t *req) {
    stop_timers();

    char buf[200];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;
    }

    save_new_time_data(buf, req);

    return ESP_OK;
}

httpd_uri_t uri_post_update_data = {
    .uri      = "/update_data",
    .method   = HTTP_POST,
    .handler  = post_update_data_handler,
    .user_ctx = NULL
};

httpd_handle_t setup_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_get_time_left);
        httpd_register_uri_handler(server, &uri_get_watering_interval);
        httpd_register_uri_handler(server, &uri_post_update_data);
    }

    return server;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void setup_wifi(void) {   
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    update_curr_time();

    get_data_values();

    update_incr_time();
}