
#include <stdio.h>
#include <math.h>
#include "kk_error_code.h"
#include "gps.h"
#include "applied_math.h"
#include "nmea_protocol.h"
#include "service_log.h"
#include "gpio_board_config.h"
#include "uart_board_config.h"
#include "circular_queue.h"
#include "gps_service.h"
#include "system_state.h"
#include "utility.h"
#include "config_service.h"
#include "led_service.h"
#include "command.h"
#define GPS_BUFF_LEN 20


typedef enum 
{	//未初始化
	KK_GPS_STATE_NONE = 0,
		
	//初始化OK
	KK_GPS_STATE_INITED,

	//发送了版本号查询,等待版本号响应
	KK_GPS_STATE_WAIT_VERSION,

	//发送了AGPS时间请求,等待AGPS时间响应
	KK_GPS_STATE_WAIT_APGS_TIME,
	
	//发送了AGPS数据请求,等待AGPS数据响应
	KK_GPS_STATE_WAIT_APGS_DATA,

	//工作中
	KK_GPS_STATE_WORKING,
	
}GPSInternalState;

typedef struct
{
	//使用的波特率
	U32 baud_rate;

	//GPS芯片型号
	GPSChipType gpsdev_type;

	//内部状态
	GPSInternalState internal_state;

	//整体状态
	GPSState state;

	//定位状态时间记录
    StateRecord state_record;

	//存放定位数据的环形队列
	CircularQueue gps_time_queue;
	CircularQueue gps_lat_queue;
	CircularQueue gps_lng_queue;
	CircularQueue gps_speed_queue;
	CircularQueue gps_course_queue;
	CircularQueue gps_hdop_queue;
	CircularQueue gps_satellites_queue;

	//启动时间
	time_t power_on_time;

	time_t mtk_start_time;

	time_t last_rcv_time;

	bool push_data_enbale;

	//AGPS完成注入时间
	U16 agps_time;

	//定位时间
	time_t fix_time;

	//内存中保存时间（每秒一条）
	time_t save_time;

	//上报平台时间
	time_t report_time;

	time_t sleep_time;

	//从上次上报点到当前点的距离（曲线距离）
	float distance_since_last_report;

	//是否已打开VTG语句（泰斗芯片需要打开VTG语句）
	bool has_opened_td_vtg;
	
    NMEASentenceVER frame_ver;
	NMEASentenceRMC frame_rmc;
	NMEASentenceGGA frame_gga;
	NMEASentenceGSA frame_gsa;
	//NMEASentenceGST frame_gst;
	NMEASentenceGSV frame_gsv;
	//NMEASentenceVTG frame_vtg;
	//NMEASentenceZDA frame_zda;
	//NMEASentenceGLL frame_gll;

	//辅助定位经纬度
	float ref_lng;
	float ref_lat;

	//定位状态
	//GPSState的第0位表示是否定位定位,0——未定位；1——已定位
	bool is_fix;
	
	//GPSState的第1位表示2D/3D定位,0——2D；1——3D
	bool is_3d;

	//GPSState的第2位表示是否差分定位定位,0——非差分定位；1——差分定位
	bool is_diff;

	//信号强度等级（0-100,100最高）
	U8 signal_intensity_grade;

	//水平方向定位精度[0.5,99.9]
	float hdop;

    //可见卫星数
	U8 satellites_tracked;

	//天线高度（相对于平均海平面,单位米）
    float altitude; 

	//最大信噪比(1分钟更新一次)
	U8 max_snr;

	//信号最好的5颗星
	SNRInfo snr_array[SNR_INFO_NUM];

	//可见卫星数(1秒更新一次)
	U8 satellites_inview;

	//snr大于35的卫星数,即我们认为可用的卫星数(1秒更新一次)
	U8 satellites_good;

	float aclr_avg_calculate_by_speed;
	U32 constant_speed_time;
	U32 static_speed_time;
	StateRecord over_speed_alarm_state;
	
    u8 gps_recovery_flg;

    //GPSPowerManagerStep gpm_step;

    u8 gpm_send_flg;

    UartOptionStruct *seril;
}Gps,*PGps;

static Gps s_gps;

s32 s_gps_rx_sem = -1;

typedef struct
{
    u8  flag;

    u16 step;

    u32 start_time;
        
}GpsInjectStruct;

GpsInjectStruct s_inject;

	
//GPS串口超过多少秒没有接收到数据关闭重新打开
#define KK_REOPEN_GPS_PORT_TIME 30

static void clear_data(void);

static void check_has_received_data(void *ptmr, void *parg);

static void on_received_gga(const NMEASentenceGGA gga);

static void on_received_gsa(const NMEASentenceGSA gsa);

static void on_received_fixed_state(const NMEAGSAFixType fix_type);

static on_received_gsv(const NMEASentenceGSV gsv);

static void on_received_rmc(const NMEASentenceRMC rmc);

static bool is_turn_point(const GPSData current_gps_data);

static GpsDataModeEnum upload_mode(const GPSData current_gps_data);

static void upload_gps_data(const GPSData current_gps_data);

static void check_fix_state(void *ptmr, void *parg);

static void check_fix_state_change(void);

static void calc_alcr_by_speed(GPSData gps_info);

static void check_over_speed_alarm(float speed);
static void hd_send_sentence(HDClassIDEnum id,u8 *data,u16 len);
//static void reopen_gps(void);


void gps_rx_sem_release(void)
{
    kk_os_semaphore_release(s_gps_rx_sem);
}

void gps_rx_sem_request(void)
{
    kk_os_semaphore_request(s_gps_rx_sem,0);
}

KK_ERRCODE gps_create(void)
{
	clear_data();
	s_gps.sleep_time = 0;
	circular_queue_create(&s_gps.gps_time_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_INT);
	circular_queue_create(&s_gps.gps_lat_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_FLOAT);
	circular_queue_create(&s_gps.gps_lng_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_FLOAT);
	circular_queue_create(&s_gps.gps_speed_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_FLOAT);
	circular_queue_create(&s_gps.gps_course_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_FLOAT);
	circular_queue_create(&s_gps.gps_hdop_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_FLOAT);
	circular_queue_create(&s_gps.gps_satellites_queue, GPS_BUFF_LEN, KK_QUEUE_TYPE_INT);
    if(s_gps_rx_sem < 0)s_gps_rx_sem  = kk_os_semaphore_create();
    if(!s_gps.seril)
    {
        s_gps.seril = uart_board_init("uart2",DMA_NUL,1600);
        s_gps.seril->read_cb = gps_rx_sem_release;
    }

	return KK_SUCCESS;
}

KK_ERRCODE gps_destroy(void)
{
	circular_queue_destroy(&s_gps.gps_time_queue, KK_QUEUE_TYPE_INT);
	circular_queue_destroy(&s_gps.gps_lat_queue, KK_QUEUE_TYPE_FLOAT);
	circular_queue_destroy(&s_gps.gps_lng_queue, KK_QUEUE_TYPE_FLOAT);
	circular_queue_destroy(&s_gps.gps_speed_queue, KK_QUEUE_TYPE_FLOAT);
	circular_queue_destroy(&s_gps.gps_course_queue, KK_QUEUE_TYPE_FLOAT);
	circular_queue_destroy(&s_gps.gps_hdop_queue, KK_QUEUE_TYPE_FLOAT);
	circular_queue_destroy(&s_gps.gps_satellites_queue, KK_QUEUE_TYPE_INT);
	return KK_SUCCESS;
}



static void clear_data(void)
{
	s_gps.gpsdev_type = KK_GPS_TYPE_UNKNOWN;
	s_gps.baud_rate = 115200;
	s_gps.internal_state = KK_GPS_STATE_INITED;
	s_gps.agps_time = 0;
	s_gps.state = KK_GPS_OFF;
	s_gps.state_record.state = false;
	s_gps.state_record.true_state_hold_seconds = 0;
	s_gps.state_record.false_state_hold_seconds = 0;
	s_gps.power_on_time = 0;
	s_gps.mtk_start_time = 0;
    s_gps.last_rcv_time = 0;
	s_gps.push_data_enbale = false;
	s_gps.fix_time = 0;
	s_gps.save_time = 0;
	s_gps.report_time = 0;
	s_gps.distance_since_last_report = 0;
	s_gps.has_opened_td_vtg = false;
	s_gps.ref_lng = 0;
	s_gps.ref_lat = 0;
	s_gps.is_fix = false;
	s_gps.is_3d = false;
	s_gps.is_diff = false;
	s_gps.signal_intensity_grade = 0;
	s_gps.hdop = 99.9;
	s_gps.satellites_tracked = 0;
	s_gps.max_snr = 0;
	memset(s_gps.snr_array, 0, sizeof(s_gps.snr_array));
	s_gps.satellites_inview = 0;
	s_gps.satellites_good = 0;
	s_gps.altitude = 0;
	s_gps.aclr_avg_calculate_by_speed = 0;
	s_gps.constant_speed_time = 0;
	s_gps.static_speed_time = 0;
	s_gps.static_speed_time = 0;
	s_gps.over_speed_alarm_state.state = system_state_get_overspeed_alarm();
	s_gps.over_speed_alarm_state.true_state_hold_seconds = 0;
	s_gps.over_speed_alarm_state.false_state_hold_seconds = 0;
	
	memset(&s_gps.frame_ver, 0, sizeof(NMEASentenceVER));
	memset(&s_gps.frame_rmc, 0, sizeof(NMEASentenceRMC));
	memset(&s_gps.frame_gga, 0, sizeof(NMEASentenceGGA));
	//memset(&s_gps.frame_gst, 0, sizeof(NMEASentenceGST));
	memset(&s_gps.frame_gsv, 0, sizeof(NMEASentenceGSV));
	//memset(&s_gps.frame_vtg, 0, sizeof(NMEASentenceVTG));
	//memset(&s_gps.frame_zda, 0, sizeof(NMEASentenceZDA));
	
}

enum
{
    GPS_POWER_OFF = 0,
    GPS_POWER_ON = 1,
};

KK_ERRCODE hard_ware_open_gps(void)
{
    #if 1
    //GPS_EN
    gpio_type_set(PC7,GPIO_Mode_Out_PP);
    gpio_out(PC7,GPS_POWER_ON);

    //PRSTX
    gpio_type_set(PC6,GPIO_Mode_Out_PP);
    gpio_out(PC6,1);

    //PRTRG
    gpio_type_set(PB15,GPIO_Mode_IN_FLOATING);
    //gpio_out(PB15,0);
    #endif
    return KK_SUCCESS;
}

KK_ERRCODE hard_ware_restart_gps(void)
{
    gpio_type_set(PC6,GPIO_Mode_Out_PP);
    gpio_out(PC6,0);
    kk_os_sleep(100);
    gpio_out(PC6,1);

    return KK_SUCCESS;
}

void hard_ware_close_gps(void)
{
    //return;
    gpio_type_set(PC7,GPIO_Mode_Out_PP);
    gpio_out(PC7,GPS_POWER_OFF);
}

KK_ERRCODE gps_power_on(bool push_data_enbale)
{
	//KK_ERRCODE ret = KK_SUCCESS;
	bool gps_close = false;
	
	if (gps_close)
	{
		return KK_SUCCESS;
	}

	
	if (KK_GPS_OFF != s_gps.state)
	{
        hard_ware_open_gps();
		return KK_SUCCESS;
	}
	else
	{
		clear_data();
	}

	s_gps.push_data_enbale = push_data_enbale;

	s_gps.state = KK_GPS_FIX_NONE;
	led_service_gps_set(LED_SLOW_FLASH);
	s_gps.power_on_time = util_clock();
    s_gps.gps_recovery_flg = 0;
	
	s_gps.gpsdev_type =KK_GPS_TYPE_TD_AGPS;

	s_gps.internal_state = KK_GPS_STATE_WAIT_VERSION;

	kk_os_timer_stop(TIMER_GPS_CHECK_RECEIVED);
	kk_os_timer_start(TIMER_GPS_CHECK_RECEIVED, check_has_received_data, 1000*10);
    kk_os_timer_stop(TIMER_GPS_CHECK_STATE);
	kk_os_timer_start(TIMER_GPS_CHECK_STATE, check_fix_state, 1000*60);

	//gps_kalman_filter_create(1.0);
	
	return hard_ware_open_gps();
}


static void check_fix_state(void *ptmr, void *parg)
{
	JsonObject* p_log_root = NULL;
	//char snr_str[128] = {0};

	if (s_gps.state < KK_GPS_FIX_3D)
	{
		if (false == system_state_has_reported_lbs_since_boot() && false == system_state_has_reported_gps_since_boot())
		{
			gps_service_push_lbs();
		    system_state_set_has_reported_lbs_since_boot(true);
		}
		p_log_root = json_create();
		json_add_string(p_log_root, "event", "unfixed");
		json_add_int(p_log_root, "AGPS time", s_gps.agps_time);
		json_add_int(p_log_root, "satellites_in_view", s_gps.satellites_inview);
		json_add_int(p_log_root, "satellites_good", s_gps.satellites_good);
		json_add_int(p_log_root, "satellites_tracked", s_gps.satellites_tracked);

		
		json_add_int(p_log_root, "csq", ModelCsq);

		
		log_service_upload(LEVEL_INFO,p_log_root);
	}

    kk_os_timer_stop(TIMER_GPS_CHECK_STATE);
	
}
#if 0
static void reopen_gps(void)
{
	bool push_data_enbale = s_gps.push_data_enbale;
	JsonObject* p_log_root = json_create();

	LOG(LEVEL_INFO,"reopen_gps");
	
	clear_data();
	s_gps.push_data_enbale = push_data_enbale;

    hard_ware_restart_gps();
    
	s_gps.internal_state = KK_GPS_STATE_WAIT_VERSION;
	s_gps.power_on_time = util_clock();
	s_gps.state = KK_GPS_FIX_NONE;
	kk_os_timer_stop(TIMER_GPS_CHECK_RECEIVED);
	kk_os_timer_start(TIMER_GPS_CHECK_RECEIVED, check_has_received_data, 1000*10);
    kk_os_timer_stop(TIMER_GPS_CHECK_STATE);
	kk_os_timer_start(TIMER_GPS_CHECK_STATE, check_fix_state, 1000*60);
	
	
	json_add_string(p_log_root, "event", "reopen_gps");
	json_add_int(p_log_root, "AGPS time late", util_clock() - s_gps.mtk_start_time);
	log_service_upload(LEVEL_INFO,p_log_root);
	return;
}
#endif
KK_ERRCODE gps_write_agps_info(const float lng,const float lat,const U8 leap_sencond,const U32 data_start_time)
{
    return KK_SUCCESS;
}

/**
 * Function:   写入agps（epo）数据
 * Description:
 * Input:	   seg_index:分片ID；p_data:数据指针；len:数据长度
 * Output:	   无
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
KK_ERRCODE gps_write_agps_data(const U16 seg_index, const U8* p_data, const U16 len)
{
    return KK_SUCCESS;
}

KK_ERRCODE gps_power_off(void)
{
	s_gps.state = KK_GPS_OFF;
	
	kk_os_timer_stop(TIMER_GPS_CHECK_RECEIVED);
	kk_os_timer_stop(TIMER_GPS_CHECK_STATE);
	led_service_gps_set(LED_OFF);
	
	hard_ware_close_gps();
	s_gps.sleep_time = util_clock();
	
	return KK_SUCCESS;
}



GPSState gps_get_state(void)
{
	return s_gps.state;
}

bool gps_is_fixed(void)
{
	if (KK_GPS_FIX_3D > gps_get_state() || KK_GPS_OFF == gps_get_state())
	{
		return false;
	}
	else
	{
		return true;
	}
}

U16 gps_get_fix_time(void)
{
	return s_gps.fix_time;
}

U8 gps_get_max_snr(void)
{
	return s_gps.max_snr;
}

U8 gps_get_satellites_tracked(void)
{
	return s_gps.satellites_tracked;
}

U8 gps_get_satellites_inview(void)
{
	return s_gps.satellites_inview;
}

U8 gps_get_satellites_good(void)
{
	return s_gps.satellites_good;
}
const SNRInfo* gps_get_snr_array(void)
{
	return s_gps.snr_array;
}

bool gps_get_last_data(GPSData* p_data)
{
	return gps_get_last_n_senconds_data(0,p_data);
}

/**
 * Function:   获取最近n秒的GPS数据
 * Description:
 * Input:	   seconds:第几秒,从0开始
 * Output:	   p_data:指向定位数据的指针
 * Return:	   KK_SUCCESS——成功；其它错误码——失败
 * Others:	   
 */
bool gps_get_last_n_senconds_data(U8 seconds,GPSData* p_data)
{
	if (NULL == p_data)
	{
		return false;
	}
	else
	{
		if(!circular_queue_get_by_index_i(&s_gps.gps_time_queue, seconds, (S32 *)&p_data->gps_time))
		{
				return false;
		}
		if(!circular_queue_get_by_index_f(&s_gps.gps_lat_queue, seconds, &p_data->lat))
		{
				return false;
		}
		if(!circular_queue_get_by_index_f(&s_gps.gps_lng_queue, seconds, &p_data->lng))
		{
				return false;
		}
		if(!circular_queue_get_by_index_f(&s_gps.gps_speed_queue, seconds, &p_data->speed))
		{
				return false;
		}
		if(!circular_queue_get_by_index_f(&s_gps.gps_course_queue, seconds, &p_data->course))
		{
				return false;
		}
		if(!circular_queue_get_by_index_f(&s_gps.gps_hdop_queue, seconds, &p_data->hdop))
		{
				return false;
		}
		if(!circular_queue_get_by_index_i(&s_gps.gps_satellites_queue, seconds, (S32*)&p_data->satellites_tracked))
		{
				return false;
		}
		p_data->signal_intensity_grade = s_gps.signal_intensity_grade;
		
	}

    return true;
}

static u16  hd_sentence_checksum(u8 *data,u16 len)
{
    u16 i = 0;

    u8  checksum[] = {0,0};

    for(i = 0 ; i < len ; i++)
    {
        *checksum += data[i];
        *(checksum +1) += *checksum;
    }

    return *(u16 *)checksum;
}

void gps_cold_start(void)
{
    u8 data = 0x01;
    s_inject.step = HD_COLD_START;
    hd_send_sentence(HD_COLD_START,&data,1);
}

void gps_inject_start(void)
{
    s_inject.flag = 1;
}

void gps_inject_stop(void)
{
    s_inject.flag = 0;
}

u8 is_gps_inject_started(void)
{
    return s_inject.flag;
}

void gps_time_set(void)
{
    struct tm *utcNtp;
    time_t utc = (time_t)util_get_utc_time();
    unsigned char cmd[24] = {0}; /* packet head+payload */
    u8 i = 0,j = 0;

    utcNtp = util_localtime(&utc);

    cmd[j++] = 0x00; /* UTC */
    cmd[j++] = 0x00; /* reserved */
    cmd[j++] = 0x12; /* leap cnt, hd8030 not used */

    cmd[j++] = (1900 + utcNtp->tm_year)&0xFF;
    cmd[j++] = (1900 + utcNtp->tm_year)>>8&0xFF; /* year */
    cmd[j++] = 1 + utcNtp->tm_mon; /* month */
    cmd[j++] = utcNtp->tm_mday; /* day */
    cmd[j++] = utcNtp->tm_hour; /* hour */
    cmd[j++] = utcNtp->tm_min; /* minu */
    cmd[j++] = utcNtp->tm_sec; /* sec */

    cmd[j++] = 0;//uncertainty_sec & 0xFF;
    cmd[j++] = 0;//(uncertainty_sec >> 8) & 0xFF;
    for (i = 0; i < 4; i++)
    {
        cmd[j++] = (0 >> (i * 8)) & 0xFF;    
    }

    s_inject.step = HD_AID_TIME;

    hd_send_sentence(HD_AID_TIME,cmd,j);

    //LOG(LEVEL_INFO,"%d/%d/%d %d:%d:%d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
}

enum
{
    GPS_BDS_INJECT_OK,
    GPS_BDS_INJECT_FAIL,
    GPS_BDS_INJECT_COMPLETE,
};
//

char *gps_inject_log(HDClassIDEnum id)
{
    char *ptr = NULL;

    switch(id)
    {
        case HD_AID_PEPH_BDS:
            ptr = "HD_AID_PEPH_BDS";
            break;
        case HD_AID_PEPH_GPS:
            ptr = "HD_AID_PEPH_GPS";        
            break;
        case HD_AID_TIME:
            ptr = "HD_AID_TIME";
            break;
        case HD_AID_POS:
            ptr = "HD_AID_POS";
            break;
        case HD_COLD_START:
            ptr = "HD_CLOD_START";
            break;
        default:
            ptr = "UnKonw!!";
            break;
    }

    return ptr;
}

bool gps_bds_data_injection(void)
{
    static u16 send_len = 0;

    HDGpsFrameStruct *hd = NULL;

    u8 *p = NULL;

    u16 checksum ,i,len  = 0;

    p = (u8 *)((u32)agps_file_base() + send_len);

    if(send_len >= agps_file_len())
    {
        send_len = 0;

        LOG(LEVEL_INFO,"[AGPS]gps/bds inject success!");

        return GPS_BDS_INJECT_COMPLETE;
    }
    else
    {
        hd = (HDGpsFrameStruct *)p;
        
        len = hd->framelen + 8;

        //LOG(LEVEL_INFO,"HEAD %X ID %X LEN %X",hd->head,hd->classID,hd->framelen);


        checksum = hd_sentence_checksum((u8 *)&hd->classID,len - 4);

        LOG(LEVEL_INFO,"CHECKSUM1 %X CHECKSUM2 %X",checksum,MKWORD(p[len-1],p[len - 2]));
        
        //LOG_HEX(LEVEL_INFO,p,len,"inject: ");
        LOG(LEVEL_INFO,"[AGPS]Send sentence %s",gps_inject_log(hd->classID));
        
        s_inject.step = HD_AID_PEPH_BDS;
        
        for(i = 0 ; i <len  ; i++)
        {
            s_gps.seril->putc(p[i]);
        }
        
        send_len += len;
    }

    return GPS_BDS_INJECT_OK;
}

void gps_pos_set(void)
{
    int lat = ModelLat *10000000;
    int lon = ModelLong*10000000;
    int acc = 0;
    unsigned char cmd[21] = {0};
    u8 i = 0,j = 0;

    cmd[j++] = 0x01;

	/* lat : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[j++] = (lat >> (i * 8)) & 0xff;
	}

	/* long : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[j++] = (lon >> (i * 8)) & 0xff;
	}

	/* alt : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[j++] = 0;
	}

	/* accuracy : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[j++] = (acc >> (i * 8)) & 0xff;
	}

    s_inject.step = HD_AID_POS;

    hd_send_sentence(HD_AID_POS,cmd,j);
}

static void hd_send_sentence(HDClassIDEnum id,u8 *data,u16 len)
{
    u16 i = 0,j = 0;
    //HDGpsFrameStruct p;
    //u8 *d = NULL;
    //u8  checksum[] = {0,0};
    u16 checksum;
    u8 temp[100] = {0};

    LOG(LEVEL_INFO,"[AGPS]Send sentence %s",gps_inject_log(id));

    #if 0
    p.head    = 0xD9F1;
    p.classID = id;
    p.framelen= len;

    d = (u8 *)&p.classID;

    for(i = 0 ; i < 4 i++)
    {
        *checksum += d[i];
        *(checksum +1) += *checksum;
    }

    d = (u8 *)&p.head;

    for(i = 0 ; i < 6 ; i++)
    {
        s_gps.seril.putc(d[i]);
    }

    for(i = 0 ; i < len ; i++)
    {
        s_gps.seril.putc(data[i]);
    }

    s_gps.seril.putc(checksum[0]);

    s_gps.seril.putc(checksum[1]);
    #else
    temp[i++] = 0xF1;
    temp[i++] = 0xD9;
    temp[i++] = id&0xFF;
    temp[i++] = (id>>8)&0xFF;
    temp[i++] = len&0xFF;
    temp[i++] = (len>>8)&0xFF;
    memcpy(&temp[i],data,len);
    i+=len;
    checksum = hd_sentence_checksum(&temp[2],i-2);
    temp[i++]=checksum&0xFF;
    temp[i++]=(checksum>>8)&0xFF;
    //LOG_HEX(LEVEL_DEBUG,temp,i,"sentence:");
    if(!s_gps.seril)
    {
        LOG(LEVEL_FATAL,"[AGPS]seril uninit!");

        return;
    }
    for(j = 0 ; j < i  ; j++)
    {
        s_gps.seril->putc(temp[j]);
    }
    #endif
    
}

void gps_inject_retry(void *prg ,void *prg1)
{
    LOG(LEVEL_INFO,"[AGPS]Retry sentesnce %s",gps_inject_log(s_inject.step));
    switch(s_inject.step)
    {
        case HD_AID_PEPH_BDS:
        case HD_AID_PEPH_GPS:
            gps_bds_data_injection();
            break;
        case HD_AID_TIME:
            gps_pos_set();
            break;
        case HD_AID_POS:
            gps_bds_data_injection();
            break;
        case HD_COLD_START:
            gps_time_set();
            break;
        default:
            //LOG(LEVEL_WARN,"Unknow HD class ID %x",p->classID);
            break;
    }
}

//F1 D9 05 01 02 00 06 40 4E 77
static void hd_gps_senstence_prase(u8 *data,u16 len)
{
    HDGpsFrameStruct *p = (HDGpsFrameStruct *)data;

    u16 checksum;

    u16 id;


    checksum = hd_sentence_checksum((u8 *)&p->classID,p->framelen+4);

    if(checksum == MKWORD(data[len-1],data[len - 2]))
    {
        if(p->classID == HD_INJECT_RESP)
        {
            id = MKWORD(p->payload[1],p->payload[0]);
            
            LOG(LEVEL_INFO,"[AGPS]Inject resp   %s",gps_inject_log(id));

            kk_os_timer_stop(TIMER_GPS_INJECT);
            #if 1
            switch(id)
            {
                case HD_AID_PEPH_BDS:
                case HD_AID_PEPH_GPS:
                    if(GPS_BDS_INJECT_COMPLETE != gps_bds_data_injection())
                    {
                        kk_os_timer_start(TIMER_GPS_INJECT,gps_inject_retry, 1000);
                    }
                    break;
                case HD_AID_TIME:
                    gps_pos_set();
                    kk_os_timer_start(TIMER_GPS_INJECT,gps_inject_retry, 1000);
                    break;
                case HD_AID_POS:
                    gps_bds_data_injection();
                    kk_os_timer_start(TIMER_GPS_INJECT,gps_inject_retry, 1000);
                    break;
                case HD_COLD_START:
                    //gps_time_set();
                    break;
                default:
                    LOG(LEVEL_WARN,"[AGPS]Unknow HD class ID %x",p->classID);
                    break;
            }
            #else
            switch(s_inject.step)
            {
                case HD_AID_PEPH_BDS:
                case HD_AID_PEPH_GPS:
                    gps_bds_data_injection();
                    
                    break;
                case HD_AID_TIME:
                    gps_pos_set();
                    break;
                case HD_AID_POS:
                    gps_bds_data_injection();
                    kk_os_timer_stop(TIMER_GPS_INJECT);
                    kk_os_timer_start(TIMER_GPS_INJECT,gps_inject_retry, 5000);
                    break;
                case HD_COLD_START:
                    gps_time_set();
                    break;
                default:
                    LOG(LEVEL_WARN,"Unknow HD class ID %x",p->classID);
                    break;
            }
            #endif
        }
        else
        {
            LOG(LEVEL_WARN,"[AGPS]Unknow HD RESP ID %x",p->classID);
        }
        
    }
    else
    {
        //LOG(LEVEL_WARN,"[AGPS]recv sentence checksum(%x != %x)",checksum,MKWORD(data[len-1],data[len - 2]));
        LOG_HEX(LEVEL_WARN,data,len,"[AGPS]err frame:");
    }
}

static void check_has_received_data(void *ptmr, void *parg)
{
    //如果串口还没收到数据      
	//连续10分钟未收到数据重启,否则重新打开GPS和串口继续检查是否收到数据
	if((util_clock() - s_gps.last_rcv_time) >= 10*SECONDS_PER_MIN && (util_clock() - s_gps.power_on_time) > 10*SECONDS_PER_MIN)
	{
		//hard_ware_reboot(KK_REBOOT_GPS,1);
		//System Reboot
		reset_system();
		return;
	}
	else if ((util_clock() - s_gps.last_rcv_time) >= SECONDS_PER_MIN/2 || s_gps.internal_state <= KK_GPS_STATE_WAIT_VERSION)
	{
		//KK_ERRCODE ret = KK_SUCCESS;
		hard_ware_close_gps();

        kk_os_sleep(200);

        hard_ware_open_gps();
        
		led_service_gps_set(LED_SLOW_FLASH);
		s_gps.state = KK_GPS_FIX_NONE;
		s_gps.constant_speed_time = 0;
		s_gps.static_speed_time = 0;
		s_gps.aclr_avg_calculate_by_speed = 0;
		s_gps.is_fix = false;
		s_gps.is_3d = false;
		s_gps.is_diff = false;
	}

	if(KK_SYSTEM_STATE_WORK == system_state_get_work_state())
	{
		kk_os_timer_stop(TIMER_GPS_CHECK_RECEIVED);
	}

    
}

void gps_on_rcv_uart_data(char* p_data, const U16 len)
{
	//bool has_agps_data_left = false
	s_gps.last_rcv_time = util_clock();
	
	switch (nmea_sentence_id(p_data,len, false)) 
	{	
        case NMEA_SENTENCE_TXT:
		{
			if (nmea_parse_txt(&s_gps.frame_ver, p_data)) 
			{
                if(is_gps_inject_started())
                {
                    gps_inject_stop();
                    gps_bds_data_injection();
                    //hd_gps_senstence_prase("\xF1\xD9\x06\x40\x01\x00\x01\x48\x22",9);
                }
	        }
	        else
			{
	            //LOG(DEBUG,"$TXT sentence is not parsed:%s",p_data);
	        }	
		}
		break;
        
        case NMEA_SENTENCE_RMC: 
		{
    		
            if (nmea_parse_rmc(&s_gps.frame_rmc, p_data)) 
			{
                //LOG(DEBUG,"$RMC: raw coordinates,speed and course: (%d/%d,%d/%d), %d/%d,%d/%d",
                        //s_gps.frame_rmc.latitude.value, s_gps.frame_rmc.latitude.scale,
                        //s_gps.frame_rmc.longitude.value, s_gps.frame_rmc.longitude.scale,
                        //s_gps.frame_rmc.speed.value, s_gps.frame_rmc.speed.scale,
                        //s_gps.frame_rmc.course.value, s_gps.frame_rmc.course.scale);
 
				
				on_received_rmc(s_gps.frame_rmc);
            }
            else
			{
                LOG(LEVEL_WARN,"$RMC sentence is not parsed");
            }
        } 
			break;

		case NMEA_SENTENCE_GSA:
		{
    		
            if (nmea_parse_gsa(&s_gps.frame_gsa, p_data)) 
			{
                //LOG(DEBUG,"$GSA: %c,%d,%f,%f,%f", 
					//s_gps.frame_gsa.mode,
    				//s_gps.frame_gsa.fix_type,
   					//nmea_tofloat(&s_gps.frame_gsa.pdop),
   					//nmea_tofloat(&s_gps.frame_gsa.hdop),
   					//nmea_tofloat(&s_gps.frame_gsa.vdop));
				on_received_gsa(s_gps.frame_gsa);
            }
            else 
			{
                LOG(LEVEL_WARN,"$GSA sentence is not parsed");
            }
        } 
			break;
		
        case NMEA_SENTENCE_GGA: 
		{
    		
            if (nmea_parse_gga(&s_gps.frame_gga, p_data)) 
			{
                //LOG(DEBUG,"$GGA: fix quality: %d", s_gps.frame_gga.fix_quality);
				on_received_gga(s_gps.frame_gga);
            }
            else 
			{
                LOG(LEVEL_WARN,"$GGA sentence is not parsed");
            }
        } 
			break;

        
		case NMEA_SENTENCE_GSV: 
		{	
            if (nmea_parse_gsv(&s_gps.frame_gsv, p_data))
			{ 
                on_received_gsv(s_gps.frame_gsv);
            }
            else 
			{
                LOG(LEVEL_WARN,"$XXGSV sentence is not parsed");
            }
        } 
        	break;

		case NMEA_SENTENCE_BDGSV:
		{	
			U8 index_sate = 0;
            if (nmea_parse_gsv(&s_gps.frame_gsv, p_data))
            {
				LOG(LEVEL_DEBUG,"msg_number:%d,total_msgs:%d",s_gps.frame_gsv.msg_number,s_gps.frame_gsv.total_msgs);
				for (index_sate = 0; index_sate < 4; index_sate++)
				{
					LOG(LEVEL_DEBUG,"$BDGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm",
						s_gps.frame_gsv.satellites[index_sate].nr,
						s_gps.frame_gsv.satellites[index_sate].elevation,
						s_gps.frame_gsv.satellites[index_sate].azimuth,
						s_gps.frame_gsv.satellites[index_sate].snr);
	            }
            }
		}
		break;
    

        case NMEA_INVALID: 
		{
            LOG(LEVEL_WARN,"Sentence is not valid:len=%d",len);
			//LOG_HEX(p_data,len);
        } 
	    break;

        case HD_SENTENCE_ID:
        {
            hd_gps_senstence_prase(p_data,len);
        }
        break;
        default:
		{
			//LOG(DEBUG,"Unknown data:%s",p_data);
            //LOG_HEX(p_data,len);
        } 
			break;
    }
}

static void on_received_gga(const NMEASentenceGGA gga)
{
	if (s_gps.gpsdev_type != KK_GPS_TYPE_MTK_EPO)
	{
		if (gga.fix_quality == 0)
		{
			s_gps.is_fix = false;
			s_gps.is_3d = false;
			s_gps.is_diff = false;
		}
		//中科微的航迹推算为6
		else if(gga.fix_quality == 1 || gga.fix_quality == 6)
		{
			s_gps.is_fix = true;
			s_gps.is_diff = false;
		}
		else if(gga.fix_quality == 2)
		{
			s_gps.is_fix = true;
			s_gps.is_diff = true;
		}
		else
		{
			LOG(LEVEL_ERROR, "Unknown fix quality:%d", gga.fix_quality);
		}
	}
	
	s_gps.satellites_tracked = gga.satellites_tracked;
	s_gps.altitude = nmea_tofloat(&gga.altitude);
	s_gps.hdop = nmea_tofloat(&gga.hdop);
	
	check_fix_state_change();
	LOG(LEVEL_DEBUG, "fix_state=%d,tracked=%d,hdop=%f,altitude=%f", s_gps.state,s_gps.satellites_tracked,s_gps.hdop,s_gps.altitude);
}

static void on_received_gsa(const NMEASentenceGSA gsa)
{
	if (s_gps.gpsdev_type != KK_GPS_TYPE_MTK_EPO)
	{
		on_received_fixed_state((NMEAGSAFixType)gsa.fix_type);
	}
	s_gps.hdop = nmea_tofloat(&gsa.hdop);
}

static void on_received_fixed_state(const NMEAGSAFixType fix_type)
{
	if (NMEA_GPGSA_FIX_NONE == fix_type)
	{
		s_gps.is_fix = false;
		s_gps.is_3d = false;
		s_gps.is_diff = false;
	}
	else if (NMEA_GPGSA_FIX_2D == fix_type)
	{
		s_gps.is_fix = true;
		s_gps.is_3d = false;
	}
	else if (NMEA_GPGSA_FIX_3D == fix_type)
	{
		s_gps.is_fix = true;
		s_gps.is_3d = true;
	}
	else 
	{
		LOG(LEVEL_WARN, "Unknown fix_type:%d", fix_type);
	}
	check_fix_state_change();
}

static on_received_gsv(const NMEASentenceGSV gsv)
{
	U8 index_sate = 0;
	U8 index_snr = 0;
	static U8 satellites_good = 0;
	static U8 max_snr = 0;
	//信号最好的5颗星
	static SNRInfo snr_array[5] = {{0,0},{0,0},{0,0},{0,0},{0,0}};
	s_gps.satellites_inview = gsv.total_satellites;


    for (index_sate = 0; index_sate < 4; index_sate++)
    {
    	//找出最差的那颗星的位置
		U8 min_snr_index = 0;
		U8 min_snr = 0xFF;
		#if 0
		LOG(LEVEL_DEBUG,"$GPGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm\n",
			gsv.satellites[index_sate].nr,
			gsv.satellites[index_sate].elevation,
			gsv.satellites[index_sate].azimuth,
			gsv.satellites[index_sate].snr);
		#endif
		for(index_snr = 0;index_snr < SNR_INFO_NUM;index_snr++)
		{
			if (snr_array[index_snr].snr < min_snr)
			{
				min_snr = snr_array[index_snr].snr;
				min_snr_index = index_snr;
			}
		}

		//LOG(DEBUG,"min_snr:%d,min_snr_index:%d",min_snr,min_snr_index);
	
		//如果当前这颗星比之前保存的5颗中最差的一个要好，那么替换掉最差的那个
		if (gsv.satellites[index_sate].snr > min_snr)
		{
			snr_array[min_snr_index].prn = gsv.satellites[index_sate].nr;
			snr_array[min_snr_index].snr = gsv.satellites[index_sate].snr;
		}

		if (gsv.satellites[index_sate].snr > 35)
		{
			satellites_good++;
		}
		
		if (gsv.satellites[index_sate].snr > max_snr)
		{
			max_snr = gsv.satellites[index_sate].snr;
		}
    }

	//LOG(DEBUG,"msg_number:%d,total_msgs:%d",gsv.msg_number, gsv.total_msgs);
	//最后一条消息，更新可用卫星数和最大信噪比等成员变量,中间变量清零
	if (gsv.msg_number == gsv.total_msgs)
	{
		s_gps.satellites_good = satellites_good;
		satellites_good = 0;
		
		s_gps.max_snr = max_snr;
		max_snr = 0;
		//LOG(DEBUG,"$GSV: sattelites in view: %d,satellites_good: %d,max_snr %d", gsv.total_satellites,s_gps.satellites_good,s_gps.max_snr);
			
		//上个版本用的水平精度因子（和卫星分布有关）
		s_gps.signal_intensity_grade = s_gps.max_snr/10;
		if (s_gps.signal_intensity_grade > 4)
		{
			s_gps.signal_intensity_grade = 4;
		}
		memcpy(s_gps.snr_array, snr_array, sizeof(snr_array));
		memset(snr_array, 0, sizeof(snr_array));
	}
}

static void check_fix_state_change(void)
{
	KK_CHANGE_ENUM change = util_check_state_change(s_gps.is_3d,&s_gps.state_record,10,SECONDS_PER_MIN);
	
	if (KK_CHANGE_TRUE == change)
	{
		JsonObject* p_log_root = json_create();
		char snr_str[128] = {0};
		
		s_gps.internal_state = KK_GPS_STATE_WORKING;

		if(0 == s_gps.fix_time)
		{
			s_gps.fix_time = util_clock() - s_gps.power_on_time;
			json_add_string(p_log_root, "event", "fixed");
			json_add_int(p_log_root, "AGPS time", s_gps.agps_time);
			//距离上次休眠到现在超过6小时或者没有休眠过是冷启动，否则是热启动
			if((util_clock() - s_gps.sleep_time) > 6*SECONDS_PER_HOUR || 0 == s_gps.sleep_time)
			{
				json_add_int(p_log_root, "cold fix_time", s_gps.fix_time);
				LOG(LEVEL_INFO,"cold fix_time:%d",s_gps.fix_time);
			}
			else
			{
				json_add_int(p_log_root, "hot fix_time", s_gps.fix_time);
				LOG(LEVEL_INFO,"hot fix_time:%d",s_gps.fix_time);
			}
		}
		else
		{
			json_add_string(p_log_root, "event", "become to fixed");
		}
				
		json_add_int(p_log_root, "satellites_in_view", s_gps.satellites_inview);
		json_add_int(p_log_root, "satellites_good", s_gps.satellites_good);
		json_add_int(p_log_root, "satellites_tracked", s_gps.satellites_tracked);
		
		snprintf(snr_str,128,"%03d:%02d,%03d:%02d,%03d:%02d,%03d:%02d,%03d:%02d,",
			s_gps.snr_array[0].prn,s_gps.snr_array[0].snr,
			s_gps.snr_array[1].prn,s_gps.snr_array[1].snr,
			s_gps.snr_array[2].prn,s_gps.snr_array[2].snr, 
			s_gps.snr_array[3].prn,s_gps.snr_array[3].snr,
			s_gps.snr_array[4].prn,s_gps.snr_array[4].snr);

		json_add_string(p_log_root, "snr", snr_str);
		json_add_int(p_log_root, "csq", ModelCsq);
		log_service_upload(LEVEL_INFO, p_log_root);

		
		
		led_service_gps_set(LED_ON);
		s_gps.state = (GPSState)(s_gps.is_fix << 2 | (s_gps.is_3d << 1) | (s_gps.is_diff));
	}
	else if(KK_CHANGE_FALSE == change)
	{
		JsonObject* p_log_root = json_create();
		char snr_str[128] = {0};
		json_add_string(p_log_root, "event", "become to unfixed");
		json_add_int(p_log_root, "satellites_in_view", s_gps.satellites_inview);
		json_add_int(p_log_root, "satellites_good", s_gps.satellites_good);
		json_add_int(p_log_root, "satellites_tracked", s_gps.satellites_tracked);
		
		snprintf(snr_str,128,"%03d:%02d,%03d:%02d,%03d:%02d,%03d:%02d,%03d:%02d,",
			s_gps.snr_array[0].prn,s_gps.snr_array[0].snr,
			s_gps.snr_array[1].prn,s_gps.snr_array[1].snr,
			s_gps.snr_array[2].prn,s_gps.snr_array[2].snr, 
			s_gps.snr_array[3].prn,s_gps.snr_array[3].snr,
			s_gps.snr_array[4].prn,s_gps.snr_array[4].snr);

		json_add_string(p_log_root, "snr", snr_str);
		json_add_int(p_log_root, "csq", ModelCsq);
		log_service_upload(LEVEL_INFO, p_log_root);

		//由定位到不定位立即上传（进隧道、车库等）
		//gps_service_send_one_locate(GPS_MODE_TURN_POINT, false);
		
		led_service_gps_set(LED_SLOW_FLASH);
		s_gps.state = (GPSState)(s_gps.is_fix << 2 | (s_gps.is_3d << 1) | (s_gps.is_diff));
		s_gps.constant_speed_time = 0;
		s_gps.static_speed_time = 0;
		s_gps.aclr_avg_calculate_by_speed = 0;
	}
	else
	{
		
	}
	
}

//从最近10秒的历史数据中根据时间和水平精度因子找到一个最优的数据
//时间*0.1+水平精度因子作为选择的标准，最小的为最优
bool gps_find_optimal_data_by_time_and_hdop(GPSData* p_data)
{
	GPSData data;
	U8 index = 0;
	float min_select_value = INFINITY;
	U32 last_gps_time = p_data->gps_time;
	
	for(index = 0;index < CONFIG_UPLOAD_TIME_DEFAULT;index++)
	{
		if(!gps_get_last_n_senconds_data(index,&data) || (last_gps_time - data.gps_time) >= CONFIG_UPLOAD_TIME_DEFAULT)
		{
			break;
		}
		else
		{
			float select_value = index*0.1 + data.hdop;
			if(select_value < min_select_value)
			{
				*p_data = data;
				min_select_value = select_value;
				//LOG(DEBUG,"index = %d,hdop=%f,value=%f",index,data.hdop,select_value);
			}
		}
	}
	return true;
}


static void on_received_rmc(const NMEASentenceRMC rmc)
{
	GPSData gps_data = {0};
	/*U8 index = 0;
	float lat_avg = 0;
	float lng_avg = 0;
	float speed_avg = 0;*/
	time_t seconds_from_reboot = util_clock();

	//if (s_gps.gpsdev_type == KK_GPS_TYPE_MTK_EPO)
	{
		on_received_fixed_state(rmc.valid?NMEA_GPGSA_FIX_3D:NMEA_GPGSA_FIX_NONE);
	}

	if (!gps_is_fixed())
	{
		return;
	}

	if (seconds_from_reboot - s_gps.save_time >= 1)
	{
		//U8 filter_mode = 0;
		gps_data.gps_time = nmea_get_utc_time(&rmc.date,&rmc.time);
		gps_data.lat = nmea_tocoord(&rmc.latitude);
		gps_data.lng = nmea_tocoord(&rmc.longitude);
		gps_data.speed = util_mile_to_km(nmea_tofloat(&rmc.speed));
		//判断角度是否有效,如果无效取上一个点的角度
		if (0 == rmc.course.value && 0 == rmc.course.scale)
		{	
			GPSData last_gps_data = {0};
			if(gps_get_last_data(&last_gps_data))
			{
				gps_data.course = last_gps_data.course;
			}
			else
			{
				gps_data.course = 0;
			}
		}
		else
		{
			gps_data.course = nmea_tofloat(&rmc.course);
		}
		gps_data.satellites_tracked = s_gps.satellites_tracked;
		gps_data.hdop = s_gps.hdop;
		gps_data.signal_intensity_grade = s_gps.signal_intensity_grade;
		gps_data.alt = s_gps.altitude;

		//检查异常数据
		if (0 == gps_data.gps_time || false == rmc.valid || (fabs(gps_data.lat) < 0.1 &&  fabs(gps_data.lng)  < 0.1))
		{
			return;
		}
		
		calc_alcr_by_speed(gps_data);
		check_over_speed_alarm(gps_data.speed);

		
		s_gps.save_time = seconds_from_reboot;
		
		circular_queue_en_queue_i(&s_gps.gps_time_queue,gps_data.gps_time);
		circular_queue_en_queue_f(&s_gps.gps_lat_queue,gps_data.lat);
		circular_queue_en_queue_f(&s_gps.gps_lng_queue, gps_data.lng);
		circular_queue_en_queue_f(&s_gps.gps_speed_queue,gps_data.speed);
		circular_queue_en_queue_f(&s_gps.gps_course_queue,gps_data.course);
		circular_queue_en_queue_f(&s_gps.gps_hdop_queue,gps_data.hdop);
		circular_queue_en_queue_i(&s_gps.gps_satellites_queue,gps_data.satellites_tracked);
		

		upload_gps_data(gps_data);
	}
}

//每1秒调用一次
static void calc_alcr_by_speed(GPSData gps_info)
{
    float last_second_speed = 0;
    float aclr_calculate_by_speed = 0;

    if (gps_info.speed >= 20)
    {
        s_gps.constant_speed_time++;
        s_gps.static_speed_time = 0;
        if (circular_queue_get_tail_f(&(s_gps.gps_speed_queue), &last_second_speed))
        {
            aclr_calculate_by_speed = (gps_info.speed - last_second_speed) * 1000.0 / SECONDS_PER_HOUR;
            s_gps.aclr_avg_calculate_by_speed = (s_gps.aclr_avg_calculate_by_speed * (s_gps.constant_speed_time - 1) + aclr_calculate_by_speed) / (s_gps.constant_speed_time);
        }
    }
    else
    {
        s_gps.constant_speed_time = 0;
        s_gps.static_speed_time++;
        s_gps.aclr_avg_calculate_by_speed = 0;
    }

}

static void check_over_speed_alarm(float speed)
{
	bool speed_alarm_enable = true;
	U8 speed_threshold = 120;
	U8 speed_check_time = 10;
	KK_CHANGE_ENUM state_change = KK_NO_CHANGE;
//	AlarmInfo alarm_info;
	//alarm_info.type = ALARM_SPEED;
	//alarm_info.info = speed;
	
	
	if(!speed_alarm_enable)
	{
		return;
	}
	
	state_change = util_check_state_change((bool)(speed > speed_threshold), &s_gps.over_speed_alarm_state, speed_check_time, speed_check_time);
	if(KK_CHANGE_TRUE == state_change)
	{
		system_state_set_overspeed_alarm(true);
		//gps_service_push_alarm(&alarm_info);
	}
	else if(KK_CHANGE_FALSE == state_change)
	{
		system_state_set_overspeed_alarm(false);
	}
	else
	{
		
	}
}

time_t gps_get_constant_speed_time(void)
{
	return s_gps.constant_speed_time;
}

float gps_get_aclr(void)
{
	return s_gps.aclr_avg_calculate_by_speed;
}

static GpsDataModeEnum upload_mode(const GPSData current_gps_data)
{
	KK_ERRCODE err_code = KK_SUCCESS;
	U32 seconds_from_reboot = util_clock();
    U16 upload_time_interval = 0;
//	bool static_upload_enable = false;
	
	err_code = config_service_get(CFG_UPLOADTIME, TYPE_SHORT, &upload_time_interval, sizeof(upload_time_interval));
	if (KK_SUCCESS != err_code)
	{
		LOG(LEVEL_ERROR, "Failed to config_service_get(CFG_UPLOADTIME),err_code=%d", err_code);
		return GPS_MODE_NONE;
	}

	if(false == system_state_has_reported_gps_since_boot())
	{
		system_state_set_has_reported_gps_since_boot(true);
		system_state_set_reported_gps_since_modify_ip(true);
		s_gps.distance_since_last_report = 0;
		s_gps.report_time = seconds_from_reboot;

		return GPS_MODE_POWER_UP;
	}
	
	if(false == system_state_has_reported_gps_since_modify_ip())
	{
		system_state_set_reported_gps_since_modify_ip(true);
		s_gps.distance_since_last_report = 0;
		s_gps.report_time = seconds_from_reboot;

		return GPS_MODE_POWER_UP;
	}
	
    if (VEHICLE_STATE_STATIC == system_state_get_vehicle_state()  
		&& system_state_has_reported_gps_after_run())
	{
		LOG(LEVEL_INFO, "Need not upload data because of static.");
		return GPS_MODE_NONE;
	}
	else 
	{
		if ((seconds_from_reboot - s_gps.report_time) >= upload_time_interval)
		{
			s_gps.distance_since_last_report = 0;
			s_gps.report_time = seconds_from_reboot;
            //GPMFUN(step2)();
			if(VEHICLE_STATE_STATIC == system_state_get_vehicle_state())
			{
				return GPS_MODE_STATIC_POSITION;
			}
			else
			{
                
				return GPS_MODE_FIX_TIME;
			}
			
		}
		else if (is_turn_point(current_gps_data)) 
		{
			s_gps.distance_since_last_report = 0;
			s_gps.report_time = seconds_from_reboot;
            //GPMFUN(step2)();
			LOG(LEVEL_DEBUG, "GPS_MODE_TURN_POINT");
			return GPS_MODE_TURN_POINT;
		}
		else
		{
			return GPS_MODE_NONE;
		}
	}
}

static void upload_gps_data(const GPSData current_gps_data)
{
	KK_ERRCODE ret = KK_SUCCESS;
	GpsDataModeEnum mode = upload_mode(current_gps_data);

	//struct tm tm_time = {0};

	static GPSData last_report_gps = {0};

	if(false == s_gps.push_data_enbale)
	{
		LOG(LEVEL_WARN,"do not push data!");
		return;
	}
	
	if (0 == current_gps_data.gps_time)
	{
		LOG(LEVEL_WARN,"Invalid GPS data!");
		return;
	}

	if (GPS_MODE_NONE == mode)
	{
		return;
	}

	if (GPS_MODE_POWER_UP == mode)
	{	
		//tm_time = util_gmtime(current_gps_data.gps_time);
		//util_tm_to_mtktime(&tm_time,&st_time);
		set_local_time(current_gps_data.gps_time);
	    //KK_SetLocalTime(&st_time);
		LOG(LEVEL_INFO,"gps set local time:(%d)).",current_gps_data.gps_time);
	}

    system_state_set_has_reported_gps_after_run(true);

	ret = gps_service_push_gps(mode,&current_gps_data);

	if (last_report_gps.gps_time != 0)
	{
		s_gps.distance_since_last_report = applied_math_get_distance(current_gps_data.lng, current_gps_data.lat, last_report_gps.lng, last_report_gps.lat);
		system_state_set_mileage(system_state_get_mileage() + s_gps.distance_since_last_report);
	}


	last_report_gps = current_gps_data;
	
	
	if (KK_SUCCESS != ret)
	{
		LOG(LEVEL_ERROR, "Failed to gps_service_push_gps,ret=%d", ret);
		return;
	}
}

static bool is_turn_point(const GPSData current_gps_data)
{
	U16 course_change_threshhold = 20;
    float course_change = 0;
	float current_course = current_gps_data.course;
	float last_second_course = 0;
	float last_speed = 0;
	//KK_ERRCODE ret = KK_SUCCESS;
	U8 index = 0;

	
	if(!circular_queue_get_by_index_f(&s_gps.gps_course_queue, 1, &last_second_course))
	{
		return false;
	}

	course_change = applied_math_get_angle_diff(current_course,last_second_course);	
	LOG(LEVEL_DEBUG,"[%s]:current course:%f,last second course:%f,course change:%f",__FUNCTION__,current_course,last_second_course,course_change);
	
    if (current_gps_data.speed >= 80.0f)
    {
        if (course_change >= 6)
        {   
            return true;
        }
        else if (course_change >= course_change_threshhold)
        {
            return true;
        }
		else
		{
			return false;
		}
    }
    else if (current_gps_data.speed >= 40.0f)
    {
        if (course_change >= 7)
        {
            return true;
        }
        else if (course_change >= course_change_threshhold)
        {
            return true;
        }
		else
		{
			return false;
		}
    }
    else if (current_gps_data.speed >= 25.0f)
    {
        if (course_change >= 8)
        {
            return true;
        }
        else if (course_change >= course_change_threshhold)
        {
            return true;
        }
		else
		{
			return false;
		}
    }
    else if (current_gps_data.speed >= 15.0f)
    {
        if (course_change >= (course_change_threshhold - 6))
        {
            return true;
        }
		else
		{
			return false;
		}
    }
    else if ((current_gps_data.speed >= 5.0f) && course_change >= 16 && s_gps.distance_since_last_report > 20.0f)
    {
        // 连续5秒速度大于5KMH,角度大于20度,里程超过20米,上传拐点
        for (index =0; index < 5; index++)
        {
			if(!circular_queue_get_by_index_f(&s_gps.gps_speed_queue, index, &last_speed))
			{
				return false;
			}
            if (last_speed <= 5.0f)
            {
                return false;
            }
        }
		return true;
    }
	else
	{
		return false;
	}
}

u8 gps_buff[240] = {0};

const char *test_buff = "$GPGSV,4,1,15,01,11,180,21,04,47,226,25,07,18,320,37,08,79,286,23*75\r\n$GPGSV,4,2,15,09,40,274,33,11,47,177,22,23,45,236,26,27,54,028,23*79\r\n$GPGSV,4,3,15,31,07,126,20,03,,,,05,,,,10,,,*4F\r\n$GPGSV,4,4,15,39,,,36,40,,,35,41,,,35*73\r\n$GPRMC,082732.00,A,2235.446376,N,11351.035678,E,0.0,246.4,160220,2.3,W,A*23\r\n";

void gps_task(void *arg)
{
    u16 len = 240;

    u16 head;
    
    gps_create();
    
    gps_power_on(true);

    for(;;)
    {
        gps_rx_sem_request();

        //fifo_reset(s_gps.seril->rx);

        //fifo_insert(s_gps.seril->rx,(u8 *)test_buff, strlen(test_buff));

        len = 240;

        #if 1
        while(fifo_peek(s_gps.seril->rx,gps_buff,6) == KK_SUCCESS)
        {
            #if 1
            head = MKWORD(gps_buff[0],gps_buff[1]);
            
            if(head == HD_HEADER)
            {
                len = MKWORD(gps_buff[5],gps_buff[4]) + 8;

                fifo_peek(s_gps.seril->rx,gps_buff,len);
            }
            else
            #endif
            {
                len  = 240;
                
                fifo_peek_until(s_gps.seril->rx,gps_buff,&len,'\n');
            }

            if((len  > 2) && (is_nmea_log_enable()) && head != HD_HEADER)
            LOG(LEVEL_TRANS,"%s",gps_buff);

            fifo_pop_len(s_gps.seril->rx,len);

            if(len  > 2)
            gps_on_rcv_uart_data((char *)gps_buff,len);

            len  = 240;
        }
        #else
        while(fifo_peek_until(s_gps.seril->rx,gps_buff,&len,'\n') == KK_SUCCESS)
        {
            if((len  > 2) && (is_nmea_log_enable()) && head != HD_HEADER)
            LOG(LEVEL_TRANS,"%s",gps_buff);

            fifo_pop_len(s_gps.seril->rx,len);

            if(len  > 2)
            gps_on_rcv_uart_data((char *)gps_buff,len);

            len = 240;
        }
        
        #endif
        //kk_os_sleep(500);

    }
}



