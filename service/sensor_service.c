#include "kk_os_header.h"
#include "sensor_service.h"
#include "i2c_board_config.h"
#include "stm32f10x_exti.h"

typedef struct
{
    u8 sensor_type;

    u32 shake;

    u32 stable; 

    u32 enit_time;
    
}GsensorControlStruct;

GsensorControlStruct s_sensor;


void sensor_service_create(void)
{
    EXTI_InitTypeDef EXTI_InitStructure; 
		NVIC_InitTypeDef NVIC_InitStructure;
    
    Sensor_I2C_Init();

    gpio_type_set(PB2,GPIO_Mode_IN_FLOATING);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource2);

                                                                                               
    EXTI_InitStructure.EXTI_Line = EXTI_Line2;               //中断线选择
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;      //EXTI为中断模式
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  //下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;                //使能中断
    EXTI_Init(&EXTI_InitStructure);

    
    NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;          //
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; //抢占优先级：1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;        //子优先级：1
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           //使能中断通道
    NVIC_Init(&NVIC_InitStructure);


    memset((u8 *)&s_sensor,0,sizeof(GsensorControlStruct));
}

void sensor_service_proc(void)
{
    static u8 count = 0;

    u16 aclr[3] = {0};

    u8 config = 0;

    JsonObject* p_log_root = NULL;

    static VehicleState sta = VEHICLE_STATE_RUN;
    
    if(s_sensor.sensor_type == 0)
    {
        Sensor_I2C_BufferRead(0x31 , &s_sensor.sensor_type,0x0F,1);

        LOG(LEVEL_INFO,"Gsensor ID %x",s_sensor.sensor_type);  

        config = 0x67;
        Sensor_I2C_ByteWrite(0x30,&config,0x20);

	    //  高精度模式 //2G 
        config = 0x88;
        Sensor_I2C_ByteWrite(0x30,&config,0x23);
	    //  高精度模式
        config = 0x31;
        Sensor_I2C_ByteWrite(0x30,&config,0x21);
        config = 0x40;
        Sensor_I2C_ByteWrite(0x30,&config,0x22);
        config = 0x00;
        Sensor_I2C_ByteWrite(0x30,&config,0x25);
        config = 0x00;
        Sensor_I2C_ByteWrite(0x30,&config,0x24);
        config = 0x2A;
        Sensor_I2C_ByteWrite(0x30,&config,0x30);
        config = 1;
        Sensor_I2C_ByteWrite(0x30,&config,0x32);
        config = 0;
        Sensor_I2C_ByteWrite(0x30,&config,0x33);
    }
    else
    {
        if(++count > 100)
        {
            count = 0;
            
            s_sensor.stable++;
            
            if(s_sensor.stable > 30 && s_sensor.stable < 300)
            {
                if(system_state_get_vehicle_state() != VEHICLE_STATE_STATIC)
                {
                    system_state_set_vehicle_state(VEHICLE_STATE_STATIC);

                    sta = VEHICLE_STATE_STATIC;

                    p_log_root = json_create();

                    json_add_string(p_log_root,"vehicle_change","static");

                    log_service_upload(LEVEL_INFO, p_log_root);
                }
            }
            else if(s_sensor.stable >= 300)
            {
                if(system_state_get_work_state() == KK_SYSTEM_STATE_WORK)
                {
                    system_state_set_work_state(KK_SYSTEM_STATE_SLEEP);

                    p_log_root = json_create();

                    json_add_string(p_log_root,"work_state","into sleep");

                    log_service_upload(LEVEL_INFO, p_log_root);
                }
            }

            if(sta == VEHICLE_STATE_STATIC && sta != system_state_get_vehicle_state())
            {
                sta = system_state_get_vehicle_state();

                p_log_root = json_create();

                json_add_string(p_log_root,"vehicle_change","run");

                log_service_upload(LEVEL_INFO, p_log_root);
            }

            if(system_state_get_work_state() == KK_SYSTEM_STATE_SLEEP)
            {
                if(s_sensor.shake >= 5)
                {
                    system_state_set_work_state(KK_SYSTEM_STATE_WORK);

                    p_log_root = json_create();

                    json_add_string(p_log_root,"work_state","exit sleep");

                    log_service_upload(LEVEL_INFO, p_log_root);
                }
            }
        }
    }
}

void sensor_service_destory(void)
{
    
}

void EXTI2_IRQHandler(void)
{
    JsonObject* p_log_root = NULL;
    u32 diff = util_clock() - s_sensor.enit_time;
    
    if(EXTI_GetITStatus(EXTI_Line2))
    {
        LOG(LEVEL_DEBUG,"shake detect!");

        if(diff == 1)
        {
            s_sensor.shake++;
        }
        else if(diff != 0)
        {
            s_sensor.shake = 0;
        }

        s_sensor.enit_time = util_clock();

        if(s_sensor.shake >= 3)
        {
            system_state_set_vehicle_state(VEHICLE_STATE_RUN);
            
        }
        else if(s_sensor.shake >= 2)
        {
            s_sensor.stable = 0;  //静止清0
        }
    }

    EXTI_ClearITPendingBit(EXTI_Line2);
}

