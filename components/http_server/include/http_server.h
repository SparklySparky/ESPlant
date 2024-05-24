#include <esp_http_server.h>

void setup_wifi(void);
httpd_handle_t setup_server(void);

extern uint64_t watering_interval;
extern uint16_t watering_duration;

