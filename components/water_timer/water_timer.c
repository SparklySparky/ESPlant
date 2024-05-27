#include <stdio.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <http_server.h>
#include "esp_tls.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include <water_timer.h>

#define HOSE_PIN GPIO_NUM_26

void watering_task(void* arg);
void calculate_time_left(void);

static const char *TAG = "Water Timer";

esp_timer_handle_t watering_timer;

TaskHandle_t time_left_calc_handle;

uint64_t start_time = 0;
uint64_t current_time = 0;
uint64_t time_left = 0;

void initialize_water_timer(void)
{   

    gpio_set_direction(HOSE_PIN, GPIO_MODE_OUTPUT);

    const esp_timer_create_args_t watering_timer_args = {
        .callback = &watering_task,
        .name = "Watering Timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&watering_timer_args, &watering_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(watering_timer, watering_interval));

    start_time = esp_timer_get_time();

    xTaskCreate(
                (TaskFunction_t) &calculate_time_left,
                "Time Left",
                2048,
                NULL,
                1,
                &time_left_calc_handle
            );

    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    
    ESP_LOGI(TAG, "The current date/time in Rome is: %s", strftime_buf);
}

void watering_task(void* arg)
{
    start_time = esp_timer_get_time();
    
    gpio_set_level(HOSE_PIN, 1);

    vTaskDelay(pdMS_TO_TICKS(watering_duration));

    gpio_set_level(HOSE_PIN, 0);
}

void calculate_time_left(void)
{
    while(1)
    {   
        current_time = esp_timer_get_time();

        time_left = ((start_time + watering_interval) - current_time) / 1000000;

        ESP_LOGI(TAG, "%llu sec", time_left);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void stop_timers(void)
{
    vTaskDelete(time_left_calc_handle);

    esp_timer_stop(watering_timer);

    esp_timer_delete(watering_timer);
}