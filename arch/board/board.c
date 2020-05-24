
#include "board.h"
#include "kk_os_config_api.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_iwdg.h"
#include "core_cm3.h"
static u32 g_us_count_multiplier = 0;
static u32 g_ms_count_multiplier = 0;
static u32 s_sys_clock = 0;
static u32 s_setting_clock = 0;
static u32 s_base_time = 0;
static u32 s_iwdg_enable = 0;


void rcc_init(void)
{

  RCC_DeInit();

  RCC_HSEConfig(RCC_HSE_ON);                                    //RCC_HSE_ON——HSE晶振打开(ON)

  if(RCC_WaitForHSEStartUp() == SUCCESS)                        //SUCCESS：HSE晶振稳定且就绪
  {   
    RCC_HCLKConfig(RCC_SYSCLK_Div1);                            //RCC_SYSCLK_Div1——AHB时钟 = 系统时钟

    RCC_PCLK2Config(RCC_HCLK_Div1);                             //RCC_HCLK_Div1——APB2时钟 = HCLK

    RCC_PCLK1Config(RCC_HCLK_Div2);                             //RCC_HCLK_Div2——APB1时钟 = HCLK / 2

    FLASH_SetLatency(FLASH_Latency_2);                          //FLASH_Latency_2  2延时周期

    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);       // 预取指缓存使能

    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);        // PLL的输入时钟 = HSE时钟频率；RCC_PLLMul_9——PLL输入时钟x 9

    RCC_PLLCmd(ENABLE);

    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) ;    

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);                  //RCC_SYSCLKSource_PLLCLK——选择PLL作为系统时钟
                          

    while(RCC_GetSYSCLKSource() != 0x08);                       //0x08：PLL作为系统时钟

  }

}

void nvic_init(void)
{

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3);	            //设置NVIC中断分组2:2位抢占优先级，2位响应优先级

}

                           
void SysTick_Handler(void)      //systick中断服务函数,使用ucos时用到
{				   
	OSIntEnter();		    //进入中断
    OSTimeTick();           //调用ucos的时钟服务程序               
    OSIntExit();            //触发任务切换软中断
    service_log_timer_proc();
    led_serivce_time_proc();
    //led_serivce_time_proc();
}


//初始化延迟函数
//当使用ucos的时候,此函数会初始化ucos的时钟节拍
void delay_init()	 
{

    #ifdef OS_CRITICAL_METHOD 	//如果OS_CRITICAL_METHOD定义了,说明使用ucosII了.
	u32 reload;
    #endif
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);	//选择外部时钟  HCLK/8
	
	g_us_count_multiplier = SystemCoreClock/8000000;	//为系统时钟的1/8  
	 
	reload = SystemCoreClock/8000000;		//每秒钟的计数次数 单位为K	   
	reload *= 1000000/OS_TICKS_PER_SEC;//根据OS_TICKS_PER_SEC设定溢出时间
							//reload为24位寄存器,最大值:16777216,在72M下,约合1.86s左右	
	g_ms_count_multiplier = 1000/OS_TICKS_PER_SEC;//代表ucos可以延时的最少单位	   
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;   	//开启SYSTICK中断
	SysTick->LOAD  = reload; 	//每1/OS_TICKS_PER_SEC秒中断一次	
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;   	//开启SYSTICK    
}								    

//延时nus
//nus为要延时的us数.		    								   
void delay_us(u32 nus)
{		
	u32 ticks;
	u32 told,tnow,tcnt=0;
	u32 reload=SysTick->LOAD;	//LOAD的值	    	 
	ticks=nus*g_us_count_multiplier; 			//需要的节拍数	  		 
	tcnt=0;
	told=SysTick->VAL;        	//刚进入时的计数器值
	while(1)
	{
		tnow=SysTick->VAL;	
		if(tnow!=told)
		{	    
			if(tnow<told)tcnt+=told-tnow;//这里注意一下SYSTICK是一个递减的计数器就可以了.
			else tcnt+=reload-tnow+told;	    
			told=tnow;
			if(tcnt>=ticks)break;//时间超过/等于要延迟的时间,则退出.
		}  
	}; 									    
}
//延时nms
//nms:要延时的ms数
void delay_ms(u32 nms)
{	
	if(OSRunning == true)//如果os已经在跑了	    
	{		  
		if(nms>=g_ms_count_multiplier)//延时的时间大于ucos的最少时间周期 
		{
   			OSTimeDly(nms/g_ms_count_multiplier);//ucos延时
		}
		nms%=g_ms_count_multiplier;				//ucos已经无法提供这么小的延时了,采用普通方式延时    
	}
	delay_us((u32)(nms*1000));	//普通方式延时,此时ucos无法启动调度.
}


void clock_init(void)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //时钟使能

    TIM_DeInit(TIM3);
	
	//定时器TIM3初始化
	TIM_TimeBaseStructure.TIM_Period = 9999; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值	
	TIM_TimeBaseStructure.TIM_Prescaler = 7199; //设置用来作为TIMx时钟频率除数的预分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //根据指定的参数初始化TIMx的时间基数单位
 
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE ); //使能指定的TIM3中断,允许更新中断

	//中断优先级NVIC设置
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //先占优先级0级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;  //从优先级3级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道被使能
	NVIC_Init(&NVIC_InitStructure);  //初始化NVIC寄存器
    
    
	TIM_Cmd(TIM3, ENABLE);  //使能TIMx		

    s_sys_clock = 0;
}
//定时器3中断服务程序
void TIM3_IRQHandler(void)   //TIM3中断
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)  //检查TIM3更新中断发生与否
	{
	    TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );  //清除TIMx更新中断标志 
	    s_sys_clock++;
        iwdg_feed();
	}
}


u32 clock(void)
{
    return s_sys_clock;
}


u32 util_clock(void)
{
    return s_sys_clock;
}


void set_local_time(u32 ticks)
{
    s_base_time = ticks;

    s_setting_clock = s_sys_clock;
}

u32 get_local_utc_sec(void)
{
    return (s_base_time + s_sys_clock - s_setting_clock);
}

void iwdg_init(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable); /* 使能对寄存器IWDG_PR和IWDG_RLR的写操作*/
    IWDG_SetPrescaler(4);     /*设置IWDG预分频值:设置IWDG预分频值*/
    IWDG_SetReload(1250);     /*设置IWDG重装载值*/
    IWDG_ReloadCounter();     /*按照IWDG重装载寄存器的值重装载IWDG计数器*/
    IWDG_Enable();            /*使能IWDG*/
    s_iwdg_enable = 1;
}

void disable_iwdg(void)
{
    s_iwdg_enable = 0;
}

void iwdg_feed(void)
{
    if(s_iwdg_enable)
    IWDG_ReloadCounter();
}

void reset_system(void)
{
    s_iwdg_enable = 0;
}

board_init_func _board_init[] = {

        rcc_init,
        nvic_init,
        iwdg_init,
        clock_init,
        delay_init,
        spi_flash_init,
        0,

}; 


