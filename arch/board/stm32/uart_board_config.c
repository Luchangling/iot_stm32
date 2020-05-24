
#include "uart_board_config.h"
#include "kk_os_header.h"

#define UART_USE_DMA

#ifdef  UART_USE_DMA
#define UART_DMA_BUFF 128
#define UART_FIFO_BUFF 1280
typedef struct
{
    u8 *rx_buf;
    u8 *tx_buf;

    FifoType rx_fifo;
    FifoType tx_fifo;
    
}UartDMABuffStruct;

UartDMABuffStruct _DMA[UART_INVLIAD];

#endif

UartOptionStruct stm32_op[];


const UartConfigStruct s_uart_config[UART_INVLIAD] = {

    {                       //model
        UART1,
        "uart1",
        USART1_BASE,
        GPIO_Remap_USART1,         //Enable Remap
        PB7,PB6,   //RX,Tx
        APB2,
        RCC_APB2Periph_USART1|RCC_APB2Periph_AFIO, //Usart1 APB
        {          //uart_init
          115200,
          USART_WordLength_8b,USART_StopBits_1,USART_Parity_No,
          USART_Mode_Rx | USART_Mode_Tx,
          USART_HardwareFlowControl_None,
        },
        {          //nvic control
            USART1_IRQn,1,0,ENABLE,
        },
        RCC_AHBPeriph_DMA1,
        DMA1_Channel5_BASE,     //Rx
        {
            DMA1_Channel5_IRQn,2,0,ENABLE
        },
        DMA1_FLAG_TC5,
        DMA1_Channel4_BASE,     //Tx
        {
            DMA1_Channel4_IRQn,2,1,ENABLE
        },
        DMA1_FLAG_GL4
    },
    {                       //gps
        UART2,
        "uart2",
        USART2_BASE,
        0,         //Enable Remap
        PA3,PA2,   //RX,Tx
        APB1,
        RCC_APB1Periph_USART2, //Usart1 APB
        {          //uart_init
          115200,
          USART_WordLength_8b,USART_StopBits_1,USART_Parity_No,
          USART_Mode_Rx | USART_Mode_Tx,
          USART_HardwareFlowControl_None,
        },
        {          //nvic control
            USART2_IRQn,1,1,ENABLE,
        },
        RCC_AHBPeriph_DMA1,
        DMA1_Channel6_BASE,     //Rx
        {
            DMA1_Channel6_IRQn,2,2,ENABLE
        },
        DMA1_FLAG_TC6,
        DMA1_Channel7_BASE,     //Tx
        {
            DMA1_Channel7_IRQn,2,3,ENABLE
        },
        DMA1_FLAG_GL7
    },
    {                       //Dbg
        UART3,
        "uart3",
        USART3_BASE,
        GPIO_PartialRemap_USART3,         //Enable Remap
        PC11,PC10,   //RX,Tx
        APB1,
        RCC_APB1Periph_USART3, //Usart1 APB
        {          //uart_init
          115200,
          USART_WordLength_8b,USART_StopBits_1,USART_Parity_No,
          USART_Mode_Rx | USART_Mode_Tx,
          USART_HardwareFlowControl_None,
        },
        {          //nvic control
            USART3_IRQn,1,2,ENABLE,
        },
        RCC_AHBPeriph_DMA1,
        DMA1_Channel3_BASE,     //Rx
        {
            DMA1_Channel3_IRQn,2,4,ENABLE
        },
        DMA1_FLAG_TC3,
        DMA1_Channel2_BASE,     //Tx
        {
            DMA1_Channel2_IRQn,2,5,ENABLE
        },
        DMA1_FLAG_GL2
    },
    

};

s8 uart_get_config_file(u8 *name)
{
    s8 i,max = 0; 

    max = sizeof(s_uart_config)/sizeof(UartConfigStruct);

    for(i = 0; i < max; i++)
    {
        if(strcmp((char *)name,(char *)s_uart_config[i].name) == 0)
        {
            break;
        }
    }

    if(i >= max) i = -1;

    return i;
}

UartOptionStruct* uart_board_init(u8 *name, DMAInfoStruct use_dma,u16 rx_len)
{
    s8 i = -1;
    //u8 data;
    UartOptionStruct *res = NULL;

    UartConfigStruct *pt = NULL;
    #ifdef UART_USE_DMA
    DMA_InitTypeDef DMA_InitStructure;
    #endif

    //GPIO_InitTypeDef GPIO_InitStructure;
    //USART_InitTypeDef USART_InitStructure;
    //NVIC_InitTypeDef NVIC_InitStructure;

    i = uart_get_config_file(name);

    if(i >= 0)
    {
        pt = (UartConfigStruct *)&s_uart_config[i];

        #if 1

        #ifdef UART_USE_DMA

        //tx init

        if(use_dma & DMA_TX)
        {
            if(_DMA[i].tx_buf == NULL)
            {
                _DMA[i].tx_buf = malloc(UART_DMA_BUFF);
            }

            if(_DMA[i].tx_buf != NULL)
            {
                RCC_AHBPeriphClockCmd(pt->dma_apb, ENABLE);

                DMA_Cmd((DMA_Channel_TypeDef *)pt->dma_channel_tx,DISABLE);
                DMA_DeInit((DMA_Channel_TypeDef *)pt->dma_channel_tx);
                if(pt->base == (u32)USART1)
                {
                    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART1->DR);        // 设置串口发送数据寄存器
                }
                else if(pt->base == (u32)USART2)
                {
                    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART2->DR);        // 设置串口发送数据寄存器
                }
                else if(pt->base == (u32)USART3)
                {
                    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART3->DR);        // 设置串口发送数据寄存器
                }
                DMA_InitStructure.DMA_MemoryBaseAddr = (u32)_DMA[i].tx_buf;             // 设置发送缓冲区首地址
                DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;                      // 设置外设位目标，内存缓冲区 -> 外设寄存器
                DMA_InitStructure.DMA_BufferSize = 0;                                   // 需要发送的字节数，这里其实可以设置为0，因为在实际要发送的时候，会重新设置次值
                DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;        // 外设地址不做增加调整，调整不调整是DMA自动实现的
                DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;                 // 内存缓冲区地址增加调整
                DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据宽度8位，1个字节
                DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;         // 内存数据宽度8位，1个字节
                DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                           // 单次传输模式
                DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;                 // 优先级设置
                DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                            // 关闭内存到内存的DMA模式
                DMA_Init((DMA_Channel_TypeDef *)pt->dma_channel_tx, &DMA_InitStructure);// 写入配置
                DMA_ClearFlag(pt->dma_channel_tx_flg);                                           // 清除DMA所有标志
                DMA_Cmd((DMA_Channel_TypeDef *)pt->dma_channel_tx, DISABLE); // 关闭DMA
                //DMA_ITConfig((DMA_Channel_TypeDef *)pt->dma_channel_tx, DMA_IT_TC, ENABLE);

                NVIC_Init(&pt->dma_tx_nvic);

                if(_DMA[i].tx_fifo.base_addr == NULL)
                {
                    fifo_init(&_DMA[i].tx_fifo,UART_FIFO_BUFF);
                }
            }
            else
            {
                return res;
            }
            
            
        }

        //rx init
        if(use_dma & DMA_RX)
        {
            if(_DMA[i].rx_buf == NULL)
            {
                _DMA[i].rx_buf = malloc(UART_DMA_BUFF);
            }

            if(_DMA[i].rx_buf != NULL)
            {
                DMA_Cmd((DMA_Channel_TypeDef*)pt->dma_channel_rx, DISABLE);                                   // 关DMA通道
                DMA_DeInit((DMA_Channel_TypeDef*)pt->dma_channel_rx);                                         // 恢复缺省值

                if(pt->base == (u32)USART1)
                {
                    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART1->DR);        // 设置串口发送数据寄存器
                }
                else if(pt->base == (u32)USART2)
                {
                    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART2->DR);        // 设置串口发送数据寄存器
                }
                else if(pt->base == (u32)USART3)
                {
                    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART3->DR);        // 设置串口发送数据寄存器
                }
                //DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&((USART_TypeDef *)(pt->base)->DR));// 设置串口接收数据寄存器
                DMA_InitStructure.DMA_MemoryBaseAddr = (u32)_DMA[i].rx_buf;             // 设置接收缓冲区首地址
                DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;                      // 设置外设为数据源，外设寄存器 -> 内存缓冲区
                DMA_InitStructure.DMA_BufferSize = UART_DMA_BUFF;                       // 需要最大可能接收到的字节数  不能为零
                DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;        // 外设地址不做增加调整，调整不调整是DMA自动实现的
                DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;                 // 内存缓冲区地址增加调整
                DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据宽度8位，1个字节
                DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;         // 内存数据宽度8位，1个字节
                DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                           // 单次传输模式
                DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;                 // 优先级设置
                DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                            // 关闭内存到内存的DMA模式
                DMA_Init((DMA_Channel_TypeDef *)pt->dma_channel_rx, &DMA_InitStructure);               // 写入配置
                DMA_ClearFlag(pt->dma_channel_rx_flg);                                           // 清除DMA所有标志
                DMA_Cmd((DMA_Channel_TypeDef *)pt->dma_channel_rx, ENABLE);                                  // 开启接收DMA通道，等待接收数据
                //DMA_ITConfig((DMA_Channel_TypeDef *)pt->dma_channel_rx, pt->dma_channel_rx_flg, ENABLE);
                /* Enable the DMA Interrupt */
                NVIC_Init(&pt->dma_rx_nvic);
                
            }
            else
            {
                return res;
            }
        }
        #endif

        if(_DMA[i].rx_fifo.base_addr == NULL)
        {
            fifo_init(&_DMA[i].rx_fifo,rx_len); 
        }
        
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);

        if(pt->apb_flag == APB1)
        {
            RCC_APB1PeriphClockCmd(pt->uart_apb,ENABLE);
        }
        else
        {
            RCC_APB2PeriphClockCmd(pt->uart_apb,ENABLE);
        }

        if(pt->pin_remap)
        {
            GPIO_PinRemapConfig(pt->pin_remap,ENABLE);
        }

        gpio_type_set(pt->tx, GPIO_Mode_AF_PP);
        
        gpio_type_set(pt->rx, GPIO_Mode_IPU);

        NVIC_Init(&pt->uart_nvic);  

        USART_Init((USART_TypeDef *)pt->base,&pt->uart_init);

        USART_Cmd((USART_TypeDef *)pt->base, ENABLE);

        USART_ITConfig((USART_TypeDef *)pt->base, USART_IT_IDLE, ENABLE);  // 开启 串口空闲IDEL 中断 

        if(!(use_dma&DMA_RX))
        {

            USART_ITConfig((USART_TypeDef *)pt->base, USART_IT_RXNE, ENABLE);  // 开启 串口空闲IDEL 中断 
        }
        
        if(use_dma != DMA_NUL)
        {
            //USART_ITConfig((USART_TypeDef *)pt->base,USART_IT_TC,DISABLE);
            if(use_dma&DMA_RX)
            USART_DMACmd((USART_TypeDef *)pt->base, USART_DMAReq_Rx, ENABLE); // 开启串口DMA接收  接受可以提前开启
            if(use_dma & DMA_TX)
            USART_DMACmd((USART_TypeDef *)pt->base, USART_DMAReq_Tx, ENABLE);  // 开启串口DMA发送
        }
        
        #else
        {
            if(_DMA[i].rx_fifo.base_addr == NULL)
            {
                fifo_init(&_DMA[i].rx_fifo,UART_FIFO_BUFF); 
            }
            
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);

            RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);

            //RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);

            //GPIO_PinRemapConfig(GPIO_Remap_USART1,ENABLE);

            //gpio_type_set(pt->tx, GPIO_Mode_AF_PP);
        
            //gpio_type_set(pt->rx, GPIO_Mode_IPU);

            //USART_Init((USART_TypeDef *)pt->base,&pt->uart_init);

            //USART_Cmd((USART_TypeDef *)pt->base, ENABLE);

            //NVIC_Init(&pt->uart_nvic); 
            

       
            GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_Init(GPIOA,&GPIO_InitStructure);

            GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;//GPIO_Mode_IN_FLOATING;
            GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_Init(GPIOA,&GPIO_InitStructure);

            USART_InitStructure.USART_BaudRate = 115200;
            USART_InitStructure.USART_WordLength = USART_WordLength_8b;
            USART_InitStructure.USART_StopBits = USART_StopBits_1;
            USART_InitStructure.USART_Parity = USART_Parity_No;
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None ;
            USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
            USART_Init(USART2,&USART_InitStructure);
            data = data;
            data = USART2->DR;
            data = USART2->SR;

            USART_ITConfig(USART2,USART_IT_RXNE,ENABLE);
            USART_Cmd(USART2,ENABLE);

            NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
            NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
            NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
            NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
            NVIC_Init(&NVIC_InitStructure);

            // USART_ITConfig((USART_TypeDef *)pt->base, USART_IT_RXNE, ENABLE); 
            //USART_ITConfig((USART_TypeDef *)pt->base, uint16_t USART_IT, FunctionalState NewState)
        }
        #endif
        return &stm32_op[i];
    }

    return NULL;
}

void uart_isr_handle(UartIDEnum id, u8 is_dma)
{
    UartConfigStruct *p = NULL;

    u16 len = 0;

    u8 res = 0;

    p = (UartConfigStruct*)&s_uart_config[id];
    
    if(is_dma)
    {
        if(DMA_GetITStatus(p->dma_channel_rx_flg) != RESET)
        {
            DMA_ClearITPendingBit(p->dma_channel_rx_flg); 
            
            DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_rx, DISABLE);
            
            fifo_insert(&_DMA[id].rx_fifo,_DMA[id].rx_buf, UART_DMA_BUFF);

            DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_rx,UART_DMA_BUFF);

            DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_rx, ENABLE);
        }

        if(DMA_GetITStatus(p->dma_channel_tx_flg) != RESET)
        {
            DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,DISABLE);
            
            DMA_ClearITPendingBit(p->dma_channel_tx_flg);         // 清除标志
        }

        {
            DMA_ClearITPendingBit(p->dma_channel_tx_flg);         // 清除标志

            DMA_ClearITPendingBit(p->dma_channel_rx_flg); 
        }
    }
    else
    {
        if(USART_GetITStatus((USART_TypeDef* )p->base,USART_IT_RXNE) != RESET)
        {
            res = ((USART_TypeDef* )(p->base))->DR;

            if(_DMA[id].rx_fifo.size > 0)
            {
                len = _DMA[id].rx_fifo.write_idx;

                _DMA[id].rx_fifo.base_addr[len] = res;

                _DMA[id].rx_fifo.write_idx = (++len)%(_DMA[id].rx_fifo.size);
            }

            

            //fifo_insert(&_DMA[id].rx_fifo,&res, 1);

            USART_ITConfig((USART_TypeDef* )p->base,USART_IT_IDLE,ENABLE);
        }
        else if(USART_GetITStatus((USART_TypeDef* )p->base,USART_IT_IDLE) != RESET)
        {
            res = ((USART_TypeDef* )(p->base))->DR;
            res = ((USART_TypeDef* )(p->base))->SR;

            if(_DMA[id].rx_buf)
            {
                DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_rx,DISABLE);

                //DMA_ClearFlag(p->dma_channel_rx_flg);

                len = UART_DMA_BUFF - DMA_GetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_rx);

                DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_rx,UART_DMA_BUFF);

                fifo_insert(&_DMA[id].rx_fifo,_DMA[id].rx_buf, len);

                DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_rx, ENABLE);

            }
            else
            {
                USART_ITConfig((USART_TypeDef* )p->base,USART_IT_IDLE,DISABLE);
            }

            //通知上层应用

            if(stm32_op[id].read_cb)stm32_op[id].read_cb();
            
        }
        else
        {
            res = ((USART_TypeDef* )(p->base))->DR;
        }

        res = res;

        len = len;
    }
}

void USART1_IRQHandler(void)
{
    uart_isr_handle(UART1,0);
}

void USART2_IRQHandler(void)
{
    uart_isr_handle(UART2,0);
}

void USART3_IRQHandler(void)
{
    uart_isr_handle(UART3,0);
}

void DMA1_Channel4_IRQHandler(void)
{
    uart_isr_handle(UART1,1);
}
void DMA1_Channel5_IRQHandler(void)
{
    uart_isr_handle(UART1,1);
}

void DMA1_Channel6_IRQHandler(void)
{
    uart_isr_handle(UART2,1);
}
void DMA1_Channel7_IRQHandler(void)
{
    uart_isr_handle(UART2,1);
}

void DMA1_Channel2_IRQHandler(void)
{
    uart_isr_handle(UART3,1);
}
void DMA1_Channel3_IRQHandler(void)
{
    uart_isr_handle(UART3,1);
}

void uart1_putc(u8 ch)
{
    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART1];

    ((USART_TypeDef *)(p->base))->DR = (ch &(u16)0x01FF);

    while(USART_GetFlagStatus((USART_TypeDef* )p->base,USART_FLAG_TC) == RESET);

}

s8 uart1_getc(u8 *ch)
{
    s8 res = KK_UNKNOWN;

    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART1];

    if(USART_GetFlagStatus((USART_TypeDef* )p->base,USART_IT_RXNE) != RESET)
    {
        *ch = ((USART_TypeDef *)(p->base))->DR&0xFF;

        res = KK_SUCCESS;
    }

    return res;
}

void uart1_puts(void)
{
    u16 len = 0;
    
    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART1];

    if(_DMA[UART1].tx_fifo.size == 0)return;

    if(DMA_GetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_tx) == 0)
    {
        DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,DISABLE);

        len  = fifo_get_msg_length(&_DMA[UART1].tx_fifo);

        if(len == 0)return;

        if(len > UART_DMA_BUFF) len = UART_DMA_BUFF;

        fifo_peek(&_DMA[UART1].tx_fifo,_DMA[UART1].tx_buf,len);

        fifo_pop_len(&_DMA[UART1].tx_fifo,len);

        DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_tx,len);
        
        DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,ENABLE);
    }
}

void uart2_putc(u8 ch)
{
    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART2];

    ((USART_TypeDef *)(p->base))->DR = (ch &(u16)0x01FF);

    while(USART_GetFlagStatus((USART_TypeDef* )p->base,USART_FLAG_TC) == RESET);

}

s8 uart2_getc(u8 *ch)
{
    s8 res = KK_UNKNOWN;

    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART2];

    if(USART_GetFlagStatus((USART_TypeDef* )p->base,USART_IT_RXNE) != RESET)
    {
        *ch = ((USART_TypeDef *)(p->base))->DR&0xFF;

        res = KK_SUCCESS;
    }

    return res;
}

void uart2_puts(void)
{
    u16 len = 0;
    
    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART2];

    if(_DMA[UART2].tx_fifo.size == 0)return;

    if(DMA_GetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_tx) == 0)
    {
        DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,DISABLE);

        len  = fifo_get_msg_length(&_DMA[UART2].tx_fifo);

        if(len == 0)return;

        if(len > UART_DMA_BUFF) len = UART_DMA_BUFF;

        fifo_peek(&_DMA[UART2].tx_fifo,_DMA[UART2].tx_buf,len);

        fifo_pop_len(&_DMA[UART2].tx_fifo,len);

        DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_tx,len);
        
        DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,ENABLE);
    }
}


void uart3_putc(u8 ch)
{
    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART3];

    ((USART_TypeDef *)(p->base))->DR = (ch &(u16)0x01FF);


	while (USART_GetFlagStatus((USART_TypeDef* )p->base, USART_FLAG_TC) == RESET) {}
}

s8 uart3_getc(u8 *ch)
{
    s8 res = KK_UNKNOWN;

    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART3];

    if(USART_GetFlagStatus((USART_TypeDef* )p->base,USART_IT_RXNE) != RESET)
    {
        *ch = ((USART_TypeDef *)(p->base))->DR&0xFF;

        res = KK_SUCCESS;
    }

    return res;
}

void uart3_puts(void)
{
    u16 len = 0;
    
    UartConfigStruct *p = (UartConfigStruct*)&s_uart_config[UART3];

    if(_DMA[UART3].tx_fifo.size == 0)return;

    if(DMA_GetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_tx) == 0)
    {
        DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,DISABLE);

        len  = fifo_get_msg_length(&_DMA[UART3].tx_fifo);

        if(len == 0)return;

        if(len > UART_DMA_BUFF) len = UART_DMA_BUFF;

        fifo_peek(&_DMA[UART3].tx_fifo,_DMA[UART3].tx_buf,len);

        fifo_pop_len(&_DMA[UART3].tx_fifo,len);

        DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)p->dma_channel_tx,len);
        
        DMA_Cmd((DMA_Channel_TypeDef *)p->dma_channel_tx,ENABLE);
    }
}


UartOptionStruct stm32_op[UART_INVLIAD] = {

    {
        uart1_putc,
        uart1_getc,
        uart1_puts,
        &_DMA[UART1].rx_fifo,
        &_DMA[UART1].tx_fifo,
        NULL,
        NULL,
    },
    {
        uart2_putc,
        uart2_getc,
        uart2_puts,
        &_DMA[UART2].rx_fifo,
        &_DMA[UART2].tx_fifo,
        NULL,
        NULL,
    },
    {
        uart3_putc,
        uart3_getc,
        uart3_puts,
        &_DMA[UART3].rx_fifo,
        &_DMA[UART3].tx_fifo,
        NULL,
        NULL,
    }
    
};


