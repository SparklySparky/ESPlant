#include <stdio.h>

#include <http_server.h>
#include <water_timer.h>

void app_main(void)
{   
    setup_wifi();
    setup_server();

    initialize_water_timer();
}
