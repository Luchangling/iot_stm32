
#include "kk_os_header.h"
#include "system_state.h"
#include "utility.h"
#include "gprs.h"
#include "gps.h"
//#include "gps_service.h"
#include "socket.h"
#include "service_log.h"
#define SECONDS_PER_MIN 60

#define GPS_ON_TIME_SECONDS (5*SECONDS_PER_MIN)
//#define SYSTEM_STATE_FILE L"Z:\\goome\\GmStatusFile\0"

#define SYSTEM_STATE_MAGIC_NUMBER 0xFEFEFEFE

//4字节对齐，crc会占用4个字节
//8769D311FEFEFEFE3A000000FF00010001000000000000000000000000000100000000002A0000000000000000000000600000000000000065110301119863002A00000001000000
typedef struct
{
	//16比特CRC（不包含前2个字段,从status_bits开始算起）
    u16 crc;
	
	//特征值
	u32 magic;
	
	/**********************************************************************
	bit0 ——启动类型:0-重启,1——上电
    bit1 ——上电后是否已上传过GPS定位
    bit2 ——是否已发送静止点
    bit3 ——修改IP后是否已上传GPS数据
    bit4 
    bit5 ——ACC检测模式:0——震动检测；  1——ACC线检测
    bit6 ——设备relay端口状态:           0——低电平（恢复油电）；  1——高电平（断油电）  
    bit7 ——用户设置relay端口状态:0——低电平（恢复油电）；  1——高电平（断油电）
    bit8 ——设防状态
    bit9 
    bit10——是否冷启动
    bit11——备用
    bit12——备用
    bit13——备用
    bit14——备用
    bit15——备用
    
    bit16——断电报警,
    bit17——电池低电报警
    bit18——电源电压过低报警
    bit19——电源电压过高报警
    bit20——震动报警
    bit21——超速报警
    bit22——伪基站报警
    bit23——碰撞报警
    bit24——急加速报警
    bit25——急减速报警
    bit26——翻车报警
    bit27——急转弯报警
    bit28——拆动报警
    bit29——车辆移动报警
    bit30——备用
    bit31——备用
    **********************************************************************/
    u32 status_bits;

	BootReason boot_reason;

	//系统状态（工作,休眠）
	SystemWorkState work_state;

	//车辆状态（运动,静止）
	VehicleState vehicle_state;

	//重启总次数
	u32 reboot_count;

	//上电重启次数
	u32 reboot_for_power_on_counts;
	
	//断网重启次数
	u16 reboot_for_net_counts;

	//不定位重启次数
	u16 reboot_for_gps_counts;
	
	//升级重启次数
	u16 reboot_for_upgrade_counts;
	
	//指令重启次数
	u16 reboot_for_command_counts;

	//意外重启
	u16 reboot_for_exception_counts;

	//修复重启
	u16 reboot_for_checkpara_counts;

	//其它重启
	u16 reboot_for_other_counts;

	//启用时间
	u32 start_time;

	//高电压报警次数
	u32 power_high_voltage_count;

	//电池低电压报警次数
	u32 battery_low_voltage_count;

	//外部电池电压等级
	u8 extern_battery_voltage_grade;

	//里程（单位:米）
	u64 mileage;

	//当前可执行文件的校验合
	u32 check_sum;

	//GSensorType gsensor_type;

    u32 last_good_time;  // 上次网络正常的时间
    u32 call_ok_count;   //CURRENT_GPRS_INIT->CURRENT_GPRS_CALL_OK 次数

    char gpss_reboot_reason[30];   /*recent gps_service disconnect reason*/

    GPSData latest_gps;

	u8 ip_cache[SOCKET_INDEX_MAX][4];
}SystemState,*PSystemState;



static SystemState s_system_state;

static void init_para(void);

static KK_ERRCODE read_state_from_file(void); 

static KK_ERRCODE save_state_to_file(void); 

KK_ERRCODE system_state_create(void)
{
	KK_ERRCODE ret = KK_SUCCESS;
	init_para();
	ret = read_state_from_file();
	return ret;
}
#if 0
void system_state_set_last_gps(const GPSData *p_gps)
{
    memcpy(&s_system_state.latest_gps, p_gps, sizeof(s_system_state.latest_gps));
    //LOG(DEBUG,"clock(%d) system_state_set_last_gps %f,%f.", util_clock(), p_gps->lng,p_gps->lat);
}

void system_state_get_last_gps(GPSData *p_gps)
{
    memcpy(p_gps, &s_system_state.latest_gps,  sizeof(s_system_state.latest_gps));
    LOG(DEBUG,"clock(%d) system_state_get_last_gps %f,%f.", util_clock(), p_gps->lng,p_gps->lat);
}
#endif


static void init_para(void)
{
    //char *addr = NULL;
	s_system_state.magic = SYSTEM_STATE_MAGIC_NUMBER;
	s_system_state.status_bits = 0;
	s_system_state.boot_reason = KK_RREBOOT_UNKNOWN;
	s_system_state.work_state = KK_SYSTEM_STATE_WORK;
	s_system_state.vehicle_state = VEHICLE_STATE_RUN;
	
	s_system_state.reboot_count = 0;
	s_system_state.reboot_for_power_on_counts = 0;
	s_system_state.reboot_for_net_counts = 0;
    s_system_state.reboot_for_gps_counts = 0;
	s_system_state.reboot_for_upgrade_counts = 0;
	s_system_state.reboot_for_command_counts = 0;
	s_system_state.reboot_for_exception_counts = 0;
	s_system_state.reboot_for_other_counts = 0;
	s_system_state.reboot_for_checkpara_counts = 0;
	s_system_state.start_time = 0;
	s_system_state.power_high_voltage_count = 0;
	s_system_state.battery_low_voltage_count = 0;
	s_system_state.extern_battery_voltage_grade = 0;
	s_system_state.mileage = 0;
	//s_system_state.gsensor_type = GSENSOR_TYPE_UNKNOWN;
	s_system_state.last_good_time = 0;
	s_system_state.call_ok_count = 0;
	s_system_state.gpss_reboot_reason[0] = 0;

    //memset(&s_system_state.latest_gps, 0, sizeof(s_system_state.latest_gps));
	memset(s_system_state.ip_cache, 0, sizeof(s_system_state.ip_cache));

    #if 0
    addr = config_service_get_pointer(CFG_SERVERADDR);
    if (KK_strstr(addr, GOOME_GPSOO_DNS) > 0)
    {
        KK_ConvertIpAddr(CONFIG_GOOCAR_SERVER_IP,s_system_state.ip_cache[SOCKET_INDEX_MAIN]);
    }

    if (KK_strstr(addr, GOOME_LITEDEV_DNS) > 0)
    {
         KK_ConvertIpAddr(CONFIG_LITE_SERVER_IP,s_system_state.ip_cache[SOCKET_INDEX_MAIN]);
    }

    KK_ConvertIpAddr(CONFIG_AGPS_SERVER_IP,s_system_state.ip_cache[SOCKET_INDEX_AGPS]);
    KK_ConvertIpAddr(CONFIG_LOG_SERVER_IP,s_system_state.ip_cache[SOCKET_INDEX_LOG]);
    KK_ConvertIpAddr(GOOME_UPDATE_SERVER_IP,s_system_state.ip_cache[SOCKET_INDEX_UPDATE]);
    KK_ConvertIpAddr(CONFIG_SERVER_IP,s_system_state.ip_cache[SOCKET_INDEX_CONFIG]);
    #endif
}

KK_ERRCODE system_state_destroy(void)
{
	return KK_SUCCESS;
}

KK_ERRCODE system_state_reset(void)
{
	init_para();
	LOG(LEVEL_INFO,"system_state_reset");
	return save_state_to_file();
}

KK_ERRCODE system_state_clear_reboot_count(void)
{
	s_system_state.reboot_count = 0;
	s_system_state.reboot_for_power_on_counts = 0;
	s_system_state.reboot_for_net_counts = 0;
    s_system_state.reboot_for_gps_counts = 0;
	s_system_state.reboot_for_upgrade_counts = 0;
	s_system_state.reboot_for_command_counts = 0;
	s_system_state.reboot_for_exception_counts = 0;
	s_system_state.reboot_for_other_counts = 0;
	s_system_state.reboot_for_checkpara_counts = 0;
	LOG(LEVEL_INFO,"system_state_reset");
	return save_state_to_file();
}



KK_ERRCODE system_state_timer_proc(void)
{
	s_system_state.start_time++;
	return KK_SUCCESS;
}

u32 system_state_get_status_bits(void)
{
	return s_system_state.status_bits;
}

u32 system_state_get_start_time()
{
	return s_system_state.start_time;
}

BootReason system_state_get_boot_reason(bool add_counts)
{
	s_system_state.reboot_count++;
	if(add_counts)
	{
		switch (s_system_state.boot_reason)
		{
			case KK_RREBOOT_UNKNOWN:
			{
				s_system_state.reboot_for_exception_counts++;
			}
			break;
			
			case KK_REBOOT_CMD:
			{
				s_system_state.reboot_for_command_counts++;
			}
			break;
			
	    	case KK_REBOOT_UPDATE:
			{
				s_system_state.reboot_for_upgrade_counts++;
			}
			break;
			
	    	case KK_REBOOT_POWER_ON:
			{
				s_system_state.reboot_for_power_on_counts++;
			}
			break;
			
			case KK_REBOOT_GPRS:
			{
				s_system_state.reboot_for_net_counts++;
			}
			break;

			case KK_REBOOT_GPS:
			{
				s_system_state.reboot_for_gps_counts++;
			}
			break;

			
			case KK_REBOOT_CHECKPARA:
			{
				s_system_state.reboot_for_checkpara_counts++;
			}
			break;
			
			default:
			{
				s_system_state.reboot_for_other_counts++;
			}
			
			break;
		}
		
		LOG(LEVEL_INFO,"system_state_get_boot_reason");
		save_state_to_file();
	}
	
	return s_system_state.boot_reason;
}

const char* system_state_get_boot_reason_str(BootReason reason)
{
	switch (reason)
	{
		case KK_RREBOOT_UNKNOWN:
			return "unkonow restart";
		case KK_REBOOT_CMD:
			return "cmd restart";
    	case KK_REBOOT_UPDATE:
			return "update restart";
    	case KK_REBOOT_POWER_ON:
			return "power on restart";
		case KK_REBOOT_GPRS:
			return "gprs restart";
		case KK_REBOOT_GPS:
			return "gps restart";
		case KK_REBOOT_CHECKPARA:
			return "check para restart";
		default:
			return "unkonow";
	}
}


KK_ERRCODE system_state_set_boot_reason(const BootReason boot_reason)
{
	if (KK_REBOOT_POWER_ON == boot_reason)
	{
		system_state_set_has_reported_gps_since_boot(false);
		system_state_set_has_reported_lbs_since_boot(false);
		system_state_set_has_reported_static_gps(false);
		s_system_state.start_time = 0;
	}
	
	s_system_state.boot_reason = boot_reason; 
	LOG(LEVEL_INFO,"system_state_set_boot_reason");
	return save_state_to_file();
}

u32 system_state_get_reboot_counts(const BootReason boot_reason)
{
	switch (boot_reason)
	{
		case KK_RREBOOT_UNKNOWN:
		{
			return s_system_state.reboot_for_exception_counts;
		}
		
		case KK_REBOOT_CMD:
		{
			return s_system_state.reboot_for_command_counts;
		}
		
    	case KK_REBOOT_UPDATE:
		{
			return s_system_state.reboot_for_upgrade_counts;
		}
		
    	case KK_REBOOT_POWER_ON:
		{
			return s_system_state.reboot_for_power_on_counts;
		}
		
		case KK_REBOOT_GPRS:
		{
			return s_system_state.reboot_for_net_counts;
		}

		case KK_REBOOT_GPS:
		{
			return s_system_state.reboot_for_gps_counts;
		}
		
		case KK_REBOOT_CHECKPARA:
		{
			return s_system_state.reboot_for_checkpara_counts;
		}
		
		default:
		{
			return s_system_state.reboot_for_other_counts;
		}
	}
}

SystemWorkState system_state_get_work_state(void)
{
	return s_system_state.work_state;
}

KK_ERRCODE system_state_set_work_state(const SystemWorkState work_state)
{
	if(s_system_state.work_state != work_state)
	{
		s_system_state.work_state = work_state;
		
		//休眠或者唤醒，全部传感器报警状态清除，电压报警和其它状态不变
		s_system_state.status_bits &=0x0007ffff;
		LOG(LEVEL_INFO,"system_state_set_work_state:%d",work_state);
		//状态发生变化，触发一次心跳
		//gps_service_heart_atonce();
		return save_state_to_file();
	}
	return KK_SUCCESS;
}

KK_ERRCODE system_state_set_vehicle_state(const VehicleState vehicle_state)
{
	if(s_system_state.vehicle_state != vehicle_state)
	{
		LOG(LEVEL_INFO, "vehicle_state from %d to %d", s_system_state.vehicle_state,vehicle_state);
		s_system_state.vehicle_state = vehicle_state;
		return save_state_to_file();
	}
	return KK_SUCCESS;
}

VehicleState system_state_get_vehicle_state(void)
{
	return s_system_state.vehicle_state;
}

static KK_ERRCODE read_state_from_file()
{
    return KK_SUCCESS;
}

static KK_ERRCODE save_state_to_file(void)
{
    return KK_SUCCESS;
}

bool system_state_has_reported_gps_since_boot(void)
{
	return GET_BIT1(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_has_reported_gps_since_boot(bool state)
{
	if (state == system_state_has_reported_gps_since_boot())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT1(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT1(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_has_reported_gps_since_boot");
	return save_state_to_file();
}

bool system_state_has_reported_static_gps(void)
{
	return GET_BIT2(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_has_reported_static_gps(bool state)
{
	if (state == system_state_has_reported_static_gps())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT2(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT2(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_has_reported_static_gps");
	return save_state_to_file();
}

bool system_state_has_reported_gps_since_modify_ip(void)
{
	return GET_BIT3(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_reported_gps_since_modify_ip(bool state)
{
	if (state == system_state_has_reported_gps_since_modify_ip())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT3(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT3(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_reported_gps_since_modify_ip");
	return save_state_to_file();
}


bool system_state_get_has_started_charge(void)
{
	return GET_BIT4(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_has_started_charge(bool state)
{
	if (state == system_state_get_has_started_charge())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();
	
	if (state)
	{
		SET_BIT4(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT4(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_has_started_charge");
	//频繁变化，不保存到文件
	return KK_SUCCESS;
}

bool system_state_get_acc_is_line_mode(void)
{
	return GET_BIT5(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_acc_is_line_mode(bool state)
{
	if (state == system_state_get_acc_is_line_mode())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT5(s_system_state.status_bits);
		LOG(LEVEL_INFO,"ACC line is valid!");
	}
	else
	{
		CLR_BIT5(s_system_state.status_bits);
		LOG(LEVEL_INFO,"ACC line is invalid!");
	}
	LOG(LEVEL_INFO,"system_state_set_acc_is_line_mode");
	return save_state_to_file();
}

bool system_state_get_device_relay_state(void)
{
	return GET_BIT6(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_device_relay_state(bool state)
{
	if (state == system_state_get_device_relay_state())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT6(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT6(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_device_relay_state");
	return save_state_to_file();
}

bool system_state_get_user_relay_state(void)
{
	return GET_BIT7(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_user_relay_state(bool state)
{
	if (state == system_state_get_user_relay_state())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();
	if (state)
	{
		SET_BIT7(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT7(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_user_relay_state");
	return save_state_to_file();
}

bool system_state_get_defence(void)
{
	return GET_BIT8(s_system_state.status_bits);
}
KK_ERRCODE system_state_set_defence(bool state)
{
	if (state == system_state_get_defence())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT8(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT8(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_defence");
	return save_state_to_file();
}

bool system_state_has_reported_lbs_since_boot(void)
{
	return GET_BIT9(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_has_reported_lbs_since_boot(bool state)
{
	if (state == system_state_has_reported_lbs_since_boot())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT9(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT9(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_has_reported_lbs_since_boot");
	return save_state_to_file();
}

bool system_state_is_cold_boot(void)
{
	return GET_BIT10(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_cold_boot(bool state)
{
	if (state == system_state_is_cold_boot())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT10(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT10(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_cold_boot");
	return save_state_to_file();
}

void system_state_set_last_gps(const GPSData *p_gps)
{
    memcpy(&s_system_state.latest_gps, p_gps, sizeof(s_system_state.latest_gps));
    LOG(LEVEL_DEBUG,"clock(%d) system_state_set_last_gps %f,%f.", util_clock(), p_gps->lng,p_gps->lat);
}

void system_state_get_last_gps(GPSData *p_gps)
{
    memcpy(p_gps, &s_system_state.latest_gps,  sizeof(s_system_state.latest_gps));
    LOG(LEVEL_DEBUG,"clock(%d) system_state_get_last_gps %f,%f.", util_clock(), p_gps->lng,p_gps->lat);
}

bool system_state_has_reported_gps_after_run(void)
{
	return GET_BIT12(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_has_reported_gps_after_run(bool state)
{
	if (state == system_state_has_reported_gps_after_run())
	{
		return KK_SUCCESS;
	}

	if (state)
	{
		SET_BIT12(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT12(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_has_reported_gps_after_run");
    
	return KK_SUCCESS;
}


U8 system_state_get_extern_battery_voltage_grade(void)
{
	return s_system_state.extern_battery_voltage_grade;
}


KK_ERRCODE system_state_set_extern_battery_voltage_grade(U8 voltage_grade)
{
	if (voltage_grade == s_system_state.extern_battery_voltage_grade)
	{
		return KK_SUCCESS;
	}
	else
	{
		s_system_state.extern_battery_voltage_grade = voltage_grade;
		LOG(LEVEL_INFO,"system_state_set_extern_battery_voltage_grade");
		return save_state_to_file();
	}
}

bool system_state_get_power_off_alarm(void)
{
	return GET_BIT16(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_power_off_alarm(bool state)
{
	if (state == system_state_get_power_off_alarm())
	{
		return KK_SUCCESS;
	}

	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();
	
	if (state)
	{
		SET_BIT16(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT16(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_power_off_alarm");
	return save_state_to_file();
}

bool system_state_get_battery_low_voltage_alarm(void)
{
	return GET_BIT17(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_battery_low_voltage_alarm(bool state)
{
	if (state == system_state_get_battery_low_voltage_alarm())
	{
		return KK_SUCCESS;
	}

	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT17(s_system_state.status_bits);
		s_system_state.battery_low_voltage_count++;
	}
	else
	{
		CLR_BIT17(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_battery_low_voltage_alarm");
	return save_state_to_file();
}

KK_ERRCODE system_state_set_high_voltage_alarm(bool state)
{
	if (state == system_state_get_high_voltage_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT19(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT19(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_high_voltage_alarm");
	return save_state_to_file();
}


bool system_state_get_shake_alarm(void)
{
	return GET_BIT20(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_shake_alarm(bool state)
{
	if (state == system_state_get_shake_alarm())
	{
		return KK_SUCCESS;
	}

	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT20(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT20(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_shake_alarm");
	return save_state_to_file();
}

bool system_state_get_overspeed_alarm(void)
{
	return GET_BIT21(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_overspeed_alarm(bool state)
{
	if (state == system_state_get_overspeed_alarm())
	{
		return KK_SUCCESS;
	}

	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT21(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT21(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_overspeed_alarm");
	return save_state_to_file();
}

bool system_state_get_fakecell_alarm(void)
{
	return GET_BIT22(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_fakecell_alarm(bool state)
{
	if (state == system_state_get_fakecell_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT22(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT22(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_fakecell_alarm");
	return save_state_to_file();
}

bool system_state_get_high_voltage_alarm(void)
{
	return GET_BIT19(s_system_state.status_bits);
}

bool system_state_get_collision_alarm(void)
{
	return GET_BIT23(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_collision_alarm(bool state)
{
	if (state == system_state_get_collision_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT23(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT23(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_collision_alarm");
	return save_state_to_file();
}

bool system_state_get_speed_up_alarm(void)
{
	return GET_BIT24(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_speed_up_alarm(bool state)
{
	if (state == system_state_get_speed_up_alarm())
	{
		return KK_SUCCESS;
	}

	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT24(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT24(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_speed_up_alarm");
	return save_state_to_file();
}

bool system_state_get_speed_down_alarm(void)
{
	return GET_BIT25(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_speed_down_alarm(bool state)
{
	if (state == system_state_get_speed_down_alarm())
	{
		return KK_SUCCESS;
	}

	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT25(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT25(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_speed_down_alarm");
	return save_state_to_file();
}

bool system_state_get_turn_over_alarm(void)
{
	return GET_BIT26(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_turn_over_alarm(bool state)
{
	if (state == system_state_get_turn_over_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT26(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT26(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_turn_over_alarm");
	return save_state_to_file();
}

bool system_state_get_sharp_turn_alarm(void)
{
	return GET_BIT27(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_sharp_turn_alarm(bool state)
{
	if (state == system_state_get_sharp_turn_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT27(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT27(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_sharp_turn_alarm");
	return save_state_to_file();
}


bool system_state_get_remove_alarm(void)
{
	return GET_BIT28(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_remove_alarm(bool state)
{
	if (state == system_state_get_remove_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT28(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT28(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_remove_alarm");
	return save_state_to_file();
}


bool system_state_get_move_alarm(void)
{
	return GET_BIT29(s_system_state.status_bits);
}

KK_ERRCODE system_state_set_move_alarm(bool state)
{
	if (state == system_state_get_move_alarm())
	{
		return KK_SUCCESS;
	}
	
	//状态发生变化，触发一次心跳
	//gps_service_heart_atonce();

	if (state)
	{
		SET_BIT29(s_system_state.status_bits);
	}
	else
	{
		CLR_BIT29(s_system_state.status_bits);
	}
	LOG(LEVEL_INFO,"system_state_set_move_alarm");
	return save_state_to_file();
}

void system_state_set_mileage(U64 mileage)
{
	if (mileage == s_system_state.mileage)
	{
		return;
	}

	s_system_state.mileage = mileage;
	if (util_clock()%(10*SECONDS_PER_MIN) == 0)
	{
		save_state_to_file();
	}
}

U64 system_state_get_mileage(void)
{
	return s_system_state.mileage;
}

void system_state_set_bin_checksum(u32 check_sum)
{
	if (check_sum == s_system_state.check_sum)
	{
		return;
	}

	else
	{
		s_system_state.check_sum = check_sum;
		LOG(LEVEL_INFO,"system_state_set_bin_checksum");
		save_state_to_file();
	}
}

u32 system_state_get_bin_checksum(void)
{
	return s_system_state.check_sum;
}
/*
void system_state_set_gsensor_type(GSensorType gsensor_type)
{
	if (gsensor_type == s_system_state.gsensor_type)
	{
		return;
	}

	else
	{
		s_system_state.gsensor_type = gsensor_type;
		LOG(INFO,"system_state_set_gsensor_type");
		save_state_to_file();
	}
}

GSensorType system_state_get_gsensor_type(void)
{
	return s_system_state.gsensor_type;
}
*/

u32 system_state_get_last_good_time(void)
{
	return s_system_state.last_good_time;
}

u32 system_state_get_call_ok_count(void)
{
	return s_system_state.call_ok_count;
}

void system_state_set_gpss_reboot_reason(const char *reason)
{
    snprintf(s_system_state.gpss_reboot_reason,sizeof(s_system_state.gpss_reboot_reason),"%s",reason);
    s_system_state.gpss_reboot_reason[sizeof(s_system_state.gpss_reboot_reason) - 1] = 0;
}

const char *system_state_get_gpss_reboot_reason(void)
{
    return s_system_state.gpss_reboot_reason;
}

void system_state_set_ip_cache(u8 index,const u8* ip)
{
	memcpy(s_system_state.ip_cache[index], ip, 4);
	save_state_to_file();
}

void system_state_get_ip_cache(u8 index,u8* ip)
{
	memcpy(ip, s_system_state.ip_cache[index], 4);
}


