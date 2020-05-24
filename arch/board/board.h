
#ifndef __BOARD_H__
#define __BOARD_H__

#include "gpio_board_config.h"
#include "i2c_board_config.h"
#include "uart_board_config.h"
#include "bit_band_define.h"
#include "spi_flash_board_config.h"
#include "kk_type.h"
#include "stm32f10x_tim.h"
typedef void (*board_init_func)(void);

extern board_init_func _board_init[];

void delay_us(u32 nus);
void delay_ms(u32 nms);
u32 clock(void);

void iwdg_feed(void);

void disable_iwdg(void);

u32 get_local_utc_sec(void);

void set_local_time(u32 ticks);


#endif

