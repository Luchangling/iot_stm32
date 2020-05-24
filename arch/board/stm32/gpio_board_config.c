
#include "gpio_board_config.h"

#define Pin(x) ((u16)1 << x)

//STM32 RCT6 64个管脚 15个为功能管脚

static u16 s_board_gpio[] = {(Pin(0)|Pin(1)|Pin(2)|Pin(3)|Pin(4)|Pin(5)|Pin(6)|Pin(7)|Pin(8)|Pin(9)|Pin(10)|Pin(11)|Pin(12)|Pin(13)|Pin(14)|Pin(15)),
                             (Pin(0)|Pin(1)|Pin(2)|Pin(3)|Pin(4)|Pin(5)|Pin(6)|Pin(7)|Pin(8)|Pin(9)|Pin(10)|Pin(11)|Pin(12)|Pin(13)|Pin(14)|Pin(15)),
                             (Pin(0)|Pin(1)|Pin(2)|Pin(3)|Pin(4)|Pin(5)|Pin(6)|Pin(7)|Pin(8)|Pin(9)|Pin(10)|Pin(11)|Pin(12)|Pin(13)),
                             (Pin(2))}; 

static const GPIO_TypeDef* s_gpio_base[] = {GPIOA,GPIOB,GPIOC,GPIOD};

static const u32 s_gpio_apb[] = {RCC_APB2Periph_GPIOA,RCC_APB2Periph_GPIOB,RCC_APB2Periph_GPIOC,RCC_APB2Periph_GPIOD};

static void gpio_extract_pin_info(PinCtrlEnum pininfo,u16 *pin,u16 *cnt)
{
    *pin = ((u16)1 << (pininfo & 0x0f));

    *cnt = (pininfo >> 4)&0x0f;
}


void gpio_type_set(PinCtrlEnum pininfo , GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    u16 pin,cnt;

    gpio_extract_pin_info(pininfo,&pin,&cnt);

    RCC_APB2PeriphClockCmd(s_gpio_apb[cnt],ENABLE);

    GPIO_InitStructure.GPIO_Pin = pin; 

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Mode = mode;	

    GPIO_Init((GPIO_TypeDef*)s_gpio_base[cnt], &GPIO_InitStructure);

    s_board_gpio[cnt] &= (~pin);
}


void gpio_out(PinCtrlEnum pininfo,u8 level)
{
    u16 pin,cnt;

    gpio_extract_pin_info(pininfo,&pin,&cnt);
    
    if(level)
    {
        GPIO_SetBits((GPIO_TypeDef*)s_gpio_base[cnt],pin);
    }
    else
    {
        GPIO_ResetBits((GPIO_TypeDef*)s_gpio_base[cnt],pin);
    }   
}

u8 gpio_read(PinCtrlEnum pininfo)
{
    u16 pin,cnt;

    gpio_extract_pin_info(pininfo,&pin,&cnt);

    return (u8)GPIO_ReadInputDataBit((GPIO_TypeDef*)s_gpio_base[cnt],pin);
}

u8 gpio_unuse_sleep(GPIOMode_TypeDef mode)
{
    u8 i = 0;

    GPIO_InitTypeDef GPIO_InitStructure;

    for(i = 0; i < sizeof(s_board_gpio)/sizeof(u16) ; i++)
    {
        if(s_board_gpio[i] != 0)
        {
            //GPIO_DeInit(s_gpio_apb[i]);
            GPIO_InitStructure.GPIO_Pin  = s_board_gpio[i];
            GPIO_InitStructure.GPIO_Mode = mode;
            GPIO_InitStructure.GPIO_Speed= GPIO_Speed_10MHz;
            GPIO_Init((GPIO_TypeDef*)s_gpio_base[i], &GPIO_InitStructure);
        }
    }

    return 0;
}

