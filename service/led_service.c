
//led service
#include "led_service.h"
#include "gpio_board_config.h"
#include "bit_band_define.h"

#define GPS_LED_RCC_APB RCC_APB2Periph_GPIOA
#define GPS_LED_BASE GPIOA
#define GPS_LED_PIN  GPIO_Pin_8
#define GPS_LED      PAout(8)

#define NET_LED_RCC_APB RCC_APB2Periph_GPIOC
#define NET_LED_BASE GPIOC
#define NET_LED_PIN  GPIO_Pin_9
#define NET_LED      PCout(9)

#define LED_LIGHT    1

#define LED_TIMER_FQ  5
#define LED_FAST_FLASH_INTERVAL 200/LED_TIMER_FQ
#define LED_SLOW_FLASH_INTERVAL 1000/LED_TIMER_FQ

typedef struct
{
    LedFlashTypeEnum gps_flash;

    LedFlashTypeEnum net_flash;

    u16 gps_led_timer;

    u16 net_led_timer;
    
}LedStateControlStruct;

LedStateControlStruct s_led;

void led_service_create(void)
{
    gpio_type_set(PA8,GPIO_Mode_Out_PP);
    GPS_LED = !LED_LIGHT;

    gpio_type_set(PC9,GPIO_Mode_Out_PP);
    NET_LED = !LED_LIGHT;

    s_led.gps_flash = LED_OFF;
    s_led.net_flash = LED_OFF;
    s_led.gps_led_timer = 0;
    s_led.net_led_timer = 0;
    
}


//此函数在中断中调用10ms一次
void led_serivce_time_proc(void)
{
    
    if(s_led.gps_flash == LED_FAST_FLASH)
    {
        if(++s_led.gps_led_timer >= LED_FAST_FLASH_INTERVAL)
        {
            s_led.gps_led_timer = 0;
            GPS_LED = !GPS_LED;
        }
    }
    else if(s_led.gps_flash == LED_SLOW_FLASH)
    {
        if(++s_led.gps_led_timer >= LED_SLOW_FLASH_INTERVAL)
        {
            s_led.gps_led_timer = 0;
            GPS_LED = !GPS_LED;
        }
    }

    if(s_led.net_flash == LED_FAST_FLASH)
    {
        if(++s_led.net_led_timer >= LED_FAST_FLASH_INTERVAL)
        {
            s_led.net_led_timer = 0;
            NET_LED = !NET_LED;
        }
    }
    else if(s_led.net_flash == LED_SLOW_FLASH)
    {
        if(++s_led.net_led_timer >= LED_SLOW_FLASH_INTERVAL)
        {
            s_led.net_led_timer = 0;
            NET_LED = !NET_LED;
        }
    }
}

void led_service_gps_set(LedFlashTypeEnum type)
{
    if(s_led.gps_flash ^ type)
    {
        s_led.gps_flash = type;
        
        if(s_led.gps_flash == LED_OFF)
        {
            GPS_LED = !LED_LIGHT;
        }
        else if(s_led.gps_flash == LED_ON)
        {
            GPS_LED = LED_LIGHT;
        }
    }
}

void led_service_net_set(LedFlashTypeEnum type)
{
    if(s_led.net_flash ^ type)
    {
        s_led.net_flash = type;
        
        if(s_led.net_flash == LED_OFF)
        {
            NET_LED = !LED_LIGHT;
        }
        else if(s_led.net_flash == LED_ON)
        {
            NET_LED = LED_LIGHT;
        }
    }
}


void led_service_destory(void)
{
    
    s_led.gps_flash = LED_OFF;
    s_led.net_flash = LED_OFF;
    s_led.gps_led_timer = 0;
    s_led.net_led_timer = 0;

    NET_LED = !LED_LIGHT;
    GPS_LED = !LED_LIGHT;
}

