
//head for led_service.c

#ifndef __LED_SERVICE_H__
#define __LED_SERVICE_H__


typedef enum
{
    LED_OFF,

    LED_ON,

    LED_FAST_FLASH,

    LED_SLOW_FLASH
    
}LedFlashTypeEnum;

void led_service_create(void);

void led_serivce_time_proc(void);

void led_service_gps_set(LedFlashTypeEnum type);

void led_service_net_set(LedFlashTypeEnum type);

void led_service_destory(void);

#endif
