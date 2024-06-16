#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_http_server.h>
#include <esp_http_client.h>

extern uint16_t days_interval;
extern uint16_t hours_interval;
extern uint64_t watering_duration;
extern time_t incr_time;

void save_new_time_data(char *buf, httpd_req_t *req);
void update_curr_time(void);
void get_data_values(void);
void update_incr_time(void);
