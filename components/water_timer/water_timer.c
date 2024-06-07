#include <stdio.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <http_server.h>
#include <esp_tls.h>
#include <esp_sntp.h>
#include <esp_netif.h>
#include <water_timer.h>
#include <esp_http_client.h>

#define HOSE_PIN GPIO_NUM_26

void calculate_time_left(void);

static const char *TAG = "Water Timer";

TaskHandle_t time_left_calc_handle;
TaskHandle_t watering_task_handle;

bool is_watering = false;

void initialize_water_timer(void)
{   

    gpio_set_direction(HOSE_PIN, GPIO_MODE_OUTPUT);

    xTaskCreate(
                (TaskFunction_t) &calculate_time_left,
                "Time Left",
                2048,
                NULL,
                1,
                &time_left_calc_handle
            );
}

void watering_task(void)
{   
    is_watering = true;

    gpio_set_level(HOSE_PIN, 1);

    vTaskDelay(pdMS_TO_TICKS(watering_duration / 1000));

    gpio_set_level(HOSE_PIN, 0);

    is_watering = false;
}

void calculate_time_left(void)
{   
    while(1)
    {   
        if(is_watering == false)
        {
            time_t now;
            char strftime_buf[64];
            struct tm timeinfo;

            time(&now);
            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

            ESP_LOGI(TAG, "CURRENT TIME -> %s", strftime_buf);

            double seconds = difftime(now, incr_time);
            if (seconds > 0) {
                
                ESP_LOGI(TAG, "E' PASSATO");

            } else if (seconds == 0) {
                
                watering_task();
            
            } else ESP_LOGI(TAG, "Non e' ancora passato");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void stop_timers(void)
{
    vTaskDelete(time_left_calc_handle);
}