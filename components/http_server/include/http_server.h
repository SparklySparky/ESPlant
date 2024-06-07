#include <esp_http_server.h>
#include <esp_http_client.h>

void setup_wifi(void);
httpd_handle_t setup_server(void);

extern uint64_t watering_duration;
extern time_t incr_time;

