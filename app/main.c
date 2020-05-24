
#include "kk_os_header.h"
#include "at_client.h"
#include "socket_service.h"
#include "gprs.h"
#include "gps.h"
#include "sensor_service.h"

void timer0_1s_proc(void *arg , void *arg1)
{
    //static u32 ticks = 0;

    //service_log_timer_proc();
    
    //LOG(LEVEL_INFO,"Runing ticks %d",clock());
    
}

void timer1_10ms_proc(void *arg , void *arg1)
{
    //service_log_timer_proc();
    //led_serivce_time_proc();
    //gps_service_timer_proc();
    sensor_service_proc();
    
}

#if 0
void main_task(void * prg)
{
    while(1)
    {
        gprs_timer_proc();
        
        kk_os_sleep(10);
    }
}

void void_task(void * prg)
{
    while(1)
    {        
        kk_os_sleep(10);
    }
}
#endif

int main(void)
{
    u16 i = 0;
    u8  *ptr = (u8 *)&i;

    for(i = 0 ; _board_init[i] != NULL ; i++)
    {
        _board_init[i]();
    }

    kk_os_init();

    service_log_init();

    config_service_create();

    LOG(LEVEL_INFO,"Start up(%s)..",*ptr>0?"little ed":"big ed");

    gprs_create();

    sensor_service_create();

    led_service_create();

    at_client_init();

    kk_os_timer_start(TIMER_LOG_DBG,timer1_10ms_proc,10);

    kk_os_create_task(socket_task , 4096 , SOCKET_TASK_PRIO);

    kk_os_create_task(gps_task,1024,GPS_TASK_PRIO);

    kk_os_create_task(at_resp_service_proc,512,AT_RESP_TASK_PRIO);

    kk_os_startup();

}

