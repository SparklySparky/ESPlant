#include <stdio.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <water_timer.h>
#include <water_timer_cfg.h>

void watering_task(void* arg);
void calculate_time_left(void);
void stop_watering_callback(void* arg);

static const char *TAG = "Water Timer";

esp_timer_handle_t watering_timer;
TaskHandle_t time_left_calc_handle;

uint64_t start_time = 0;
uint64_t current_time = 0;
uint64_t time_left = 0;

uint64_t watering_interval = 20 * 1000000;

bool watering_active = false;

void initialize_water_timer(void)
{   
    gpio_set_direction(HOSE_PIN, GPIO_MODE_INPUT_OUTPUT);

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
}

void watering_task(void* arg)
{
    start_time = esp_timer_get_time();
    watering_active = true;
    
    gpio_set_level(HOSE_PIN, 1);

    // Create a timer to stop watering after 5 seconds
    esp_timer_handle_t stop_watering_timer;
    const esp_timer_create_args_t stop_watering_timer_args = {
        .callback = &stop_watering_callback,
        .name = "Stop Watering Timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&stop_watering_timer_args, &stop_watering_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(stop_watering_timer, 5000000));
}

void stop_watering_callback(void* arg)
{
    gpio_set_level(HOSE_PIN, 0);
    watering_active = false;
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
