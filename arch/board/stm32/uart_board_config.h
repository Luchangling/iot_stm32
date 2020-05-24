
#ifndef __UART_BOARD_CONFIG_H__
#define __UART_BOARD_CONFIG_H__
#include "kk_type.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "fifo.h"
#include "gpio_board_config.h"
#include "fifo.h"
#include "stm32f10x_dma.h"
#include "stm32f10x.h"
typedef enum
{
    UART1,
    UART2,
    UART3,

    UART_INVLIAD
    
}UartIDEnum;

typedef struct
{
    UartIDEnum id;
    
    u8* name;
    
    u32 base;

    u32 pin_remap;

    PinCtrlEnum rx;

    PinCtrlEnum tx;

    u32 apb_flag;

    u32 uart_apb;

    USART_InitTypeDef uart_init;

    NVIC_InitTypeDef  uart_nvic;

    u32 dma_apb;

    u32 dma_channel_rx;

    NVIC_InitTypeDef dma_rx_nvic;

    u32 dma_channel_rx_flg;

    u32 dma_channel_tx;

    NVIC_InitTypeDef dma_tx_nvic;

    u32 dma_channel_tx_flg;
    
}UartConfigStruct;

typedef enum
{
    APB1,
    APB2
    
}UartAPBEnum;

struct uart_option
{
    void (*putc)(u8 ch);
    s8   (*getc)(u8 *res);
    void (*puts)(void);
    FifoType *rx;
    FifoType *tx;
    void (*read_cb)(void);
    void (*write_cb)(void);
    
};

typedef struct uart_option UartOptionStruct;

typedef enum
{
    DMA_NUL = 0,
    
    DMA_RX = 1<<0,
    DMA_TX = 1<<1,
    
}DMAInfoStruct;

UartOptionStruct* uart_board_init(u8 *name, DMAInfoStruct use_dma,u16 rx_len);


#endif

