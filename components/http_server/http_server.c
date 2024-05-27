#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "driver/gpio.h"
#include <stdlib.h>
#include <cJSON.h>
#include "esp_tls.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include <water_timer.h>
#include <esp_http_server.h>


#define WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define WIFI_PASSWORD   CONFIG_ESP_WIFI_PASSWORD
#define WIFI_CHANNEL    CONFIG_ESP_WIFI_CHANNEL
#define WIFI_MAX_STA    CONFIG_ESP_MAX_STA_CONN
#define WIFI_MAX_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define INTERVAL_MULTIPLIER 1000000
#define WATERING_DURATION_MULTIPLIER 1000

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "Http Server";
static char output_buffer[MAX_HTTP_OUTPUT_BUFFER];
static int output_len = 0;
static int s_retry_num = 0;

static EventGroupHandle_t s_wifi_event_group;

uint64_t watering_interval = 20 * INTERVAL_MULTIPLIER;
uint16_t watering_duration = 5 * WATERING_DURATION_MULTIPLIER;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (output_len == 0) {
                // Clean the buffer in case of a new request
                memset(output_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len) {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                    output_len += copy_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_buffer[output_len] = '\0';  // Null-terminate the buffer
            ESP_LOGI(TAG, "HTTP Response: %s", output_buffer);
            output_len = 0;

            cJSON *response = cJSON_Parse(output_buffer);
            if (response == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
                break;
            }

            cJSON *datetime = cJSON_GetObjectItem(response, "datetime");
            if (datetime == NULL) {
                ESP_LOGE(TAG, "Failed to get 'datetime' from JSON response");
                cJSON_Delete(response);
                break;
            }

            cJSON *response_timezone = cJSON_GetObjectItem(response, "abbreviation");
            if (response_timezone == NULL) {
                ESP_LOGE(TAG, "Failed to get 'abbreviation' from JSON response");
                cJSON_Delete(response);
                break;
            }

            struct tm tm;
            strptime(cJSON_GetStringValue(datetime), "%Y-%m-%dT%H:%M:%S", &tm);

            time_t t = mktime(&tm);
            struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&tv, NULL);

            setenv("TZ", cJSON_GetStringValue(response_timezone), 1);
            tzset();
            
            ESP_LOGI(TAG, "RTC updated to: %s", cJSON_GetStringValue(datetime));

            cJSON_Delete(response);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

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
    char response_buffer[30];
    snprintf(response_buffer, sizeof(response_buffer), "%" PRIu64, time_left);
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
    snprintf(response_buffer, sizeof(response_buffer), "%" PRIu64, watering_interval);
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

    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;
    }

    cJSON *response = cJSON_Parse(buf);

    cJSON *data_interval = cJSON_GetObjectItem(response, "Watering_Interval");
    cJSON *data_duration = cJSON_GetObjectItem(response, "Watering_Duration");

    char *ptr;

    long int new_watering_interval = strtol(cJSON_GetStringValue(data_interval), &ptr, 10);
    long int new_watering_duration = strtol(cJSON_GetStringValue(data_duration), &ptr, 10);
    
    watering_interval = new_watering_interval * INTERVAL_MULTIPLIER;
    watering_duration = new_watering_duration * WATERING_DURATION_MULTIPLIER;

    nvs_handle_t nvs_write_strg_handle;
    esp_err_t err = nvs_open("dataStrg", NVS_READWRITE, &nvs_write_strg_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        httpd_resp_sendstr(req, "Failed to open NVS handle");
        return ESP_FAIL;
    }

    err = nvs_set_u16(nvs_write_strg_handle, "waterIntrv", new_watering_interval);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS value! Error: %s", esp_err_to_name(err));
        nvs_close(nvs_write_strg_handle);
        httpd_resp_sendstr(req, "Failed to set NVS value");
        return ESP_FAIL;
    }

    err = nvs_set_u16(nvs_write_strg_handle, "waterDurat", new_watering_duration);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS value! Error: %s", esp_err_to_name(err));
        nvs_close(nvs_write_strg_handle);
        httpd_resp_sendstr(req, "Failed to set NVS value");
        return ESP_FAIL;
    }

    err = nvs_commit(nvs_write_strg_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS! Error: %s", esp_err_to_name(err));
        nvs_close(nvs_write_strg_handle);
        httpd_resp_sendstr(req, "Failed to commit NVS");
        return ESP_FAIL;
    }

    nvs_close(nvs_write_strg_handle);

    ESP_LOGI(TAG, "Updated watering interval to %" PRIu64, watering_interval);
    ESP_LOGI(TAG, "Updated watering duration to %" PRIu16, watering_duration);

    httpd_resp_send_chunk(req, NULL, 0);

    initialize_water_timer();

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

    nvs_handle_t nvs_read_strg_handle;
    ret = nvs_open("dataStrg", NVS_READWRITE, &nvs_read_strg_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
    } else {
        uint16_t stored_watering_intrv = 0;
        ret = nvs_get_u16(nvs_read_strg_handle, "waterIntrv", &stored_watering_intrv);
        switch (ret) {
            case ESP_OK:
                watering_interval = stored_watering_intrv * INTERVAL_MULTIPLIER;
                ESP_LOGI(TAG, "Stored watering interval: %" PRIu64, watering_interval);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "No stored watering interval found, using default.");
                break;
            default:
                ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(ret));
        }

        uint16_t stored_watering_duration = 0;
        ret = nvs_get_u16(nvs_read_strg_handle, "waterDurat", &stored_watering_duration);
        switch (ret) {
            case ESP_OK:
                watering_duration = stored_watering_duration * WATERING_DURATION_MULTIPLIER;
                ESP_LOGI(TAG, "Stored watering duration: %" PRIu16, watering_duration);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "No stored watering duration found, using default.");
                break;
            default:
                ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(ret));
        }
        nvs_close(nvs_read_strg_handle);
    }

    esp_http_client_config_t config = {
        .host = "worldtimeapi.org",
        .path = "/api/timezone/Europe/Rome",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .user_data = output_buffer,  // Pass the buffer to the event handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}